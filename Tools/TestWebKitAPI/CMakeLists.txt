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

set(TestWebKitAPI_DEPENDENCIES
    JavaScriptCoreForwardingHeaders
    JavaScriptCorePrivateForwardingHeaders
    WTFForwardingHeaders
)

if (ENABLE_WEBCORE)
    list(APPEND TestWebKitAPI_DEPENDENCIES PALForwardingHeaders)
endif ()

if (ENABLE_WEBKIT)
    set(test_webkit_api_LIBRARIES
        JavaScriptCore
        TestWebKitAPIBase
        WTF
        WebKit
        gtest
    )

    set(test_webkit_api_SOURCES
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/AboutBlankLoad.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/CanHandleRequest.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/CookieManager.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DocumentStartUserScriptAlertCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionBasic.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DOMWindowExtensionNoCache.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/DownloadDecideDestinationCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/EnumerateMediaDevices.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/EvaluateJavaScript.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/FailedLoad.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/Find.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/FirstMeaningfulPaintMilestone.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ForceRepaint.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/FrameMIMETypeHTML.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/FrameMIMETypePNG.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/Geolocation.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/GetInjectedBundleInitializationUserDataCallback.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/HitTestResultNodeHandle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleBasic.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleFrameHitTest.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/InjectedBundleInitializationUserDataCallbackWins.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadAlternateHTMLStringWithNonDirectoryURL.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadCanceledNoServerRedirectCallback.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/LoadPageOnCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/MouseMoveAfterCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayout.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFails.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutForImages.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFrames.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/PageLoadBasic.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/PageLoadDidChangeLocationWithinPageForFrame.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ParentFrame.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/PendingAPIRequestURL.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/PreventEmptyUserAgent.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/PrivateBrowsingPushStateNoHistoryCallback.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ReloadPageAfterCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ResizeWindowAfterCrash.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/RestoreSessionStateContainingFormData.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/TextFieldDidBeginAndEndEditing.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMedia.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMessage.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillSendSubmitEvent.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPageCopySessionStateWithFiltering.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPageGetScaleFactorNotZero.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKPreferences.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKRetainPtr.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKString.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKStringJSString.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WKURL.cpp
    )

    set(TestWebKitAPIBase_LIBRARIES
        JavaScriptCore
        WTF
        WebKit
        gtest
    )

    set(TestWebKitAPIBase_SOURCES
        ${TESTWEBKITAPI_DIR}/JavaScriptTest.cpp
        ${TESTWEBKITAPI_DIR}/PlatformUtilities.cpp
        ${TESTWEBKITAPI_DIR}/TestsController.cpp
    )

    list(APPEND TestWebKitAPI_LIBRARIES
        WebKit
    )
else ()
    list(APPEND TestWebKitAPI_LIBRARIES
        WebKitLegacy${DEBUG_SUFFIX}
    )
endif ()


set(TestWTF_SOURCES
    ${TESTWEBKITAPI_DIR}/Counters.cpp
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/AtomicString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/BloomFilter.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CheckedArithmeticOperations.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ConcurrentPtrHashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Condition.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/CrossThreadTask.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/DateMath.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Deque.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/EnumTraits.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Expected.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Function.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashCountedSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashMap.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/HashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Hasher.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/IntegerToStringConversion.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/JSONValue.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/LEBDecoder.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/LifecycleLogger.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/LineEnding.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ListHashSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Lock.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Logger.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MD5.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Markable.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MathExtras.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MediaTime.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MetaAllocator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/MoveOnlyLifecycleLogger.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/NakedPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/NeverDestroyed.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Optional.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/OptionSet.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ParkingLot.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Poisoned.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PoisonedRef.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PoisonedRefPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PoisonedUniquePtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PoisonedUniquePtrForNonTriviallyDestructibleArrays.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PoisonedUniquePtrForTriviallyDestructibleArrays.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/PriorityQueue.cpp
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
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringConcatenate.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringHasher.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringImpl.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringOperators.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/StringView.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/SynchronizedFixedQueue.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/TextBreakIterator.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ThreadGroup.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/ThreadMessages.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Threading.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Time.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/URL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/URLParser.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/UniqueArray.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/UniqueRef.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Variant.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/Vector.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WTFString.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WeakPtr.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WorkQueue.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WTF/WorkerPool.cpp
)

# FIXME: Make these work on Windows too.
if (NOT WIN32)
    list(APPEND TestWTF_SOURCES
        ${TESTWEBKITAPI_DIR}/Tests/WTF/FileSystem.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WTF/RunLoop.cpp
    )
endif ()

# FIXME: Platform-specific sources in Tests/WTF are not included in TestWTF_SOURCES.

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(
    ${TESTWEBKITAPI_DIR}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${JAVASCRIPTCORE_DIR}
    ${THIRDPARTY_DIR}/gtest/include
    ${WEBKIT_DIR}/Platform/IPC
    ${WEBKIT_DIR}/Shared
    ${WEBKIT_DIR}/Shared/API
    ${WEBKIT_DIR}/Shared/API/c
    ${WEBKIT_DIR}/Shared/Plugins
    ${WEBKIT_DIR}/UIProcess
    ${WEBKIT_DIR}/UIProcess/API
    ${WEBKIT_DIR}/UIProcess/API/C
    ${WEBKIT_DIR}/WebProcess/InjectedBundle
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c
)

if (ENABLE_WEBKIT)
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
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/FirstMeaningfulPaintMilestone_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ParentFrame_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/ResponsivenessTimerDoesntFireEarly_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/TextFieldDidBeginAndEndEditing_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/UserMessage_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillLoad_Bundle.cpp
        ${TESTWEBKITAPI_DIR}/Tests/WebKit/WillSendSubmitEvent_Bundle.cpp
    )

    target_link_libraries(TestWebKitAPIInjectedBundle ${TestWebKitAPI_LIBRARIES})
    add_dependencies(TestWebKitAPIInjectedBundle WTF ${TestWebKitAPI_DEPENDENCIES})

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIInjectedBundle -Wno-dangling-else
                                                                -Wno-sign-compare
                                                                -Wno-undef
                                                                -Wno-unused-parameter)
    endif ()
endif ()

get_property(gtest_DEFINITIONS GLOBAL PROPERTY gtest_DEFINITIONS)

add_definitions(
    ${gtest_DEFINITIONS}
    -DTEST_WEBKIT2_RESOURCES_DIR=\"${TESTWEBKITAPI_DIR}/Tests/WebKit\"
)

add_executable(TestWTF
    ${test_main_SOURCES}
    ${TestWTF_SOURCES}
)

if (WIN32)
    add_dependencies(TestWTF TestWTFLib)
else ()
    add_dependencies(TestWTF WTF ${TestWebKitAPI_DEPENDENCIES})
endif ()

target_link_libraries(TestWTF ${test_wtf_LIBRARIES})
add_test(TestWTF ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}/TestWTF)
set_tests_properties(TestWTF PROPERTIES TIMEOUT 60)
set_target_properties(TestWTF PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF}
)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(TestWTF -Wno-dangling-else
                                        -Wno-sign-compare
                                        -Wno-undef
                                        -Wno-unused-parameter)
endif ()

if (ENABLE_WEBKIT)
    add_library(TestWebKitAPIBase
        ${test_main_SOURCES}
        ${webkit_api_harness_SOURCES}
        ${TestWebKitAPIBase_SOURCES}
    )

    target_link_libraries(TestWebKitAPIBase ${TestWebKitAPIBase_LIBRARIES})

    add_dependencies(TestWebKitAPIBase WebKit ${TestWebKitAPI_DEPENDENCIES})

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIBase -Wno-sign-compare
                                                      -Wno-undef
                                                      -Wno-unused-parameter)
    endif ()
endif ()
