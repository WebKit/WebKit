SET(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}
    ${TESTWEBKITAPI_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include
    ${JAVASCRIPTCORE_DIR}
    ${JAVASCRIPTCORE_DIR}/API
    ${JAVASCRIPTCORE_DIR}/ForwardingHeaders
    ${THIRDPARTY_DIR}/gtest/include
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/platform/network
    ${WEBKIT2_DIR}/Shared
    ${WEBKIT2_DIR}/Shared/API/c
    ${WEBKIT2_DIR}/UIProcess
    ${WTF_DIR}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

ADD_LIBRARY(TestWebKitAPIInjectedBundle SHARED
    ${bundle_harness_SOURCES}
    ${TESTWEBKITAPI_DIR}/InjectedBundleController.cpp
    ${TESTWEBKITAPI_DIR}/InjectedBundleMain.cpp
    ${TESTWEBKITAPI_DIR}/PlatformUtilities.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/CanHandleRequest_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DOMWindowExtensionBasic_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DOMWindowExtensionNoCache_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/DocumentStartUserScriptAlertCrash_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/GetInjectedBundleInitializationUserDataCallback_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/HitTestResultNodeHandle_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/InjectedBundleBasic_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/InjectedBundleInitializationUserDataCallbackWins_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/LoadCanceledNoServerRedirectCallback_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/MouseMoveAfterCrash_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutFails_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutForImages_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayoutFrames_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/NewFirstVisuallyNonEmptyLayout_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ParentFrame_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ResponsivenessTimerDoesntFireEarly_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/ShouldGoToBackForwardListItem_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/UserMessage_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WKConnection_Bundle.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/WillSendSubmitEvent_Bundle.cpp
)

TARGET_LINK_LIBRARIES(TestWebKitAPIInjectedBundle ${WTF_LIBRARY_NAME})
ADD_DEPENDENCIES(TestWebKitAPIInjectedBundle ${WebKit2_LIBRARY_NAME} ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})

GET_PROPERTY(TestWebKitAPIInjectedBundle_PATH TARGET TestWebKitAPIInjectedBundle PROPERTY LOCATION)

ADD_DEFINITIONS(-DGTEST_LINKED_AS_SHARED_LIBRARY=1
    -DTEST_WEBKIT2_RESOURCES_DIR=\"${TESTWEBKITAPI_DIR}/Tests/WebKit2\"
    -DTEST_INJECTED_BUNDLE_PATH=\"${TestWebKitAPIInjectedBundle_PATH}\"
)

SET(test_wtf_LIBRARIES
    ${WTF_LIBRARY_NAME}
    gtest
)

ADD_EXECUTABLE(test_wtf
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/AtomicString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CheckedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Functional.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashMap.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MetaAllocator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/RedBlackTree.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringBuilder.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringImpl.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringOperators.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/TemporaryChange.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Vector.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/VectorBasic.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/VectorReverse.cpp
)

TARGET_LINK_LIBRARIES(test_wtf ${test_wtf_LIBRARIES})
ADD_DEPENDENCIES(test_wtf ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})
ADD_TEST(test_wtf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_wtf)
SET_TESTS_PROPERTIES(test_wtf PROPERTIES TIMEOUT 60)

SET(test_webcore_LIBRARIES
    ${WTF_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    gtest
)

ADD_EXECUTABLE(test_webcore
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/KURL.cpp
)

TARGET_LINK_LIBRARIES(test_webcore ${test_webcore_LIBRARIES})
ADD_DEPENDENCIES(test_webcore ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})
ADD_TEST(test_webcore ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_webcore)
SET_TESTS_PROPERTIES(test_webcore PROPERTIES TIMEOUT 60)

ADD_LIBRARY(TestWebKitAPIBase
    ${test_main_SOURCES}
    ${webkit2_api_harness_SOURCES}
    ${TESTWEBKITAPI_DIR}/JavaScriptTest.cpp
    ${TESTWEBKITAPI_DIR}/PlatformUtilities.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
)

ADD_DEPENDENCIES(TestWebKitAPIBase ${WebKit2_LIBRARY_NAME} ${ForwardingHeadersForTestWebKitAPI_NAME} ${ForwardingNetworkHeadersForTestWebKitAPI_NAME})

SET(test_webkit2_api_LIBRARIES
    TestWebKitAPIBase
    ${WTF_LIBRARY_NAME}
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebKit2_LIBRARY_NAME}
    gtest
)

FOREACH(testName ${test_webkit2_api_BINARIES})
    ADD_EXECUTABLE(test_webkit2_api_${testName} ${TESTWEBKITAPI_DIR}/Tests/WebKit2/${testName}.cpp)
    ADD_TEST(test_webkit2_api_${testName} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_webkit2_api_${testName})
    SET_TESTS_PROPERTIES(test_webkit2_api_${testName} PROPERTIES TIMEOUT 60)
    TARGET_LINK_LIBRARIES(test_webkit2_api_${testName} ${test_webkit2_api_LIBRARIES})
ENDFOREACH()

# We don't run tests that are expected to fail. We could use the WILL_FAIL
# property, but it reports failure when the test crashes or timeouts and would
# make the bot red.

FOREACH(testName ${test_webkit2_api_fail_BINARIES})
    ADD_EXECUTABLE(test_webkit2_api_fail_${testName} ${TESTWEBKITAPI_DIR}/Tests/WebKit2/${testName}.cpp)
    TARGET_LINK_LIBRARIES(test_webkit2_api_fail_${testName} ${test_webkit2_api_LIBRARIES})
ENDFOREACH()
