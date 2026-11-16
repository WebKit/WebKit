# ExecServer

set(XSCORE_SRCS
	xsDefs.cpp
	xsDefs.hpp
	xsExecutionServer.cpp
	xsExecutionServer.hpp
	xsPosixFileReader.cpp
	xsPosixFileReader.hpp
	xsPosixTestProcess.cpp
	xsPosixTestProcess.hpp
	xsProtocol.cpp
	xsProtocol.hpp
	xsTcpServer.cpp
	xsTcpServer.hpp
	xsTestDriver.cpp
	xsTestDriver.hpp
	xsTestProcess.cpp
	xsTestProcess.hpp
	)

set(XSCORE_LIBS
	decpp
	deutil
	dethread
	debase
	)

if (DE_OS_IS_WIN32)
	set(XSCORE_SRCS
		${XSCORE_SRCS}
		xsWin32TestProcess.cpp
		xsWin32TestProcess.hpp)
endif ()

add_library(xscore STATIC ${XSCORE_SRCS})
target_link_libraries(xscore ${XSCORE_LIBS})

include_directories(.)

if (DE_OS_IS_WIN32 OR DE_OS_IS_OSX OR DE_OS_IS_UNIX)
	# Build standalone execserver binary
	add_executable(execserver tools/xsMain.cpp)
	target_link_libraries(execserver xscore)

	# Tests
	add_executable(execserver-test tools/xsTest.cpp)
	target_link_libraries(execserver-test xscore)
	add_dependencies(execserver-test execserver)

	# Basic client
	add_executable(execserver-client tools/xsClient.cpp)
	target_link_libraries(execserver-client xscore)
endif ()
