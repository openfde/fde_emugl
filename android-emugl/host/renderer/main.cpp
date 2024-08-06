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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include <syslog.h>
#include <dlfcn.h>

#include  <X11/Xlib.h>

#include  "Create_Xwindow.h"

#include "OpenglRender/render_api_platform_types.h"

#define RENDERER_LIB_NAME "libOpenglRender.so"

typedef int (*ANDROID_INIT_ES_EMULATION)(void);


typedef int (*ANDROID_START_ES_RENDERER)(int width, int height, int* glesMajorVersion_out, int* glesMinorVersion_out);


typedef int (*ANDROID_SHOW_ES_WINDOW)(int wx, int wy, int ww, int wh,
                               int fbw, int fbh, float dpr, float rotation,
                               bool deleteExisting);

typedef int (*ANDROID_HIDE_ES_WINDOW)(void);
typedef void (*ANDROID_REDRAW_ES_WINDOW)(void);
typedef void (*ANDROID_STOP_ES_RENDERER)(bool wait);

ANDROID_INIT_ES_EMULATION android_init_es_Emulation = NULL;

ANDROID_START_ES_RENDERER android_start_es_Renderer = NULL;

ANDROID_HIDE_ES_WINDOW android_hide_es_Window = NULL;

ANDROID_SHOW_ES_WINDOW android_show_es_Window = NULL;

ANDROID_REDRAW_ES_WINDOW android_redraw_es_Window = NULL;

ANDROID_STOP_ES_RENDERER android_stop_es_Renderer = NULL;

static void *handle = NULL;

int GetOpenGLES3RenderFunc()
{

    syslog(LOG_DEBUG,"dlopen start");
    // open so
    if(handle ==NULL)
    {
        handle = dlopen(RENDERER_LIB_NAME, RTLD_LAZY);
        if (handle == NULL)
        {
            fprintf(stderr, "%s\n", dlerror());
            //syslog(LOG_DEBUG, "dl handle null");
            syslog(LOG_DEBUG,"dl handle null, dlerror = %s",dlerror());
            return -1;
        }
    }

    // get function
    syslog(LOG_DEBUG,"dlsym start");

    android_init_es_Emulation = ( ANDROID_INIT_ES_EMULATION ) dlsym(handle, "android_initOpenglesEmulation");

    android_start_es_Renderer = ( ANDROID_START_ES_RENDERER ) dlsym(handle, "android_startOpenglesRenderer");    

    android_hide_es_Window = ( ANDROID_HIDE_ES_WINDOW ) dlsym(handle, "android_hideOpenglesWindow");

    android_show_es_Window = ( ANDROID_SHOW_ES_WINDOW ) dlsym(handle, "android_showOpenglesWindow");

    android_redraw_es_Window = ( ANDROID_REDRAW_ES_WINDOW )dlsym(handle, "android_redrawOpenglesWindow");
      
    android_stop_es_Renderer = ( ANDROID_STOP_ES_RENDERER )dlsym(handle, "android_stopOpenglesRenderer");
    
    if ( android_init_es_Emulation == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_init_es_Emulation ",dlerror());

    }
    if ( android_start_es_Renderer == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_start_es_Renderer ",dlerror());

    }
    if ( android_stop_es_Renderer == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_stop_es_Renderer ",dlerror());

    }
    if ( android_show_es_Window == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_show_es_Window ",dlerror());

    }
    if ( android_hide_es_Window == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_hide_es_Window ",dlerror());

    }
    if ( android_redraw_es_Window == NULL)
    {
        syslog(LOG_DEBUG,"dlsym failed! error =%s, android_redraw_es_Window ",dlerror());

    }    

    if ( android_init_es_Emulation == NULL || android_start_es_Renderer == NULL
         || android_hide_es_Window == NULL || android_show_es_Window == NULL
            || android_redraw_es_Window == NULL)
    {
          //fprintf(stderr, "%s\n", dlerror());
          syslog(LOG_DEBUG,"dlsym failed! error = %s ",dlerror());
          dlclose(handle);
          handle = NULL;
          return -2;
    }
    return 0;

}

/*static void printUsage(const char *progName)
{
    fprintf(stderr, "Usage: %s -windowid <windowid> [options]\n", progName);
    fprintf(stderr, "    -windowid <windowid>   - window id to render into\n");
    fprintf(stderr, "    -port <portNum>        - listening TCP port number\n");
    fprintf(stderr, "    -x <num>               - render subwindow x position\n");
    fprintf(stderr, "    -y <num>               - render subwindow y position\n");
    fprintf(stderr, "    -width <num>           - render subwindow width\n");
    fprintf(stderr, "    -height <num>          - render subwindow height\n");
    exit(-1);
}
*/
int main(int argc, char *argv[])
{
    //int portNum = 0 ;//CODEC_SERVER_PORT;
    int winX = 0;
    int winY = 0;
    //int winWidth = 480;
    //int winHeight = 800;
    int winWidth = 540;
    int winHeight = 960;
    FBNativeWindowType windowId = 0;
    int iWindowId  = 0;    
    //iWindowId = atoi(argv[1]);
    
    //iWindowId =  WinCreate("render_test", winWidth, winHeight);
    
    //iWindowId = DefaultRootWindow(x_display);
    if(iWindowId <= 0)
    {
        fprintf(stderr, "WinCreate error!");
        //return -1;
    }
    
    //
    // Parse command line arguments
    //
    /*
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "-windowid")) {
            if (++i >= argc || sscanf(argv[i],"%d", &iWindowId) != 1) {
                printUsage(argv[0]);
            }
        }
        else if (!strncmp(argv[i], "-port", 5)) {
            if (++i >= argc || sscanf(argv[i],"%d", &portNum) != 1) {
                printUsage(argv[0]);
            }
        }
        else if (!strncmp(argv[i], "-x", 2)) {
            if (++i >= argc || sscanf(argv[i],"%d", &winX) != 1) {
                printUsage(argv[0]);
            }
        }
        else if (!strncmp(argv[i], "-y", 2)) {
            if (++i >= argc || sscanf(argv[i],"%d", &winY) != 1) {
                printUsage(argv[0]);
            }
        }
        else if (!strncmp(argv[i], "-width", 6)) {
            if (++i >= argc || sscanf(argv[i],"%d", &winWidth) != 1) {
                printUsage(argv[0]);
            }
        }
        else if (!strncmp(argv[i], "-height", 7)) {
            if (++i >= argc || sscanf(argv[i],"%d", &winHeight) != 1) {
                printUsage(argv[0]);
            }
        }
    }
    */
    windowId = (FBNativeWindowType)iWindowId;
    printf("window id: %d\n", windowId);
    if (!windowId) {
        // window id must be provided
        //printUsage(argv[0]);
    }
#if 0 //Enable to attach gdb to renderer on startup
    fprintf(stderr, "renderer pid %d , press any key to continue...\n", getpid());
    getchar();
#else
    fprintf(stderr, "renderer pid %d \n", getpid());
#endif
    // some OpenGL implementations may call X functions
    // it is safer to synchronize all X calls made by all the
    // rendering threads. (although the calls we do are locked
    // in the FrameBuffer singleton object).
    XInitThreads();
    //
    // initialize Framebuffer
    //    

    /* Resolve the functions */
    if (GetOpenGLES3RenderFunc() < 0) {
            syslog(LOG_ERR, " OpenGLES emulation library mismatch. Be sure to use the correct version.");
            return -1;
    }
    if(android_init_es_Emulation == NULL)
    {
            syslog(LOG_ERR, " android_init_es_Emulation == NULL.");
            return -2;
    }
    if (android_init_es_Emulation() != 0) {
            fprintf(stderr,"Failed to initialize Opengles Emulation\n");
            return -3;
    }    
    syslog(LOG_DEBUG, " Start OpenGLES renderer.");
    if(android_start_es_Renderer == NULL)
    {
            syslog(LOG_ERR, " android_start_es_Renderer == NULL.");
            return -4;
    }
    int gles_major_version = 2;
    int gles_minor_version = 0;
    if (android_start_es_Renderer(winWidth, winHeight, &gles_major_version , &gles_minor_version) != 0) {
            fprintf(stderr,"Failed to start Opengles Renderer\n");
            return -5;
    }
    syslog(LOG_DEBUG, " gles_major_version = %d", gles_major_version );
    syslog(LOG_DEBUG, " gles_minor_version = %d", gles_minor_version);
    
    /*
    char* vendor = NULL;
    char* renderer = NULL;
    char* version = NULL;
    android_getOpenglesHardwareStrings(&vendor,
                                       &renderer,
                                       &version);

    if(vendor)
        printf("vendor: %s\n", vendor);
    if(renderer)
        printf("renderer: %s\n", renderer);
    if(version)
        printf("version: %s\n", version);
*/
    
    if(android_show_es_Window == NULL)
    {
        syslog(LOG_ERR, " android_show_es_Window == NULL.");
        return -6;
    }
       android_show_es_Window( winX, winY, winWidth, winHeight, winWidth, winHeight, 1.0, 0.0,false);
    
    int a = 0;

    while(1)
    {
        if(a % 2 == 0)
        {
                //syslog(LOG_DEBUG, " this while sleep(1) run...");
                if(android_hide_es_Window == NULL)
                {
                    syslog(LOG_ERR, "android_hide_es_Window == NULL.");
                    return -7;
                }
                //android_hide_es_Window(); 
        }           
//        else
//            android_showOpenglesWindow((void*)windowId, 0, 0, 600, 1000,
//                                   600, 1000, 1.0, 0.0);
        sleep(1);
        ++a;
    }

    return 0;
}
