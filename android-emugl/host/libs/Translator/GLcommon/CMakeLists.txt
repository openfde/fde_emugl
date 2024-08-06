set(SOURCES
     etc.cpp
     FramebufferData.cpp
     GLBackgroundLoader.cpp
     GLDispatch.cpp
     GLutils.cpp
     GLEScontext.cpp
     GLESvalidate.cpp
     GLESpointer.cpp
     GLESbuffer.cpp
     NamedObject.cpp
     ObjectData.cpp
     ObjectNameSpace.cpp
     PaletteTexture.cpp
     RangeManip.cpp
     SaveableTexture.cpp
     ScopedGLState.cpp
     ShareGroup.cpp
     TextureData.cpp
     TextureUtils.cpp)

add_library(GLcommon STATIC ${SOURCES})
target_link_libraries(GLcommon  emugl_common )
#android-emu-base emulator-astc-codec