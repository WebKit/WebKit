# Executor

# Executor depends on execserver library (xscore)
include_directories(../execserver)

set(XECORE_SRCS
	xeBatchExecutor.cpp
	xeBatchExecutor.hpp
	xeBatchResult.cpp
	xeBatchResult.hpp
	xeCallQueue.cpp
	xeCallQueue.hpp
	xeCommLink.cpp
	xeCommLink.hpp
	xeContainerFormatParser.cpp
	xeContainerFormatParser.hpp
	xeLocalTcpIpLink.cpp
	xeLocalTcpIpLink.hpp
	xeTcpIpLink.cpp
	xeTcpIpLink.hpp
	xeTestCase.cpp
	xeTestCase.hpp
	xeTestCaseListParser.cpp
	xeTestCaseListParser.hpp
	xeTestCaseResult.cpp
	xeTestCaseResult.hpp
	xeTestLogParser.cpp
	xeTestLogParser.hpp
	xeTestLogWriter.cpp
	xeTestLogWriter.hpp
	xeTestResultParser.cpp
	xeTestResultParser.hpp
	xeXMLWriter.cpp
	xeXMLWriter.hpp
	)

set(XECORE_LIBS
	xexml
	xscore
	decpp
	deutil
	dethread
	debase
	)

add_library(xecore STATIC ${XECORE_SRCS})
target_link_libraries(xecore ${XECORE_LIBS})

include_directories(.)
include_directories(../framework/xexml)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX OR DE_OS_IS_ANDROID)
	add_executable(executor tools/xeCommandLineExecutor.cpp)
	target_link_libraries(executor xecore)

	add_executable(testlog-to-csv tools/xeTestLogCompare.cpp)
	target_link_libraries(testlog-to-csv xecore)

	add_executable(testlog-to-xml tools/xeBatchResultToXml.cpp)
	target_link_libraries(testlog-to-xml xecore)

	add_executable(testlog-to-junit tools/xeBatchResultToJUnit.cpp)
	target_link_libraries(testlog-to-junit xecore)

	add_executable(extract-values tools/xeExtractValues.cpp)
	target_link_libraries(extract-values xecore)

	add_executable(extract-shader-programs tools/xeExtractShaderPrograms.cpp)
	target_link_libraries(extract-shader-programs xecore)

	add_executable(merge-testlogs tools/xeMergeTestLogs.cpp)
	target_link_libraries(merge-testlogs xecore)

	add_executable(extract-sample-lists tools/xeExtractSampleLists.cpp)
	target_link_libraries(extract-sample-lists xecore)
endif ()
