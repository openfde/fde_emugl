/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Clom Huang   huangcailong@kylinos.cn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gpu_detection.h"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pciaccess.h>
#include <syslog.h>


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>

#include <fcntl.h>
#include <sys/syslog.h>

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xf86drm.h>
#include <errno.h>
#include <unistd.h>

static int gpuVendorId = -1;
static int gpuChipId = -1;

static char *drm_construct_id_path_tag(drmDevicePtr device)
{
   char *tag = NULL;

   if (device->bustype == DRM_BUS_PCI) {
      if (asprintf(&tag, "pci-%04x_%02x_%02x_%1u",
                   device->businfo.pci->domain,
                   device->businfo.pci->bus,
                   device->businfo.pci->dev,
                   device->businfo.pci->func) < 0) {
         return NULL;
      }
   } else if (device->bustype == DRM_BUS_PLATFORM ||
              device->bustype == DRM_BUS_HOST1X) {
      char *fullname, *name, *address;

      if (device->bustype == DRM_BUS_PLATFORM)
         fullname = device->businfo.platform->fullname;
      else
         fullname = device->businfo.host1x->fullname;

      name = strrchr(fullname, '/');
      if (!name)
         name = strdup(fullname);
      else
         name = strdup(name + 1);

      address = strchr(name, '@');
      if (address) {
         *address++ = '\0';

         if (asprintf(&tag, "platform-%s_%s", address, name) < 0)
            tag = NULL;
      } else {
         if (asprintf(&tag, "platform-%s", name) < 0)
            tag = NULL;
      }

      free(name);
   }
   return tag;
}

static char *drm_get_id_path_tag_for_fd(int fd)
{
   drmDevicePtr device;
   char *tag;

   if (drmGetDevice2(fd, 0, &device) != 0)
       return NULL;

   tag = drm_construct_id_path_tag(device);
   drmFreeDevice(&device);
   return tag;
}

static bool drm_device_matches_tag(drmDevicePtr device, const char *prime_tag)
{
   char *tag = drm_construct_id_path_tag(device);
   int ret;

   if (tag == NULL)
      return false;

   ret = strcmp(tag, prime_tag);

   free(tag);
   return ret == 0;
}

static bool drm_get_pci_id_for_fd(int fd, int *vendor_id, int *chip_id)
{
   drmDevicePtr device;
   bool ret;

   if (drmGetDevice2(fd, 0, &device) == 0) {
      if (device->bustype == DRM_BUS_PCI) {
         *vendor_id = device->deviceinfo.pci->vendor_id;
         *chip_id = device->deviceinfo.pci->device_id;
         ret = true;
      }
      else {
         fprintf(stderr, "device is not located on the PCI bus\n");
         ret = false;
      }
      drmFreeDevice(&device);
   }
   else {
      fprintf(stderr, "failed to retrieve device information\n");
      ret = false;
   }

   return ret;
}

static int open_device(const char *device_name)
{
   int fd;
   fd = open(device_name, O_RDWR | O_CLOEXEC);
   if (fd == -1 && errno == EINVAL)
   {
      fd = open(device_name, O_RDWR);
      if (fd != -1)
         fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
   }
   return fd;
}

static int get_user_preferred_fd(int default_fd)
{
/* Arbitrary "maximum" value of drm devices. */
#define MAX_DRM_DEVICES 32
    const char *dri_prime = getenv("DRI_PRIME");
    char *default_tag, *prime = NULL;
    drmDevicePtr devices[MAX_DRM_DEVICES];
    int i, num_devices, fd;
    bool found = false;

    if (dri_prime)
        prime = strdup(dri_prime);

    if (prime == NULL) {
        return default_fd;
    }

    default_tag = drm_get_id_path_tag_for_fd(default_fd);
    if (default_tag == NULL)
        goto err;

    num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
    if (num_devices < 0)
        goto err;

    /* two format are supported:
     * "1": choose any other card than the card used by default.
     * id_path_tag: (for example "pci-0000_02_00_0") choose the card
     * with this id_path_tag.
     */
    if (!strcmp(prime,"1")) {
        /* Hmm... detection for 2-7 seems to be broken. Oh well ...
         * Pick the first render device that is not our own.
         */
        for (i = 0; i < num_devices; i++) {
            if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
                    !drm_device_matches_tag(devices[i], default_tag)) {
                found = true;
                break;
            }
        }
    } else {
        for (i = 0; i < num_devices; i++) {
            if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
                    drm_device_matches_tag(devices[i], prime)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        drmFreeDevices(devices, num_devices);
        goto err;
    }

    fd = open_device(devices[i]->nodes[DRM_NODE_RENDER]);
    drmFreeDevices(devices, num_devices);
    if (fd < 0)
        goto err;

    close(default_fd);


    free(default_tag);
    free(prime);
    return fd;

err:

    free(default_tag);
    free(prime);
    return default_fd;
}

static int dri3_open(xcb_connection_t *conn,
                     xcb_window_t root,
                     uint32_t provider)
{
   xcb_dri3_open_cookie_t       cookie;
   xcb_dri3_open_reply_t        *reply;
   int                          fd;

   cookie = xcb_dri3_open(conn,
                          root,
                          provider);

   reply = xcb_dri3_open_reply(conn, cookie, NULL);
   if (!reply)
      return -1;

   if (reply->nfd != 1) {
      free(reply);
      return -1;
   }

   fd = xcb_dri3_open_reply_fds(conn, reply)[0];
   free(reply);
   fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

   return fd;
}

static int xcb_open(xcb_connection_t **connection, xcb_screen_t **screen)
{
    int screen_number;
    const xcb_setup_t *setup;
    xcb_connection_t *c;
    xcb_screen_t *scrn;
    xcb_screen_iterator_t screen_iter;

    /* getting the connection */
    c = xcb_connect(NULL, &screen_number);
    if (xcb_connection_has_error(c)) {
        return -1;
    }

    /* getting the current screen */
    setup = xcb_get_setup(c);

    scrn = NULL;
    screen_iter = xcb_setup_roots_iterator(setup);
    for (; screen_iter.rem != 0; --screen_number, xcb_screen_next(&screen_iter))
    {
        if (screen_number == 0) {
            scrn = screen_iter.data;
            break;
        }
    }
    if (!scrn) {
        xcb_disconnect(c);
        return -1;
    }

    *connection = c;
    *screen = scrn;
    return 0;
}

static bool get_gpu_pci_info(int* vendor_id, int* chip_id)
{
    int ret = 0;
    int fd = -1;
    xcb_connection_t* connection = NULL;
    xcb_screen_t* screen = NULL;
    bool result = false;

    ret = xcb_open(&connection, &screen);
    if (ret < 0)
        return false;

    fd = dri3_open(connection, screen->root, 0);
    if (fd < 0) {
        return false;
    }

    fd = get_user_preferred_fd(fd);

    result = drm_get_pci_id_for_fd(fd, vendor_id, chip_id);


    close(fd);
    xcb_disconnect(connection);

    return result;
}

static GpuType get_gpu_vendor(int vendor_id)
{
    if (vendor_id == 0x0709) {
        return GP101_VGA;
    } else if (vendor_id == 0x0716) {
        return ZC716_VGA;
    } else if (vendor_id == 0x0731) {
        return JJM_VGA;
    } else if (vendor_id == 0x1002) {
        return AMD_VGA;
    } else if (vendor_id == 0x10de) {
        return NVIDIA_VGA;
    } else if (vendor_id == 0x8086) {
        return INTEL_VGA;
    } else if (vendor_id == 0x1d17) {
        return ZHAOXIN_VGA;
    }

    return UNKNOWN_VGA;
}

static GpuType getGpuVendorFromDrm()
{
    bool result = false;

    if (gpuVendorId != -1 && gpuChipId != -1) {
        return get_gpu_vendor(gpuVendorId);
    }

    result = get_gpu_pci_info(&gpuVendorId, &gpuChipId);
    if (result) {
        return get_gpu_vendor(gpuVendorId);
    }

    return UNKNOWN_VGA;
}


#define PCIC_DISPLAY    0x03
#define PCIS_DISPLAY_VGA        0x00
#define PCI_CLASS_DISPLAY_VGA           0x0300
#define PCI_CLASS_DISPLAY_DC           0x0380

bool GpuDetection::mGpuCheckCompleted = false;
GpuType GpuDetection::mGpuType = UNKNOWN_VGA;

void prinfGpuType(GpuType gpuType)
{
    switch (gpuType)
    {
    case UNKNOWN_VGA:
        syslog(LOG_DEBUG,"gpu type is UNKNOWN_VGA");
        break;
    case NVIDIA_VGA:
        syslog(LOG_DEBUG,"gpu type is NVIDIA_VGA");
        break;
    case AMD_VGA:
        syslog(LOG_DEBUG,"gpu type is AMD_VGA");
        break;
    case INTEL_VGA:
        syslog(LOG_DEBUG,"gpu type is INTEL_VGA");
        break;
    case MALI_VGA:
        syslog(LOG_DEBUG,"gpu type is MALI_VGA");
        break;
    case GP101_VGA:
        syslog(LOG_DEBUG,"gpu type is GP101_VGA");
        break;
    case ZC716_VGA:
        syslog(LOG_DEBUG,"gpu type is ZC716_VGA");
        break;
    case JJM_VGA:
        syslog(LOG_DEBUG,"gpu type is JJM_VGA");
        break;
    case VIRTUAL_VGA:
        syslog(LOG_DEBUG,"gpu type is VIRTUAL_VGA");
        break;
    case ZHAOXIN_VGA:
        syslog(LOG_DEBUG,"gpu type is ZHAOXIN_VGA");
        break;
    case OTHER_VGA:
        syslog(LOG_DEBUG,"gpu type is OTHER_VGA");
        break;
    
    default:
        break;
    }

}

static GpuType IterateVgaPciDevice(struct pci_device *dev)
{
    // It's a VGA device
    //if ((dev->device_class & 0x00ffff00) == ((PCIC_DISPLAY << 16) | (PCIS_DISPLAY_VGA << 8))) {
    if ((dev->device_class & 0x00ffff00) == PCI_CLASS_DISPLAY_VGA << 8 ||
            (dev->device_class & 0x00ffff00) == PCI_CLASS_DISPLAY_DC << 8 ) {
        const char *dev_name;
        const char *vend_name;

        vend_name = pci_device_get_vendor_name(dev);
        dev_name = pci_device_get_device_name(dev);
        if (dev_name == NULL) {
            dev_name = "Device unknown";
        }        

        char card_vendor_id[24] = {0};
        snprintf(card_vendor_id, sizeof(card_vendor_id), "0x%04x", dev->subvendor_id);
        
        // 通过vendor_id判断显卡类型
        if (dev->vendor_id == 0x0709) {
            return GP101_VGA;
        }
        else if (dev->vendor_id == 0x0716) {
            return ZC716_VGA;
        }
        else if (dev->vendor_id == 0x0731) {
            return JJM_VGA;
        }
        else if (dev->vendor_id == 0x1002) {
            return AMD_VGA;
        }
        else if (dev->vendor_id == 0x10de) {
            return NVIDIA_VGA;
        }
        else if (dev->vendor_id == 0x8086) {
            return INTEL_VGA;
        }

        // 如果通过vendor_id判断显卡类型出现问题，则继续通过vend_name和dev_name判断显卡类型
        if (vend_name != NULL) {
            //printf("vend_name: %s %s\n", vend_name, dev_name);
            
            if (strcasestr(vend_name, "nvidia")) {
                return NVIDIA_VGA;
            }
            else if (strcasestr(vend_name, "AMD")) {
                return AMD_VGA;
            }
            else if (strcasestr(vend_name, "ATI")) {
                return AMD_VGA;
            }
            else if (strcasestr(vend_name, "Intel")) {//Integrated graphics card must check as the last one
                return INTEL_VGA;
            }
        }
        else {
            //printf("dev_name: %s\n", dev_name);
            if (strcasestr(dev_name, "nvidia")) {
                return NVIDIA_VGA;
            }
            else if (strcasestr(dev_name, "AMD")) {
                return AMD_VGA;
            }
            else if (strcasestr(dev_name, "ATI")) {
                return AMD_VGA;
            }
            else if (strcasestr(dev_name, "Intel")) {//Integrated graphics card must check as the last one
                return INTEL_VGA;
            }
        }
    }

    return UNKNOWN_VGA;
}

GpuType GpuDetection::getGpuModel()
{
    if (mGpuCheckCompleted) {
        return mGpuType;
    }

    GpuType nRet = UNKNOWN_VGA;    
    
    if (isMaliGraphicCard()) {
        syslog(LOG_DEBUG,"generic check gpu type is MALI_VGA");
        nRet = MALI_VGA;
    }

    nRet = getGpuVendorFromDrm();
    if (nRet == UNKNOWN_VGA) {
        if (isJjwGraphicCard()) {
            syslog(LOG_DEBUG,"generic check gpu type is JJM_VGA");
            nRet = JJM_VGA;
        }
        if (isGP101GraphicCard()) {
            syslog(LOG_DEBUG,"generic check gpu type is GP101_VGA");
            nRet = GP101_VGA;
        }
        if (isAMDGraphicCard()) {
            syslog(LOG_DEBUG,"generic check pu type is AMD_VGA");
            nRet = AMD_VGA;
        }
        if (isNvidiaGraphicCard()) {
            syslog(LOG_DEBUG,"generic check gpu type is NVIDIA_VGA");
            nRet = NVIDIA_VGA;
        }
    }

    if(nRet == UNKNOWN_VGA)
    {
        struct pci_device_iterator *iter;
        struct pci_device *dev;
        int ret = pci_system_init();
        if (ret != 0) {
            printf("Couldn't initialize PCI system\n");
        }
        else {
            iter = pci_slot_match_iterator_create(NULL);

            while ((dev = pci_device_next(iter)) != NULL) {
                nRet = IterateVgaPciDevice(dev);
                if (nRet > 0) {
                    break;
                }
            }

            pci_system_cleanup();
        }
    }
    
    mGpuCheckCompleted = true;
    mGpuType = nRet;
    prinfGpuType(nRet);
    return nRet;
}

static bool fbMatched(const char* fbName)
{
    FILE* fp = NULL;
    char line[256] = {0};
    bool matched = false;

    fp = fopen("/proc/fb", "r");
    if(!fp)
    {
        fprintf(stderr, "GPU detection open /proc/fb error!");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strcasestr(line, fbName)) {
            matched = true;
            break;
        }
    }

    fclose(fp);

    return matched;
}

static bool isGpuFileExist(const char* pathname)
{
    if(!access(pathname, F_OK))
    {
        return true;
    }
    return false;
}

bool GpuDetection::isJjwGraphicCard()
{
    return fbMatched("MWV206");    
}

bool GpuDetection::isGP101GraphicCard()
{
    return (fbMatched("CSMDRMFB") || fbMatched("csmicrodrmfb"));
}

bool GpuDetection::isAMDGraphicCard()
{
    return (fbMatched("amdgpudrmfb") || fbMatched("radeondrmfb"));
}

bool GpuDetection::isNvidiaGraphicCard()
{    
    return (isGpuFileExist("/dev/nvidiactl") || isGpuFileExist("/dev/nvidia0"));
}

bool GpuDetection::isMaliGraphicCard()
{
    return isGpuFileExist("/dev/mali0");
}