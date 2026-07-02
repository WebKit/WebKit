include(PlatformCocoa.cmake)

add_definitions(-DBPLATFORM_IOS=1)
add_definitions(-DBPLATFORM_IOS_FAMILY=1)
add_definitions(-DBPLATFORM_COCOA=1)

add_definitions(-DPAS_PLATFORM_IOS=1)
add_definitions(-DPAS_PLATFORM_IOS_FAMILY=1)
add_definitions(-DPAS_PLATFORM_COCOA=1)

list(APPEND bmalloc_COMPILE_OPTIONS $<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-O3>)
