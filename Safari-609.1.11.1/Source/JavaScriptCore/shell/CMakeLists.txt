set(jsc_SOURCES
    ../jsc.cpp
)

if ("${JavaScriptCore_LIBRARY_TYPE}" MATCHES "STATIC")
    set(jsc_PRIVATE_DEFINITIONS STATICALLY_LINKED_WITH_JavaScriptCore)
endif ()

set(jsc_LIBRARIES
    ${CMAKE_DL_LIBS}
    WebKit::JavaScriptCore
)

set(jsc_PRIVATE_INCLUDE_DIRECTORIES
    ${JavaScriptCore_INCLUDE_DIRECTORIES}
    ${JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES}
    ${JavaScriptCore_SYSTEM_INCLUDE_DIRECTORIES}
)

if (WTF_OS_MAC_OS_X)
    list(APPEND jsc_LIBRARIES edit)
endif ()

if (WIN32)
    list(APPEND jsc_LIBRARIES Winmm)
endif ()

WEBKIT_EXECUTABLE_DECLARE(jsc)

if (DEVELOPER_MODE)
    set(testapi_SOURCES
        ../API/tests/CompareAndSwapTest.cpp
        ../API/tests/CustomGlobalObjectClassTest.c
        ../API/tests/ExecutionTimeLimitTest.cpp
        ../API/tests/FunctionOverridesTest.cpp
        ../API/tests/GlobalContextWithFinalizerTest.cpp
        ../API/tests/JSONParseTest.cpp
        ../API/tests/JSObjectGetProxyTargetTest.cpp
        ../API/tests/MultithreadedMultiVMExecutionTest.cpp
        ../API/tests/PingPongStackOverflowTest.cpp
        ../API/tests/TypedArrayCTest.cpp
        ../API/tests/testapi.c
        ../API/tests/testapi.cpp
    )
    set(testapi_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testapi_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testapi_LIBRARIES WebKit::JavaScriptCore)

    set(testmasm_SOURCES ../assembler/testmasm.cpp)
    set(testmasm_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testmasm_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testmasm_LIBRARIES WebKit::JavaScriptCore)

    set(testRegExp_SOURCES ../testRegExp.cpp)
    set(testRegExp_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testRegExp_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testRegExp_LIBRARIES WebKit::JavaScriptCore)

    set(testb3_SOURCES
        ../b3/testb3_1.cpp
        ../b3/testb3_2.cpp
        ../b3/testb3_3.cpp
        ../b3/testb3_4.cpp
        ../b3/testb3_5.cpp
        ../b3/testb3_6.cpp
        ../b3/testb3_7.cpp
        ../b3/testb3_8.cpp
    )
    set(testb3_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testb3_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testb3_LIBRARIES WebKit::JavaScriptCore)

    set(testair_SOURCES ../b3/air/testair.cpp)
    set(testair_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testair_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testair_LIBRARIES WebKit::JavaScriptCore)

    set(testdfg_SOURCES ../dfg/testdfg.cpp)
    set(testdfg_DEFINITIONS ${jsc_PRIVATE_DEFINITIONS})
    set(testdfg_PRIVATE_INCLUDE_DIRECTORIES ${jsc_PRIVATE_INCLUDE_DIRECTORIES})
    set(testdfg_LIBRARIES WebKit::JavaScriptCore)

    WEBKIT_EXECUTABLE_DECLARE(testapi)
    WEBKIT_EXECUTABLE_DECLARE(testRegExp)
    WEBKIT_EXECUTABLE_DECLARE(testmasm)
    WEBKIT_EXECUTABLE_DECLARE(testb3)
    WEBKIT_EXECUTABLE_DECLARE(testair)
    WEBKIT_EXECUTABLE_DECLARE(testdfg)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_EXECUTABLE(jsc)

if (SHOULD_INSTALL_JS_SHELL)
    install(TARGETS jsc DESTINATION "${LIBEXEC_INSTALL_DIR}")
endif ()

if (DEVELOPER_MODE)
    WEBKIT_EXECUTABLE(testapi)
    WEBKIT_EXECUTABLE(testRegExp)
    WEBKIT_EXECUTABLE(testmasm)
    WEBKIT_EXECUTABLE(testb3)
    WEBKIT_EXECUTABLE(testair)
    WEBKIT_EXECUTABLE(testdfg)

    file(COPY
        "${JAVASCRIPTCORE_DIR}/API/tests/testapiScripts"
        DESTINATION
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/testapiScripts
    )
endif ()
