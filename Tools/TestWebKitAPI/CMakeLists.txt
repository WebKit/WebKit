set(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")

get_property(gtest_DEFINITIONS GLOBAL PROPERTY gtest_DEFINITIONS)
add_definitions(${gtest_DEFINITIONS})

macro(WEBKIT_TEST _target)
    WEBKIT_EXECUTABLE(${_target})
    add_test(${_target} ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/${_target})
    set_tests_properties(${_target} PROPERTIES TIMEOUT 60)
    set_target_properties(${_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}
    )

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${_target} -Wno-dangling-else
                                               -Wno-sign-compare
                                               -Wno-undef
                                               -Wno-unused-parameter)
    endif ()
endmacro()

# TestWTF definitions
set(TestWTF_SOURCES
    Counters.cpp
    TestsController.cpp
    WTFStringUtilities.cpp

    Tests/WTF/AtomString.cpp
    Tests/WTF/Bitmap.cpp
    Tests/WTF/BloomFilter.cpp
    Tests/WTF/BoxPtr.cpp
    Tests/WTF/BumpPointerAllocator.cpp
    Tests/WTF/CString.cpp
    Tests/WTF/CheckedArithmeticOperations.cpp
    Tests/WTF/CompactRefPtrTuple.cpp
    Tests/WTF/ConcurrentPtrHashSet.cpp
    Tests/WTF/Condition.cpp
    Tests/WTF/CrossThreadCopier.cpp
    Tests/WTF/CrossThreadTask.cpp
    Tests/WTF/DataMutex.cpp
    Tests/WTF/DateMath.cpp
    Tests/WTF/Deque.cpp
    Tests/WTF/EnumTraits.cpp
    Tests/WTF/Expected.cpp
    Tests/WTF/FileSystem.cpp
    Tests/WTF/Function.cpp
    Tests/WTF/HashCountedSet.cpp
    Tests/WTF/HashMap.cpp
    Tests/WTF/HashSet.cpp
    Tests/WTF/Hasher.cpp
    Tests/WTF/HexNumber.cpp
    Tests/WTF/IndexSparseSet.cpp
    Tests/WTF/IntegerToStringConversion.cpp
    Tests/WTF/IteratorRange.cpp
    Tests/WTF/JSONValue.cpp
    Tests/WTF/LEBDecoder.cpp
    Tests/WTF/LifecycleLogger.cpp
    Tests/WTF/LineEnding.cpp
    Tests/WTF/ListHashSet.cpp
    Tests/WTF/Lock.cpp
    Tests/WTF/Logger.cpp
    Tests/WTF/Markable.cpp
    Tests/WTF/MathExtras.cpp
    Tests/WTF/MediaTime.cpp
    Tests/WTF/MetaAllocator.cpp
    Tests/WTF/MoveOnlyLifecycleLogger.cpp
    Tests/WTF/NakedPtr.cpp
    Tests/WTF/NeverDestroyed.cpp
    Tests/WTF/OptionSet.cpp
    Tests/WTF/Optional.cpp
    Tests/WTF/Packed.cpp
    Tests/WTF/PackedRef.cpp
    Tests/WTF/PackedRefPtr.cpp
    Tests/WTF/ParkingLot.cpp
    Tests/WTF/PriorityQueue.cpp
    Tests/WTF/RedBlackTree.cpp
    Tests/WTF/Ref.cpp
    Tests/WTF/RefCounter.cpp
    Tests/WTF/RefLogger.cpp
    Tests/WTF/RefPtr.cpp
    Tests/WTF/RunLoop.cpp
    Tests/WTF/SHA1.cpp
    Tests/WTF/SaturatedArithmeticOperations.cpp
    Tests/WTF/Scope.cpp
    Tests/WTF/ScopedLambda.cpp
    Tests/WTF/SetForScope.cpp
    Tests/WTF/StdLibExtras.cpp
    Tests/WTF/StringBuilder.cpp
    Tests/WTF/StringConcatenate.cpp
    Tests/WTF/StringHasher.cpp
    Tests/WTF/StringImpl.cpp
    Tests/WTF/StringOperators.cpp
    Tests/WTF/StringParsingBuffer.cpp
    Tests/WTF/StringView.cpp
    Tests/WTF/SynchronizedFixedQueue.cpp
    Tests/WTF/TextBreakIterator.cpp
    Tests/WTF/ThreadGroup.cpp
    Tests/WTF/ThreadMessages.cpp
    Tests/WTF/Threading.cpp
    Tests/WTF/Time.cpp
    Tests/WTF/URL.cpp
    Tests/WTF/URLParser.cpp
    Tests/WTF/UniqueArray.cpp
    Tests/WTF/UniqueRef.cpp
    Tests/WTF/Variant.cpp
    Tests/WTF/Vector.cpp
    Tests/WTF/WTFString.cpp
    Tests/WTF/WeakPtr.cpp
    Tests/WTF/WorkQueue.cpp
    Tests/WTF/WorkerPool.cpp
)

set(TestWTF_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TESTWEBKITAPI_DIR}
    ${THIRDPARTY_DIR}/gtest/include
)

set(TestWTF_LIBRARIES
    WebKit::WTF
    gtest
)

WEBKIT_EXECUTABLE_DECLARE(TestWTF)

# TestJavaScriptCore definitions
if (ENABLE_JAVASCRIPTCORE)
    set(TestJavaScriptCore_SOURCES
        TestsController.cpp
        WTFStringUtilities.cpp

        Tests/JavaScriptCore/DisallowVMEntry.cpp
        Tests/JavaScriptCore/PropertySlot.cpp
    )

    set(TestJavaScriptCore_LIBRARIES
        WebKit::JavaScriptCore
        gtest
    )

    set(TestJavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
        ${THIRDPARTY_DIR}/gtest/include
        ${JavaScriptCoreCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )

    WEBKIT_EXECUTABLE_DECLARE(TestJavaScriptCore)
endif ()

# TestWebCore definitions
if (ENABLE_WEBCORE)
    set(TestWebCore_SOURCES
        TestsController.cpp
        WTFStringUtilities.cpp

        Tests/WebCore/AffineTransform.cpp
        Tests/WebCore/CSSParser.cpp
        Tests/WebCore/CalculationValue.cpp
        Tests/WebCore/ColorTests.cpp
        Tests/WebCore/ComplexTextController.cpp
        Tests/WebCore/ContextMenuAction.cpp
        Tests/WebCore/ExtendedColorTests.cpp
        Tests/WebCore/FileMonitor.cpp
        Tests/WebCore/FloatPointTests.cpp
        Tests/WebCore/FloatRectTests.cpp
        Tests/WebCore/FloatSizeTests.cpp
        Tests/WebCore/GridPosition.cpp
        Tests/WebCore/HTMLParserIdioms.cpp
        Tests/WebCore/HTTPParsers.cpp
        Tests/WebCore/IntPointTests.cpp
        Tests/WebCore/IntRectTests.cpp
        Tests/WebCore/IntSizeTests.cpp
        Tests/WebCore/KeyedCoding.cpp
        Tests/WebCore/LayoutUnitTests.cpp
        Tests/WebCore/MIMETypeRegistry.cpp
        Tests/WebCore/ParsedContentRange.cpp
        Tests/WebCore/PublicSuffix.cpp
        Tests/WebCore/SecurityOrigin.cpp
        Tests/WebCore/SharedBuffer.cpp
        Tests/WebCore/SharedBufferTest.cpp
        Tests/WebCore/TimeRanges.cpp
        Tests/WebCore/TransformationMatrix.cpp
        Tests/WebCore/URLParserTextEncoding.cpp
    )

    set(TestWebCore_LIBRARIES
        WebKit::WebCore
        gtest
    )

    set(TestWebCore_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
        ${THIRDPARTY_DIR}/gtest/include
        ${PAL_FRAMEWORK_HEADERS_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )

    WEBKIT_EXECUTABLE_DECLARE(TestWebCore)
endif ()

# TestWebKitLegacy definitions
if (ENABLE_WEBKIT_LEGACY)
    set(TestWebKitLegacy_SOURCES
        TestsController.cpp
        WTFStringUtilities.cpp
    )

    set(TestWebKitLegacy_LIBRARIES
        gtest
    )
    if (NOT APPLE)
        list(APPEND TestWebKitLegacy_LIBRARIES
            WebKitLegacy
        )
    endif ()

    set(TestWebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
        ${THIRDPARTY_DIR}/gtest/include
        ${PAL_FRAMEWORK_HEADERS_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    )

    set(TestWebKitLegacy_DEPENDENCIES WebKitLegacyFrameworkHeaders)

    WEBKIT_EXECUTABLE_DECLARE(TestWebKitLegacy)
endif ()

# TestWebKit definitions
if (ENABLE_WEBKIT)
    set(TestWebKit_SOURCES
        Tests/WebKit/AboutBlankLoad.cpp
        Tests/WebKit/CanHandleRequest.cpp
        Tests/WebKit/DOMWindowExtensionBasic.cpp
        Tests/WebKit/DOMWindowExtensionNoCache.cpp
        Tests/WebKit/DocumentStartUserScriptAlertCrash.cpp
        Tests/WebKit/DownloadDecideDestinationCrash.cpp
        Tests/WebKit/EnumerateMediaDevices.cpp
        Tests/WebKit/EvaluateJavaScript.cpp
        Tests/WebKit/FailedLoad.cpp
        Tests/WebKit/Find.cpp
        Tests/WebKit/FirstMeaningfulPaintMilestone.cpp
        Tests/WebKit/ForceRepaint.cpp
        Tests/WebKit/FrameMIMETypeHTML.cpp
        Tests/WebKit/FrameMIMETypePNG.cpp
        Tests/WebKit/Geolocation.cpp
        Tests/WebKit/GetInjectedBundleInitializationUserDataCallback.cpp
        Tests/WebKit/HitTestResultNodeHandle.cpp
        Tests/WebKit/InjectedBundleBasic.cpp
        Tests/WebKit/InjectedBundleFrameHitTest.cpp
        Tests/WebKit/InjectedBundleInitializationUserDataCallbackWins.cpp
        Tests/WebKit/LoadAlternateHTMLStringWithNonDirectoryURL.cpp
        Tests/WebKit/LoadCanceledNoServerRedirectCallback.cpp
        Tests/WebKit/LoadPageOnCrash.cpp
        Tests/WebKit/MouseMoveAfterCrash.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayout.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFails.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutForImages.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFrames.cpp
        Tests/WebKit/PageLoadBasic.cpp
        Tests/WebKit/PageLoadDidChangeLocationWithinPageForFrame.cpp
        Tests/WebKit/PageLoadState.cpp
        Tests/WebKit/ParentFrame.cpp
        Tests/WebKit/PendingAPIRequestURL.cpp
        Tests/WebKit/PreventEmptyUserAgent.cpp
        Tests/WebKit/PrivateBrowsingPushStateNoHistoryCallback.cpp
        Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback.cpp
        Tests/WebKit/ReloadPageAfterCrash.cpp
        Tests/WebKit/ResizeWindowAfterCrash.cpp
        Tests/WebKit/RestoreSessionStateContainingFormData.cpp
        Tests/WebKit/TextFieldDidBeginAndEndEditing.cpp
        Tests/WebKit/UserMedia.cpp
        Tests/WebKit/UserMessage.cpp
        Tests/WebKit/WKPageCopySessionStateWithFiltering.cpp
        Tests/WebKit/WKPageGetScaleFactorNotZero.cpp
        Tests/WebKit/WKPreferences.cpp
        Tests/WebKit/WKRetainPtr.cpp
        Tests/WebKit/WKString.cpp
        Tests/WebKit/WKStringJSString.cpp
        Tests/WebKit/WKURL.cpp
        Tests/WebKit/WillSendSubmitEvent.cpp
    )

    set(TestWebKit_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
        ${THIRDPARTY_DIR}/gtest/include
        ${PAL_FRAMEWORK_HEADERS_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )

    set(TestWebKit_LIBRARIES
        TestWebKitAPIBase
        WebKit::WebKit
        gtest
    )

    set(TestWebKit_DEPENDENCIES
        TestWebKitAPIInjectedBundle
    )

    add_library(TestWebKitAPIBase STATIC
        JavaScriptTest.cpp
        PlatformUtilities.cpp
        TestsController.cpp
        WTFStringUtilities.cpp
    )
    target_compile_definitions(TestWebKitAPIBase PRIVATE BUILDING_TestWebKit)
    target_include_directories(TestWebKitAPIBase PRIVATE ${TestWebKit_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(TestWebKitAPIBase PRIVATE WebKit::WebKit gtest)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIBase -Wno-dangling-else
                                                      -Wno-sign-compare
                                                      -Wno-undef
                                                      -Wno-unused-parameter)
    endif ()

    set(TestWebKitAPIInjectedBundle_SOURCES
        InjectedBundleController.cpp
        InjectedBundleMain.cpp
        PlatformUtilities.cpp
        Tests/WebKit/CanHandleRequest_Bundle.cpp
        Tests/WebKit/DidAssociateFormControls_Bundle.cpp
        Tests/WebKit/DOMWindowExtensionBasic_Bundle.cpp
        Tests/WebKit/DOMWindowExtensionNoCache_Bundle.cpp
        Tests/WebKit/DocumentStartUserScriptAlertCrash_Bundle.cpp
        Tests/WebKit/GetInjectedBundleInitializationUserDataCallback_Bundle.cpp
        Tests/WebKit/HitTestResultNodeHandle_Bundle.cpp
        Tests/WebKit/InjectedBundleBasic_Bundle.cpp
        Tests/WebKit/InjectedBundleFrameHitTest_Bundle.cpp
        Tests/WebKit/InjectedBundleInitializationUserDataCallbackWins_Bundle.cpp
        Tests/WebKit/LoadCanceledNoServerRedirectCallback_Bundle.cpp
        Tests/WebKit/MouseMoveAfterCrash_Bundle.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFails_Bundle.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutForImages_Bundle.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayoutFrames_Bundle.cpp
        Tests/WebKit/NewFirstVisuallyNonEmptyLayout_Bundle.cpp
        Tests/WebKit/FirstMeaningfulPaintMilestone_Bundle.cpp
        Tests/WebKit/ParentFrame_Bundle.cpp
        Tests/WebKit/ProvisionalURLAfterWillSendRequestCallback_Bundle.cpp
        Tests/WebKit/ResponsivenessTimerDoesntFireEarly_Bundle.cpp
        Tests/WebKit/TextFieldDidBeginAndEndEditing_Bundle.cpp
        Tests/WebKit/UserMessage_Bundle.cpp
        Tests/WebKit/WillLoad_Bundle.cpp
        Tests/WebKit/WillSendSubmitEvent_Bundle.cpp
    )
    if (APPLE)
        list(APPEND TestWebKitAPIInjectedBundle_SOURCES
            Tests/WebKit/DOMWindowExtensionBasic_Bundle.cpp

            cocoa/UtilitiesCocoa.mm

            mac/InjectedBundleControllerMac.mm
            mac/PlatformUtilitiesMac.mm
        )
    endif ()
    add_library(TestWebKitAPIInjectedBundle SHARED ${TestWebKitAPIInjectedBundle_SOURCES})

    target_compile_definitions(TestWebKitAPIInjectedBundle PRIVATE BUILDING_TestWebKit)
    target_include_directories(TestWebKitAPIInjectedBundle PRIVATE ${TestWebKit_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(TestWebKitAPIInjectedBundle PRIVATE WebKit::WebKit)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIInjectedBundle -Wno-dangling-else
                                                                -Wno-sign-compare
                                                                -Wno-undef
                                                                -Wno-unused-parameter)
    endif ()

    WEBKIT_EXECUTABLE_DECLARE(TestWebKit)
endif ()

# Include platform specific files
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# TestWTF target
WEBKIT_TEST(TestWTF)

# TestJavaScriptCore target
if (ENABLE_JAVASCRIPTCORE)
    WEBKIT_TEST(TestJavaScriptCore)
endif ()

# TestWebCore target
if (ENABLE_WEBCORE)
    WEBKIT_TEST(TestWebCore)
endif ()

# TestWebKitLegacy target
if (ENABLE_WEBKIT_LEGACY)
    WEBKIT_TEST(TestWebKitLegacy)
endif ()

# TestWebKit target
if (ENABLE_WEBKIT)
    WEBKIT_TEST(TestWebKit)
endif ()
