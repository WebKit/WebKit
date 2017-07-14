set(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")
set(test_wtf_LIBRARIES
    WTF${DEBUG_SUFFIX}
    gtest
)

set(test_webcore_LIBRARIES
    WTF${DEBUG_SUFFIX}
    WebCore${DEBUG_SUFFIX}
    gtest
)

set(TestWebKitAPI_LIBRARIES
    WTF${DEBUG_SUFFIX}
)

if (ENABLE_WEBKIT2)
    set(test_webkit2_api_LIBRARIES
        JavaScriptCore
        TestWebKitAPIBase
        WTF
        WebKit2
        gtest
    )
    list(APPEND TestWebKitAPI_LIBRARIES
        WebKit
    )
else ()
    list(APPEND TestWebKitAPI_LIBRARIES
        WebKit${DEBUG_SUFFIX}
    )
endif ()


set(TestJavaScriptCore_LIBRARIES
    JavaScriptCore
    gtest
)

set(TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/Counters.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/AtomicString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/BloomFilter.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CrossThreadTask.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CheckedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Condition.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/DateMath.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Deque.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/EnumTraits.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Expected.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Function.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashCountedSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashMap.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/IntegerToStringConversion.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ListHashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Lock.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MD5.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MathExtras.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MediaTime.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MetaAllocator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/NakedPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Optional.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/OptionSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ParkingLot.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RedBlackTree.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Ref.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RefCounter.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RefLogger.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RefPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SHA1.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SaturatedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Scope.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ScopedLambda.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SetForScope.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringBuilder.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringHasher.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringImpl.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringOperators.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringView.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/TextBreakIterator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Time.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/UniqueRef.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Variant.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Vector.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WTFString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WeakPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WorkQueue.cpp
)

# FIXME: Tests/WTF/RunLoop.cpp is missing because it doesn't work for Windows.
# FIXME: Platform-specific sources in Tests/WTF are not included in TestWTF_SOURCES.

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(
    ${TESTWEBKITAPI_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${JAVASCRIPTCORE_DIR}
    ${PAL_DIR}
    ${THIRDPARTY_DIR}/gtest/include
    ${WEBKIT2_DIR}/Platform/IPC
    ${WEBKIT2_DIR}/Shared
    ${WEBKIT2_DIR}/Shared/API
    ${WEBKIT2_DIR}/Shared/API/c
    ${WEBKIT2_DIR}/Shared/Plugins
    ${WEBKIT2_DIR}/UIProcess
    ${WEBKIT2_DIR}/UIProcess/API
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle
    ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c
)

if (ENABLE_WEBKIT2)
    add_library(TestWebKitAPIInjectedBundle SHARED
        ${bundle_harness_SOURCES}
        ${TESTWEBKITAPI_DIR}/InjectedBundleController.cpp
        ${TESTWEBKITAPI_DIR}/InjectedBundleMain.cpp
        ${TESTWEBKITAPI_DIR}/PlatformUtilities.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/CanHandleRequest_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DidAssociateFormControls_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionBasic_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionNoCache_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DocumentStartUserScriptAlertCrash_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/GetInjectedBundleInitializationUserDataCallback_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/HitTestResultNodeHandle_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleBasic_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleFrameHitTest_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleInitializationUserDataCallbackWins_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadCanceledNoServerRedirectCallback_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/MouseMoveAfterCrash_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFails_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutForImages_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFrames_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayout_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ParentFrame_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ResponsivenessTimerDoesntFireEarly_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ShouldGoToBackForwardListItem_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/TextFieldDidBeginAndEndEditing_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMessage_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillLoad_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillSendSubmitEvent_Bundle.cpp
    )

    target_link_libraries(TestWebKitAPIInjectedBundle ${TestWebKitAPI_LIBRARIES})
    add_dependencies(TestWebKitAPIInjectedBundle WTF ${ForwardingHeadersForTestWebKitAPI_NAME})

    get_property(TestWebKitAPIInjectedBundle_PATH TARGET TestWebKitAPIInjectedBundle PROPERTY LOCATION)
endif ()

if (WIN32)
    add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=0)
else ()
    add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=1)
endif ()

add_definitions(-DGTEST_HAS_RTTI=0
    -DTEST_WEBKIT2_RESOURCES_DIR=\"${TESTWEBKITAPI_DIR}/Tests/WebKit\"
    -DTEST_INJECTED_BUNDLE_PATH=\"${TestWebKitAPIInjectedBundle_PATH}\"
)

# FIXME: This works around compatibility problems in the old version of the third-pary
# googletest source code checkout. It should be removed once we upgrade to a newer version.
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
endif ()

add_executable(TestWTF
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)

if (WIN32)
    add_dependencies(TestWTF TestWTFLib)
endif ()

target_link_libraries(TestWTF ${test_wtf_LIBRARIES})
add_dependencies(TestWTF WTF ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})
add_test(TestWTF ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}/TestWTF)
set_tests_properties(TestWTF PROPERTIES TIMEOUT 60)
set_target_properties(TestWTF PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}
)

# FIXME: EFL is the only port that separates the WebCore binaries. Each port ought to do closer to the same thing.
foreach (testName ${test_webcore_BINARIES})
    add_executable(${testName} ${test_main_SOURCES} ${TESTWEBKITAPI_DIR}/TestsController.cpp ${TESTWEBKITAPI_DIR}/Tests/WebCore/${testName}.cpp)
    add_test(${testName} ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore/${testName})
    set_tests_properties(${testName} PROPERTIES TIMEOUT 60)
    target_link_libraries(${testName} ${test_webcore_LIBRARIES})
    set_target_properties(${testName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore
    )
endforeach ()

if (ENABLE_WEBKIT2)
    add_library(TestWebKitAPIBase
        ${test_main_SOURCES}
        ${webkit2_api_harness_SOURCES}
        ${TESTWEBKITAPI_DIR}/JavaScriptTest.cpp
        ${TESTWEBKITAPI_DIR}/PlatformUtilities.cpp
        ${TESTWEBKITAPI_DIR}/TestsController.cpp
    )

    target_link_libraries(TestWebKitAPIBase JavaScriptCore WTF WebKit2 gtest)

    add_dependencies(TestWebKitAPIBase WebKit2 ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})

    foreach (testName ${test_webkit2_api_BINARIES})
        get_filename_component(testBaseName ${testName} NAME)
        add_executable(${testBaseName} ${TESTWEBKITAPI_DIR}/Tests/WebKit/${testName}.cpp)
        add_test(${testBaseName} ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit/${testBaseName})
        set_tests_properties(${testBaseName} PROPERTIES TIMEOUT 60)
        target_link_libraries(${testBaseName} ${test_webkit2_api_LIBRARIES})
        set_target_properties(${testBaseName} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit
        )
    endforeach ()

    # We don't run tests that are expected to fail. We could use the WILL_FAIL
    # property, but it reports failure when the test crashes or timeouts and would
    # make the bot red.
    foreach (testName ${test_webkit2_api_fail_BINARIES})
        add_executable(${testName} ${TESTWEBKITAPI_DIR}/Tests/WebKit/${testName}.cpp)
        target_link_libraries(${testName} ${test_webkit2_api_LIBRARIES})
        set_target_properties(${testName} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebKit/failure
        )
    endforeach ()
endif ()
