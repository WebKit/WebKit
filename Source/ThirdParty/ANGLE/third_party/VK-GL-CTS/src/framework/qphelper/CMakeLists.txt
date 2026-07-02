# CMakeFile for qphelper library.

# PNG support enabled by default.
add_definitions(-DQP_SUPPORT_PNG)

set(QPHELPER_SRCS
	qpCrashHandler.c
	qpCrashHandler.h
	qpDebugOut.c
	qpDebugOut.h
	qpInfo.c
	qpInfo.h
	qpTestLog.c
	qpTestLog.h
	qpWatchDog.c
	qpWatchDog.h
	qpXmlWriter.c
	qpXmlWriter.h)

set(QPHELPER_LIBS
	debase
	depool
	dethread
	deutil
	${PNG_LIBRARY}
	)

if (DE_OS_IS_UNIX OR DE_OS_IS_QNX)
	# For vsnprintf()
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

if (DE_OS_IS_WIN32 AND DE_COMPILER_IS_MSC)
	set(QPHELPER_LIBS ${QPHELPER_LIBS} DbgHelp)
endif ()

add_library(qphelper STATIC ${QPHELPER_SRCS})
target_link_libraries(qphelper ${QPHELPER_LIBS})

# freebsd needs libexecinfo for backtrace symbols
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	target_link_libraries(qphelper execinfo)
endif ()

# Should qpInfo.c attempt to include qpReleaseInfo.inl
set(USE_RELEASE_INFO_FILE OFF)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qpReleaseInfo.inl")
	set(USE_RELEASE_INFO_FILE ON)

elseif (EXISTS "${PROJECT_SOURCE_DIR}/.git" AND GIT_FOUND)
	find_package(PythonInterp 2.7)

	if (PYTHON_EXECUTABLE)
		message(STATUS "Using git revision as release identifier")

		execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir=${PROJECT_SOURCE_DIR}/.git rev-parse --git-dir
						OUTPUT_VARIABLE DE_GIT_DIR
						OUTPUT_STRIP_TRAILING_WHITESPACE
						ENCODING UTF8)

		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/qpReleaseInfo.inl
						   COMMAND ${PYTHON_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/gen_release_info.py --git --git-dir=${DE_GIT_DIR} --out=${CMAKE_CURRENT_BINARY_DIR}/qpReleaseInfo.inl
						   DEPENDS gen_release_info.py ${DE_GIT_DIR}/HEAD ${DE_GIT_DIR}/index) # \note HEAD updated only when changing branches
		add_custom_target(git-rel-info DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/qpReleaseInfo.inl)
		add_dependencies(qphelper git-rel-info)
		include_directories(${CMAKE_CURRENT_BINARY_DIR})
		set(USE_RELEASE_INFO_FILE ON)
	endif ()
endif ()

if (USE_RELEASE_INFO_FILE)
	add_definitions(-DDEQP_USE_RELEASE_INFO_FILE)
endif ()
