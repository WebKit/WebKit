# cmake file for deimage

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(DEPOOL_SRCS
	deARGB.h
	deImage.c
	deImage.h
	deTarga.c
	)

include_directories(../debase)
add_library(deimage STATIC ${DEPOOL_SRCS})
target_link_libraries(deimage debase)
