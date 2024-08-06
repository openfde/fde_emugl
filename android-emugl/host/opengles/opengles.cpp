/* Copyright (C) 2011 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#include "opengles.h"

#include "android/base/CpuUsage.h"
#include "android/base/GLObjectCounter.h"
#include "android/base/files/PathUtils.h"
#include "android/base/files/Stream.h"
#include "android/base/memory/MemoryTracker.h"
#include "android/base/system/System.h"
#include "android/crashreport/crash-handler.h"
#include "android/emulation/GoldfishDma.h"
#include "android/emulation/RefcountPipe.h"
#include "android/featurecontrol/FeatureControl.h"
//#include "android/globals.h"
#include "android/opengl/emugl_config.h"
#include "android/opengl/logger.h"
#include "android/snapshot/PathUtils.h"
#include "android/snapshot/Snapshotter.h"
#include "android/utils/bufprint.h"
#include "android/utils/debug.h"
#include "android/utils/dll.h"
#include "android/utils/path.h"
//#include "config-host.h"

#include "OpenglRender/render_api.h"
#include "OpenGLESDispatch/EGLDispatch.h"
#include "OpenGLESDispatch/GLESv2Dispatch.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "syslog.h"

#include "android/utils/gpu_detection.h"
#include "android/utils/gles_config_detection.h"

static GpuType sGpuType = UNKNOWN_VGA;
static int guestApiLevel = 30;//android 11 on kmre4

bool GLESTranslatorEnable = false;

#define D(...) do { \
    VERBOSE_PRINT(init,__VA_ARGS__); \
    syslog(LOG_DEBUG,__VA_ARGS__); \
} while(0);

#define DD(...) do { \
    VERBOSE_PRINT(gles,__VA_ARGS__); \
    syslog(LOG_DEBUG,__VA_ARGS__); \
} while(0);

#define E(fmt,...) do { \
    derror(fmt, ##__VA_ARGS__); \
    syslog(LOG_DEBUG,fmt "\n", ##__VA_ARGS__); \
} while(0);

using android::base::pj;
using android::base::System;



/* Declared in "android/globals.h" */
int  android_gles_fast_pipes = 1;

static SelectedRenderer sCurrentRenderer =
    SELECTED_RENDERER_HOST;

SelectedRenderer emuglConfig_get_current_renderer() {
    return sCurrentRenderer;
}

// Define a function that initializes the function pointers by looking up
// the symbols from the shared library.

static bool sOpenglLoggerInitialized = false;
static bool sRendererUsesSubWindow = false;
static bool sEgl2egl = false;
static bool sGLESDynamicEnable = true;
static bool sNativeTextureDecompressionEnable = true;
static emugl::RenderLibPtr sRenderLib = nullptr;
static emugl::RendererPtr sRenderer = nullptr;

static const EGLDispatch* sEgl = nullptr;
static const GLESv2Dispatch* sGlesv2 = nullptr;
static char mGlesAddress[256];

int android_initOpenglesEmulation() {
    //android_init_opengl_logger();
    sOpenglLoggerInitialized = true;
    GLESTranslatorEnable = true;

    sRenderLib = initLibrary();
    if (!sRenderLib) {
        E("OpenGLES initialization failed!");
        //crashhandler_append_message_format("OpenGLES initialization failed!");
        goto BAD_EXIT;
    }

    sEgl = (const EGLDispatch *)sRenderLib->getEGLDispatch();
    sGlesv2 = (const GLESv2Dispatch *)sRenderLib->getGLESv2Dispatch();

    return 0;

BAD_EXIT:
    E("OpenGLES emulation library could not be initialized!");
    //adynamicLibrary_close(rendererSo);
    return -1;
}

int android_startOpenglesRenderer(int width, int height, int* glesMajorVersion_out, int* glesMinorVersion_out) {
    if (!sRenderLib) {
        D("Can't start OpenGLES renderer without support libraries");
        return -1;
    }

    if (!sEgl) {
        D("Can't start OpenGLES renderer without EGL libraries");
        return -1;
    }

    if (!sGlesv2) {
        D("Can't start OpenGLES renderer without GLES libraries");
        return -1;
    }

    if (sRenderer) {
        return 0;
    }
    sGpuType = GpuDetection::getGpuModel();
    if (sGpuType == NVIDIA_VGA || sGpuType == AMD_VGA || sGpuType == INTEL_VGA) {
        setenv("EGL_PLATFORM", "surfaceless", 0);
    } else {
        setenv("EGL_PLATFORM", "drm", 0);
    }
    sEgl2egl = true;
    sRendererUsesSubWindow = false;
    sGLESDynamicEnable = true;
    sRenderLib->setAvdInfo(true, guestApiLevel);

    syslog(LOG_DEBUG,"opengles setAvdInfo guestApiLevel : %d",guestApiLevel);

    //sRenderLib->setCrashReporter(&crashhandler_die_format);
    android::featurecontrol::initialize();
    sRenderLib->setFeatureController(&android::featurecontrol::isEnabled);
    android::featurecontrol::setEnabledOverride(android::featurecontrol::Egl2egl,sEgl2egl);
    android::featurecontrol::setEnabledOverride(android::featurecontrol::GLESDynamicVersion,sGLESDynamicEnable);
    android::featurecontrol::setEnabledOverride(android::featurecontrol::HostComposition,true);
    android::featurecontrol::setEnabledOverride(android::featurecontrol::NativeTextureDecompression,sNativeTextureDecompressionEnable);

    sRenderLib->setGLObjectCounter(android::base::GLObjectCounter::get());

    sRenderer = sRenderLib->initRenderer(width, height, sRendererUsesSubWindow,
                                         sEgl2egl);

    if (!sRenderer) {
        D("Can't start OpenGLES renderer?");
        return -1;
    }

    size_t addrLen = sizeof(mGlesAddress);
    sRenderer->StartRenderServer(mGlesAddress,addrLen);
    //D("mGlesAddress = %s",mGlesAddress);
    // after initRenderer is a success, the maximum GLES API is calculated depending
    // on feature control and host GPU support. Set the obtained GLES version here.
    if (glesMajorVersion_out && glesMinorVersion_out)
        sRenderLib->getGlesVersion(glesMajorVersion_out, glesMinorVersion_out);
    return 0;
}

bool
android_asyncReadbackSupported() {
    if (sRenderer) {
        return sRenderer->asyncReadbackSupported();
    } else {
        D("tried to query async readback support "
          "before renderer initialized. Likely guest rendering");
        return false;
    }
}

void
android_setPostCallback(OnPostFunc onPost, void* onPostContext, bool useBgraReadback, uint32_t displayId)
{
    if (sRenderer) {
        sRenderer->setPostCallback(onPost, onPostContext, useBgraReadback, displayId);
    }
}

ReadPixelsFunc android_getReadPixelsFunc() {
    if (sRenderer) {
        return sRenderer->getReadPixelsCallback();
    } else {
        return nullptr;
    }
}

FlushReadPixelPipeline android_getFlushReadPixelPipeline() {
    if (sRenderer) {
        return sRenderer->getFlushReadPixelPipeline();
    } else {
        return nullptr;
    }
}


static char* strdupBaseString(const char* src) {
    const char* begin = strchr(src, '(');
    if (!begin) {
        return strdup(src);
    }

    const char* end = strrchr(begin + 1, ')');
    if (!end) {
        return strdup(src);
    }

    // src is of the form:
    // "foo (barzzzzzzzzzz)"
    //       ^            ^
    //       (b+1)        e
    //     = 5            18
    int len;
    begin += 1;
    len = end - begin;

    char* result;
    result = (char*)malloc(len + 1);
    memcpy(result, begin, len);
    result[len] = '\0';
    return result;
}

void android_getOpenglesHardwareStrings(char** vendor,
                                        char** renderer,
                                        char** version) {
    assert(vendor != NULL && renderer != NULL && version != NULL);
    assert(*vendor == NULL && *renderer == NULL && *version == NULL);
    if (!sRenderer) {
        D("Can't get OpenGL ES hardware strings when renderer not started");
        return;
    }

    const emugl::Renderer::HardwareStrings strings =
            sRenderer->getHardwareStrings();
    D("OpenGL Vendor=[%s]", strings.vendor.c_str());
    D("OpenGL Renderer=[%s]", strings.renderer.c_str());
    D("OpenGL Version=[%s]", strings.version.c_str());

    /* Special case for the default ES to GL translators: extract the strings
     * of the underlying OpenGL implementation. */
    if (strncmp(strings.vendor.c_str(), "Google", 6) == 0 &&
            strncmp(strings.renderer.c_str(), "Android Emulator OpenGL ES Translator", 37) == 0) {
        *vendor = strdupBaseString(strings.vendor.c_str());
        *renderer = strdupBaseString(strings.renderer.c_str());
        *version = strdupBaseString(strings.version.c_str());
    } else {
        *vendor = strdup(strings.vendor.c_str());
        *renderer = strdup(strings.renderer.c_str());
        *version = strdup(strings.version.c_str());
    }
}

void android_getOpenglesVersion(int* maj, int* min) {
    sRenderLib->getGlesVersion(maj, min);
    fprintf(stderr, "%s: maj min %d %d\n", __func__, *maj, *min);
}

void android_stopOpenglesRenderer(bool wait)
{
    if (sRenderer) {
        sRenderer->stop(wait);
        if (wait) {
            sRenderer.reset();
            //android_stop_opengl_logger();
        }
    }
}

void android_finishOpenglesRenderer()
{
    if (sRenderer) {
        sRenderer->finish();
    }
}

static emugl::RenderOpt sOpt;
static int sWidth, sHeight;
static int sNewWidth, sNewHeight;

int android_showOpenglesWindow(int wx,
                               int wy,
                               int ww,
                               int wh,
                               int fbw,
                               int fbh,
                               float dpr,
                               float rotation,
                               bool deleteExisting) {
    if (!sRenderer) {
        return -1;
    }

    bool success = sRenderer->showOpenGLSubwindow( wx, wy, ww, wh, fbw, fbh,
                                                  dpr, rotation, deleteExisting,false);
    sNewWidth = ww * dpr;
    sNewHeight = wh * dpr;

    return success ? 0 : -1;
}

int
android_updateWindowAttri(uint32_t display_id,
                           unsigned long win_id,
                           unsigned int p_colorbuffer,
                           int32_t width,
                           int32_t height,
                           int32_t orientation, bool needpost) {
    if (!sRenderer) {
        return -1;
    }

    bool success = sRenderer->updateWindowAttri(display_id, win_id, p_colorbuffer, width, height, orientation, needpost);

    return success ? 0 : -1;
}

int
android_deleteWindowAttri(uint32_t display_id) {
    if (!sRenderer) {
        return -1;
    }

    bool success = sRenderer->deleteWindowAttri(display_id);

    return success ? 0 : -1;
}

void
android_setOpenglesTranslation(float px, float py)
{
    if (sRenderer) {
        sRenderer->setOpenGLDisplayTranslation(px, py);
    }
}

void
android_setOpenglesScreenMask(int width, int height, const unsigned char* rgbaData)
{
    if (sRenderer) {
        sRenderer->setScreenMask(width, height, rgbaData);
    }
}

int
android_hideOpenglesWindow(void)
{
    if (!sRenderer) {
        return -1;
    }
    bool success = sRenderer->destroyOpenGLSubwindow();
    return success ? 0 : -1;
}

void
android_redrawOpenglesWindow(void)
{
    if (sRenderer) {
        sRenderer->repaintOpenGLDisplay();
    }
}

bool
android_hasGuestPostedAFrame(void)
{
    if (sRenderer) {
        return sRenderer->hasGuestPostedAFrame();
    }
    return false;
}

void
android_setSupportDynamicSize(uint32_t display_id, unsigned int p_colorbuffer,
                                   int32_t width, int32_t height, bool support)
{
  if (sRenderer) {
        sRenderer->setSupportDynamicSize(display_id, p_colorbuffer, width, height, support);
  }
}

void
android_resetGuestPostedAFrame(void)
{
    if (sRenderer) {
        sRenderer->resetGuestPostedAFrame();
    }
}

static ScreenshotFunc sScreenshotFunc = nullptr;

void android_registerScreenshotFunc(ScreenshotFunc f)
{
    sScreenshotFunc = f;
}

bool android_screenShot(const char* dirname, uint32_t displayId)
{
    if (sScreenshotFunc) {
        return sScreenshotFunc(dirname, displayId);
    }
    return false;
}

const emugl::RendererPtr& android_getOpenglesRenderer() {
    return sRenderer;
}

void android_cleanupProcGLObjects(uint64_t puid) {
    if (sRenderer) {
        sRenderer->cleanupProcGLObjects(puid);
    }
}

static void* sContext, * sRenderContext, * sSurface;
static EGLint s_gles_attr[5];

//extern void tinyepoxy_init(GLESv2Dispatch* gles, int version);

static bool prepare_epoxy(void) {
    if (!sRenderLib->getOpt(&sOpt)) {
        return false;
    }
    int major, minor;
    sRenderLib->getGlesVersion(&major, &minor);
    EGLint attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, major,
        EGL_CONTEXT_MINOR_VERSION_KHR, minor,
        EGL_NONE
    };
    sContext = sEgl->eglCreateContext(sOpt.display, sOpt.config, EGL_NO_CONTEXT,
                                      attr);
    if (sContext == nullptr) {
        return false;
    }
    sRenderContext = sEgl->eglCreateContext(sOpt.display, sOpt.config,
                                            sContext, attr);
    if (sRenderContext == nullptr) {
        return false;
    }
    static constexpr EGLint surface_attr[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    sSurface = sEgl->eglCreatePbufferSurface(sOpt.display, sOpt.config,
                                             surface_attr);
    if (sSurface == EGL_NO_SURFACE) {
        return false;
    }
    static_assert(sizeof(attr) == sizeof(s_gles_attr), "Mismatch");
    memcpy(s_gles_attr, attr, sizeof(s_gles_attr));
    //tinyepoxy_init(sGLES, major * 10 + minor);
    return true;
}

struct DisplayChangeListener;
struct QEMUGLParams;

void * android_gl_create_context(DisplayChangeListener * unuse1,
                                 QEMUGLParams* unuse2) {
    static bool ok =  prepare_epoxy();
    if (!ok) {
        return nullptr;
    }
    sEgl->eglMakeCurrent(sOpt.display, sSurface, sSurface, sContext);
    return sEgl->eglCreateContext(sOpt.display, sOpt.config, sContext, s_gles_attr);
}

void android_gl_destroy_context(DisplayChangeListener* unused, void * ctx) {
    sEgl->eglDestroyContext(sOpt.display, ctx);
}

int android_gl_make_context_current(DisplayChangeListener* unused, void * ctx) {
    return sEgl->eglMakeCurrent(sOpt.display, sSurface, sSurface, ctx);
}

static GLuint s_tex_id, s_fbo_id;
static uint32_t s_gfx_h, s_gfx_w;
static bool s_y0_top;

// ui/gtk-egl.c:gd_egl_scanout_texture as reference.
void android_gl_scanout_texture(DisplayChangeListener* unuse,
                                uint32_t backing_id,
                                bool backing_y_0_top,
                                uint32_t backing_width,
                                uint32_t backing_height,
                                uint32_t x, uint32_t y,
                                uint32_t w, uint32_t h) {
    s_tex_id = backing_id;
    s_gfx_h = h;
    s_gfx_w = w;
    s_y0_top = backing_y_0_top;
    if (sNewWidth != sWidth || sNewHeight != sHeight) {
        sRenderLib->getOpt(&sOpt);
        sWidth = sNewWidth;
        sHeight = sNewHeight;
    }
    sEgl->eglMakeCurrent(sOpt.display, sOpt.surface, sOpt.surface,
                         sRenderContext);
    if (!s_fbo_id) {
        sGlesv2->glGenFramebuffers(1, &s_fbo_id);
    }
    sGlesv2->glBindFramebuffer(GL_FRAMEBUFFER_EXT, s_fbo_id);
    sGlesv2->glViewport(0, 0, h, w);
    sGlesv2->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, backing_id, 0);
}

// ui/gtk-egl.c:gd_egl_scanout_flush as reference.
void android_gl_scanout_flush(DisplayChangeListener* unuse,
                              uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!s_fbo_id)  {
        return;
    }
    sEgl->eglMakeCurrent(sOpt.display, sOpt.surface, sOpt.surface,
                         sRenderContext);

    sGlesv2->glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo_id);
    sGlesv2->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    int y1 = s_y0_top ? 0 : s_gfx_h;
    int y2 = s_y0_top ? s_gfx_h : 0;

    sGlesv2->glViewport(0, 0, sWidth, sHeight);
    sGlesv2->glBlitFramebuffer(0, y1, s_gfx_w, y2,
                             0, 0, sWidth, sHeight,
                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
    sEgl->eglSwapBuffers(sOpt.display, sOpt.surface);
    sGlesv2->glBindFramebuffer(GL_FRAMEBUFFER_EXT, s_fbo_id);
}

struct AndroidVirtioGpuOps* android_getVirtioGpuOps() {
    if (sRenderer) {
        return sRenderer->getVirtioGpuOps();
    }
    return nullptr;
}

const void* android_getEGLDispatch() {
    return sEgl;
}

const void* android_getGLESv2Dispatch() {
    return sGlesv2;
}
