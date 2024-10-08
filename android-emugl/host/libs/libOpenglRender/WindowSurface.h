/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef _LIBRENDER_WINDOW_SURFACE_H
#define _LIBRENDER_WINDOW_SURFACE_H

#include "ColorBuffer.h"
#include "RenderContext.h"

#include "emugl/common/smart_ptr.h"

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <set>

// A class used to model a guest-side window surface. The implementation
// uses a host Pbuffer to act as the EGL rendering surface instead.
class WindowSurface {
public:
    // Create a new WindowSurface instance.
    // |display| is the host EGLDisplay value.
    // |config| is the host EGLConfig value.
    // |width| and |height| are the initial size of the Pbuffer.
    // Return a new WindowSurface instance on success, or NULL on failure.
    static WindowSurface* create(EGLDisplay display,
                                 EGLConfig config,
                                 int width,
                                 int height,
                                 HandleType hndl);

    // Destructor.
    ~WindowSurface();

    // Retrieve the host EGLSurface of the WindowSurface's Pbuffer.
    EGLSurface getEGLSurface() const { return mSurface; }

    // Attach a ColorBuffer to this WindowSurface.
    // Once attached, calling flushColorBuffer() will copy the Pbuffer's
    // pixels to the color buffer.
    //
    // IMPORTANT: This automatically resizes the Pbuffer's to the ColorBuffer's
    // dimensions. Potentially losing pixel values in the process.
    void setColorBuffer(ColorBufferPtr p_colorBuffer);

    // Retrieves a pointer to the attached color buffer.
    ColorBuffer* getAttachedColorBuffer() const {
        return mAttachedColorBuffer.get();
    }

    // Copy the Pbuffer's pixels to the attached color buffer.
    // Returns true on success, or false on error (e.g. if there is no
    // attached color buffer).
    bool flushColorBuffer();

    // Used by bind() below.
    enum BindType {
        BIND_READ,
        BIND_DRAW,
        BIND_READDRAW
    };

    // TODO(digit): What is this used for exactly? For example, the
    // mReadContext is never used by this class. The mDrawContext is only
    // used temporarily during flushColorBuffer() operation, and could be
    // passed as a parameter to the function instead. Maybe this is only used
    // to increment reference counts on the smart pointers.
    //
    // Bind a context to the WindowSurface (huh? Normally you would bind a
    // surface to the context, not the other way around)
    //
    // |p_ctx| is a RenderContext pointer.
    // |p_bindType| is the type of bind. For BIND_READ, this assigns |p_ctx|
    // to mReadContext, for BIND_DRAW, it assigns it to mDrawContext, and for
    // for BIND_READDRAW, it assigns it to both.
    void bind(RenderContextPtr p_ctx, BindType p_bindType);

    GLuint getWidth() const;
    GLuint getHeight() const;

    void onSave(android::base::Stream* stream) const;
    static WindowSurface *onLoad(android::base::Stream* stream,
            EGLDisplay display);
    HandleType getHndl() const;
    void ownColorBuffer(HandleType cb){mColorBuffers.emplace(cb);}
    std::set<HandleType> getOwnedColorBuffers(){return mColorBuffers;}
    
private:
    WindowSurface(const WindowSurface& other) = delete;

    WindowSurface(EGLDisplay display, EGLConfig config, HandleType hndl);

    bool resize(unsigned int p_width, unsigned int p_height);

private:
    EGLSurface mSurface = EGL_NO_SURFACE;
    ColorBufferPtr mAttachedColorBuffer;
    RenderContextPtr mReadContext;
    RenderContextPtr mDrawContext;
    GLuint mWidth = 0;
    GLuint mHeight = 0;
    EGLConfig mConfig = nullptr;
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    HandleType mHndl;
    std::set<HandleType> mColorBuffers;
};

typedef emugl::SmartPtr<WindowSurface> WindowSurfacePtr;

#endif  // _LIBRENDER_WINDOW_SURFACE_H
