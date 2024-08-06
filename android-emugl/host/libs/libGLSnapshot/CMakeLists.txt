

add_library(GLSnapshot STATIC GLSnapshot.cpp)
#add_dependencies(GLESv1_dec GLHeaders)
target_link_libraries(GLSnapshot OpenglCodecCommon OpenGLESDispatch)
