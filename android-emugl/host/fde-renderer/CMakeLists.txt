
add_executable(fde-renderer main.cpp)

target_link_libraries(fde-renderer OpenglRender)

install(
  TARGETS fde-renderer
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})