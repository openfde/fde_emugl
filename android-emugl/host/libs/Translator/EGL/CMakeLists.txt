set(SOURCES    
    EglOsApi_glx.cpp
    EglOsApi_egl.cpp
    CoreProfileConfigs_linux.cpp
    ThreadInfo.cpp
    EglImp.cpp
    EglConfig.cpp
    EglContext.cpp
    EglGlobalInfo.cpp
    EglValidate.cpp
    EglSurface.cpp
    EglWindowSurface.cpp
    EglPbufferSurface.cpp
    EglThreadInfo.cpp
    EglDisplay.cpp
    ClientAPIExts.cpp)

include_directories(BEFORE
    ${EGL_INCLUDE_DIRS}
    ${GLESv2_INCLUDE_DIRS})

add_library(EGL_translator_static STATIC ${SOURCES})
target_link_libraries(EGL_translator_static
    GLcommon
    GL
    ${EGL_LDFLAGS}
    ${EGL_LIBRARIES}
    ${GLESv2_LDFLAGS}
    ${GLESv2_LIBRARIES}
    X11
    GLES_CM_translator_static
    GLES_V2_translator_static
    android-emu-base 
    emulator-astc-codec)