set(SOURCES
    GLESv2Imp.cpp
    GLESv2Context.cpp
    GLESv2Validate.cpp
    SamplerData.cpp
    ShaderParser.cpp
    ShaderValidator.cpp
    TransformFeedbackData.cpp
    ProgramData.cpp
    ANGLEShaderParser.cpp)

add_library(GLES_V2_translator_static STATIC ${SOURCES})
target_link_libraries(GLES_V2_translator_static GLcommon OpenglCodecCommon android-emu-base emulator-astc-codec)

