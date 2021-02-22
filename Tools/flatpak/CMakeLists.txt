
add_library(sigaction-disabler SHARED sigaction-disabler.c)
target_link_libraries(sigaction-disabler dl)
add_definitions(-DLINUX -D_GNU_SOURCE)
