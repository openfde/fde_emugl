set(GENERATED_SOURCES
    gles1_dec.cpp
    gles1_opcodes.h
    gles1_server_context.cpp)
    
if (USE_EMUGEN)
add_custom_command(
    OUTPUT ${GENERATED_SOURCES}
    POST_BUILD
    COMMAND ${CMAKE_BINARY_DIR}/android-emugl/host/tools/emugen/emugen
            -D ${CMAKE_CURRENT_BINARY_DIR} gles1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS emugen)
endif (USE_EMUGEN)

set(SOURCES
    GLESv1Decoder.cpp)

#if ("${cmake_build_type_lower}" STREQUAL "trace")
#    set(OPENGL_DEBUG "-DOPENGL_DEBUG_PRINTOUT -DCHECK_GL_ERROR")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPENGL_DEBUG}")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPENGL_DEBUG}")
#endif()

add_library(GLESv1_dec STATIC ${SOURCES} ${GENERATED_SOURCES})
#add_dependencies(GLESv1_dec GLHeaders)
target_link_libraries(GLESv1_dec OpenglCodecCommon)
