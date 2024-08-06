# Don't treat any warnings as error as we take the source directly from
# upstream and just compile it.
cmake_minimum_required(VERSION 3.5)

set(CMAKE_C_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-std=c++17 -Wall")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")

# Ensure -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads)
if (NOT CMAKE_USE_PTHREADS_INIT)
    message(ERROR "pthread not found")
endif ()
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/../../android/astc-codec/include
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/../android-emu
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/host/libs/libGLES12Translator
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/host/libs/libGLES12Translator/common
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/host/libs/libGLES12Translator/gles
    ${CMAKE_CURRENT_SOURCE_DIR}/android-emugl/host/libs/libGLES12Translator/utils
    ${CMAKE_SOURCE_DIR}/android-emugl/../astc-codec/include
    ${CMAKE_BINARY_DIR}/android-emugl/../astc-codec/include
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/include
    ${CMAKE_BINARY_DIR}/android-emugl/angle/include
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/libShaderTranslator
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/libShaderTranslator
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/common/third_party/base
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/common/third_party/base
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/common
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/common
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/id
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/id
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/third_party/compiler
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/third_party/compiler
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/compiler/translator
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/compiler/translator
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/compiler/preprocessor
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/compiler/preprocessor
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/compiler/translator/depgraph
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/compiler/translator/depgraph
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/compiler/translator/timing
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/compiler/translator/timing
    ${CMAKE_SOURCE_DIR}/android-emugl/angle/src/libShaderTranslator
    ${CMAKE_BINARY_DIR}/android-emugl/angle/src/libShaderTranslator
    ${CMAKE_SOURCE_DIR}/android-emugl/host/include
    ${CMAKE_BINARY_DIR}/android-emugl/host/include
    ${CMAKE_SOURCE_DIR}/android-emugl/src
    ${CMAKE_SOURCE_DIR}/android-emugl/shared
    ${CMAKE_SOURCE_DIR}/android-emugl/shared/OpenglCodecCommon
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs
    ${CMAKE_SOURCE_DIR}/android-emugl/host/include/libOpenglRender
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/libOpenglRender
    ${CMAKE_BINARY_DIR}/android-emugl/host/include/OpenglRender
    ${CMAKE_SOURCE_DIR}/android-emugl/host/include/OpenglRender
    ${CMAKE_BINARY_DIR}/android-emugl/host/include/OpenGLESDispatch
    ${CMAKE_SOURCE_DIR}/android-emugl/host/include/OpenGLESDispatch
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/GLESv1_dec
    ${CMAKE_BINARY_DIR}/android-emugl/host/libs/GLESv1_dec
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/GLESv2_dec
    ${CMAKE_BINARY_DIR}/android-emugl/host/libs/GLESv2_dec
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/renderControl_dec
    ${CMAKE_BINARY_DIR}/android-emugl/host/libs/renderControl_dec
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/Translator
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/libGLSnapshot
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/libOpenglRender/vulkan
    ${CMAKE_SOURCE_DIR}/android-emugl/host/libs/Translator/include
    ${CMAKE_SOURCE_DIR}/android-emugl/../android-emu
    ${CMAKE_BINARY_DIR}/android-emugl/../android-emu
    ${MIRCLIENT_INCLUDE_DIRS}
    /usr/include/libdrm)

add_subdirectory(android-emugl)
