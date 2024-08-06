set(SOURCES
    main.cpp
    Create_Xwindow.cpp
    )

add_executable(renderer ${SOURCES})
#add_dependencies(renderer GLHeaders)
target_link_libraries(renderer    
    ${X11_X11_LIB}
    rt
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    GL)
