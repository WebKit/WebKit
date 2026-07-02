
set(DEUTIL_SRCS
	deClock.c
	deClock.h
	deCommandLine.c
	deCommandLine.h
	deDynamicLibrary.c
	deDynamicLibrary.h
	deFile.c
	deFile.h
	deProcess.h
	deSocket.c
	deSocket.h
	deTimer.c
	deTimer.h
	deTimerTest.c
	deTimerTest.h
	)

set(DEUTIL_LIBS debase dethread depool)

if (DE_OS_IS_FUCHSIA)
 # Process launching isn't currently supported on Fuchsia in this library.
 # Building executor and execserver will fail.
else ()
	set(DEUTIL_SRCS ${DEUTIL_SRCS} deProcess.c)
endif ()

include_directories(
	../debase
	../dethread
	../depool
	)

if (DE_OS_IS_UNIX)
	set(DEUTIL_LIBS ${DEUTIL_LIBS} rt dl)
endif ()

if (DE_OS_IS_ANDROID)
	# inline keyword in headers...
	set_source_files_properties(deSocket.c COMPILE_FLAGS -std=c99)
endif ()

if (DE_OS_IS_WIN32)
	set(DEUTIL_LIBS ${DEUTIL_LIBS} ws2_32)
endif ()

if (DE_OS_IS_UNIX OR DE_OS_IS_QNX)
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

add_library(deutil STATIC ${DEUTIL_SRCS})
target_link_libraries(deutil ${DEUTIL_LIBS})
