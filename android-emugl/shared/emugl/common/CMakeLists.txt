set(COMMON_SOURCES
    address_space_device_control_ops.cpp
    crash_reporter.cpp
    dma_device.cpp
    vm_operations.cpp
    window_operations.cpp
    feature_control.cpp
    logging.cpp
    sockets.cpp
    misc.cpp
    shared_library.cpp
    stringparsing.cpp
    sync_device.cpp)

add_library(emugl_common STATIC ${COMMON_SOURCES})
target_link_libraries(
    emugl_common
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT})
