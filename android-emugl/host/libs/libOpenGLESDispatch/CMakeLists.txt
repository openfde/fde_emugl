set(SOURCES    
    gles1_stubbed_in_translator_namespace.cpp
    gles2_stubbed_in_translator_namespace.cpp
    StaticDispatch.cpp
    OpenGLDispatchLoader.cpp
    EGLDispatch.cpp
    GLESv2Dispatch.cpp
    GLESv1Dispatch.cpp)

add_library(OpenGLESDispatch STATIC ${SOURCES})
#add_dependencies(OpenGLESDispatch GLHeaders)
target_link_libraries(OpenGLESDispatch
    emugl_common
    EGL_translator_static
    GLESv2_dec
    GLESv1_dec)
