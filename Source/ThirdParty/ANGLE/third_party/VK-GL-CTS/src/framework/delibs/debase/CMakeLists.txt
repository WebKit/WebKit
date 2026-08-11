# cmake file for debase

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(DEBASE_SRCS
	deDefs.c
	deDefs.h
	deFloat16.c
	deFloat16.h
	deFloat16Test.c
	deInt32.c
	deInt32.h
	deInt32Test.c
	deMath.c
	deMath.h
	deMathTest.c
	deMemory.c
	deMemory.h
	deRandom.c
	deRandom.h
	deString.c
	deString.h
	deSha1.c
	deSha1.h
	)

add_library(debase STATIC ${DEBASE_SRCS})

# link debase to libm on unix systems
if (DE_OS_IS_UNIX OR DE_OS_IS_QNX)
	target_link_libraries(debase m)

	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

if (DE_OS_IS_SYMBIAN)
    target_link_libraries(debase gcc)
endif()

if (DE_OS_IS_ANDROID)
	target_link_libraries(debase log)
endif()