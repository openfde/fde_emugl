# Don't treat any warnings as error as we take the source directly from
# upstream and just compile it.

add_subdirectory(host)
add_subdirectory(shared)
add_subdirectory(android-emu-base)
add_subdirectory(astc-codec)