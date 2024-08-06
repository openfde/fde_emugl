set(SOURCES
    CoreProfileEngine.cpp
     GLEScmImp.cpp
     GLEScmUtils.cpp
     GLEScmContext.cpp
     GLEScmValidate.cpp)

add_library(GLES_CM_translator_static STATIC ${SOURCES})
target_link_libraries(GLES_CM_translator_static GLcommon android-emu-base emulator-astc-codec)
