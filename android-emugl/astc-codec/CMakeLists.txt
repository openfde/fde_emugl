cmake_minimum_required(VERSION 3.5)

#set(src "../../../../astc-codec/src")
set(src "../../astc-codec/src")
#/home/kylin/qemu-git/qemu/android/astc-codec/src/decoder
#/home/kylin/qemu-git/qemu/android/third_party/astc-codec

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -std=c++17 -Wall")

add_library(emulator-astc-codec
        ${src}/decoder/astc_file.cc
        ${src}/decoder/codec.cc
        ${src}/decoder/endpoint_codec.cc
        ${src}/decoder/footprint.cc
        ${src}/decoder/integer_sequence_codec.cc
        ${src}/decoder/intermediate_astc_block.cc
        ${src}/decoder/logical_astc_block.cc
        ${src}/decoder/partition.cc
        ${src}/decoder/physical_astc_block.cc
        ${src}/decoder/quantization.cc
        ${src}/decoder/weight_infill.cc
        )

target_include_directories(emulator-astc-codec PUBLIC ${src}/../include)
target_include_directories(emulator-astc-codec PRIVATE ${src}/..)

if (WIN32 OR ("${LOCAL_OS}" STREQUAL "windows"))
    # Work around some gcc/mingw issues
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # MinGW has some additional warnings that are safe to ignore:
        #  - maybe-uninitialized in intermediate_astc_block.cc, for a std::array
        #    that is safely uninitialzed by a helper function.
        #  - unused-variable in release builds due to asserts. clang doesn't have
        #    this warning enabled, so it's safe to disable here.
        target_compile_definitions(emulator-astc-codec PRIVATE -DLIBYUV_DISABLE_X86)
        target_compile_options(emulator-astc-codec PRIVATE
                -Wno-error=unused-variable
                -Wno-error=maybe-uninitialized)
    endif ()
endif ()
