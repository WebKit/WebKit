# dethread cmake file

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(DETHREAD_SRCS
	deAtomic.c
	deAtomic.h
	deMutex.h
	deSemaphore.h
	deSingleton.c
	deSingleton.h
	deThread.h
	deThreadLocal.h
	deThreadTest.c
	deThreadTest.h
	win32/deMutexWin32.c
	win32/deSemaphoreWin32.c
	win32/deThreadWin32.c
	win32/deThreadLocalWin32.c
	unix/deMutexUnix.c
	unix/deSemaphoreMach.c
	unix/deSemaphoreUnix.c
	unix/deThreadUnix.c
	unix/deThreadLocalUnix.c
	)

set(DETHREAD_LIBS
	debase
	depool
	)

include_directories(
	../debase
	../depool
	${CMAKE_CURRENT_SOURCE_DIR}
	)

if (DE_OS_IS_UNIX)
	if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
		add_definitions(-D__BSD_VISIBLE)
	endif ()
	add_definitions(-D_GNU_SOURCE)

	set(DETHREAD_LIBS ${DETHREAD_LIBS} pthread)
endif ()

if (DE_OS_IS_UNIX OR DE_OS_IS_ANDROID OR DE_OS_IS_OSX OR DE_OS_IS_IOS OR DE_OS_IS_QNX OR DE_OS_IS_FUCHSIA)
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

add_library(dethread STATIC ${DETHREAD_SRCS})
target_link_libraries(dethread ${DETHREAD_LIBS})

set(DETHREAD_STANDALONE_TEST ON CACHE STRING "Build standalone binary for testing dethread.")

if (DETHREAD_STANDALONE_TEST AND (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX))
	add_executable(dethread_test standalone_test.c)
	target_link_libraries(dethread_test dethread debase)
endif ()
