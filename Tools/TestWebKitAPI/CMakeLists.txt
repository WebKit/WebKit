set(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")
set(TestWebKitAPI_DISABLED_WARNINGS
    -Wno-dangling-else
    -Wno-sign-compare
    -Wno-undef
    -Wno-unused-parameter
)

macro(WEBKIT_TEST _target)
    WEBKIT_EXECUTABLE(${_target})
    add_test(${_target} ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/${_target})
    set_tests_properties(${_target} PROPERTIES TIMEOUT 60)
    set_target_properties(${_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}
    )

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(${_target} ${TestWebKitAPI_DISABLED_WARNINGS})
    endif ()
endmacro()

# TestWTF definitions
set(TestWTF_SOURCES
    Counters.cpp
    TestsController.cpp
    Utilities.cpp

    Tests/WTF/AtomString.cpp
    Tests/WTF/Base64.cpp
    Tests/WTF/BitSet.cpp
    Tests/WTF/BloomFilter.cpp
    Tests/WTF/BoxPtr.cpp
    Tests/WTF/BumpPointerAllocator.cpp
    Tests/WTF/CString.cpp
    Tests/WTF/CharacterProperties.cpp
    Tests/WTF/CheckedArithmeticOperations.cpp
    Tests/WTF/CompactPtr.cpp
    Tests/WTF/CompactRefPtr.cpp
    Tests/WTF/CompactRefPtrTuple.cpp
    Tests/WTF/CompactUniquePtrTuple.cpp
    Tests/WTF/CompletionHandlerTests.cpp
    Tests/WTF/ConcurrentPtrHashSet.cpp
    Tests/WTF/Condition.cpp
    Tests/WTF/CrossThreadCopierTests.cpp
    Tests/WTF/CrossThreadTask.cpp
    Tests/WTF/DataMutex.cpp
    Tests/WTF/DateMath.cpp
    Tests/WTF/Deque.cpp
    Tests/WTF/DragonBoxTest.cpp
    Tests/WTF/EmbeddedFixedVector.cpp
    Tests/WTF/EnumTraits.cpp
    Tests/WTF/Expected.cpp
    Tests/WTF/FileSystem.cpp
    Tests/WTF/FixedBitVector.cpp
    Tests/WTF/FixedVector.cpp
    Tests/WTF/Function.cpp
    Tests/WTF/HashCountedSet.cpp
    Tests/WTF/HashMap.cpp
    Tests/WTF/HashSet.cpp
    Tests/WTF/Hasher.cpp
    Tests/WTF/HexNumber.cpp
    Tests/WTF/IndexSparseSet.cpp
    Tests/WTF/Int128.cpp
    Tests/WTF/IntegerToStringConversion.cpp
    Tests/WTF/IteratorRange.cpp
    Tests/WTF/JSONValue.cpp
    Tests/WTF/LEBDecoder.cpp
    Tests/WTF/LazyRef.cpp
    Tests/WTF/LazyUniqueRef.cpp
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
    Tests/WTF/NativePromise.cpp
    Tests/WTF/NeverDestroyed.cpp
    Tests/WTF/OptionSet.cpp
    Tests/WTF/Packed.cpp
    Tests/WTF/PackedRef.cpp
    Tests/WTF/PackedRefPtr.cpp
    Tests/WTF/ParkingLot.cpp
    Tests/WTF/PriorityQueue.cpp
    Tests/WTF/RedBlackTree.cpp
    Tests/WTF/Ref.cpp
    Tests/WTF/RefCountedFixedVector.cpp
    Tests/WTF/RefCounter.cpp
    Tests/WTF/RefLogger.cpp
    Tests/WTF/RefPtr.cpp
    Tests/WTF/RobinHoodHashMap.cpp
    Tests/WTF/RobinHoodHashSet.cpp
    Tests/WTF/RunLoop.cpp
    Tests/WTF/SHA1.cpp
    Tests/WTF/SafeStrerror.cpp
    Tests/WTF/SaturatedArithmeticOperations.cpp
    Tests/WTF/Scope.cpp
    Tests/WTF/ScopedLambda.cpp
    Tests/WTF/SentinelLinkedList.cpp
    Tests/WTF/SetForScope.cpp
    Tests/WTF/Signals.cpp
    Tests/WTF/StackTraceTest.cpp
    Tests/WTF/StdLibExtrasTests.cpp
    Tests/WTF/StringBuilder.cpp
    Tests/WTF/StringCommon.cpp
    Tests/WTF/StringConcatenate.cpp
    Tests/WTF/StringHasher.cpp
    Tests/WTF/SuperFastHash.cpp
    Tests/WTF/WYHash.cpp
    Tests/WTF/StringImpl.cpp
    Tests/WTF/StringParsingBuffer.cpp
    Tests/WTF/StringSearch.cpp
    Tests/WTF/StringToIntegerConversion.cpp
    Tests/WTF/StringView.cpp
    Tests/WTF/SynchronizedFixedQueue.cpp
    Tests/WTF/TextBreakIterator.cpp
    Tests/WTF/ThreadGroup.cpp
    Tests/WTF/ThreadMessages.cpp
    Tests/WTF/Threading.cpp
    Tests/WTF/Time.cpp
    Tests/WTF/URL.cpp
    Tests/WTF/URLParser.cpp
    Tests/WTF/UTF8Conversion.cpp
    Tests/WTF/UniqueArray.cpp
    Tests/WTF/UniqueRef.cpp
    Tests/WTF/UniqueRefVector.cpp
    Tests/WTF/Vector.cpp
    Tests/WTF/WTFString.cpp
    Tests/WTF/WeakPtr.cpp
    Tests/WTF/WorkQueue.cpp
    Tests/WTF/WorkerPool.cpp
)

set(TestWTF_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TESTWEBKITAPI_DIR}
)

set(TestWTF_LIBRARIES
    WebKit::gtest
)

set(TestWTF_FRAMEWORKS
    WTF
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND TestWTF_FRAMEWORKS bmalloc)
endif ()

WEBKIT_EXECUTABLE_DECLARE(TestWTF)

# TestJavaScriptCore definitions
if (ENABLE_JAVASCRIPTCORE)
    set(TestJavaScriptCore_SOURCES
        TestsController.cpp

        Tests/JavaScriptCore/DisallowVMEntry.cpp
        Tests/JavaScriptCore/InspectorConsoleMessage.cpp
        Tests/JavaScriptCore/PropertySlot.cpp
    )

    set(TestJavaScriptCore_LIBRARIES
        WebKit::gtest
    )
    set(TestJavaScriptCore_FRAMEWORKS
        JavaScriptCore
        WTF
    )
    if (NOT USE_SYSTEM_MALLOC)
        list(APPEND TestJavaScriptCore_FRAMEWORKS bmalloc)
    endif ()

    set(TestJavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
    )

    WEBKIT_EXECUTABLE_DECLARE(TestJavaScriptCore)
endif ()

# TestWebCore definitions
if (ENABLE_WEBCORE)
    set(TestWebCore_SOURCES
        TestController.cpp
        Utilities.cpp

        Tests/WebCore/cg/BifurcatedGraphicsContextCGTests.cpp
        Tests/WebCore/cg/DisplayListCGTests.cpp
        Tests/WebCore/cg/GraphicsContextCGTests.mm
        Tests/WebCore/cocoa/AudioStreamDescriptionCocoaTests.mm
        Tests/WebCore/cocoa/AVFoundationSoftLinkTests.mm
        Tests/WebCore/cocoa/CoreCryptoSPI.h
        Tests/WebCore/cocoa/CoreMediaUtilitiesTests.mm
        Tests/WebCore/cocoa/DatabaseTrackerTests.mm
        Tests/WebCore/cocoa/ImageRotationSessionVTTests.cpp
        Tests/WebCore/cocoa/IOSurfaceTests.mm
        Tests/WebCore/cocoa/PrivateClickMeasurementCocoaTests.mm
        Tests/WebCore/cocoa/SerializedCryptoKeyWrapTests.mm
        Tests/WebCore/cocoa/SharedBufferTests.mm
        Tests/WebCore/cocoa/TestGraphicsContextGLCocoaTests.mm
        Tests/WebCore/cocoa/TestUTIRegistryTests.cpp
        Tests/WebCore/cocoa/TestUTIUtilitiesTests.cpp
        Tests/WebCore/cocoa/WebCoreNSURLSessionTests.mm
        Tests/WebCore/cocoa/XMLParsingTests.mm
        Tests/WebCore/curl/CookiesTests.cpp
        Tests/WebCore/curl/CurlMultipartHandleTests.cpp
        Tests/WebCore/curl/OpenSSLHelperTests.cpp
        Tests/WebCore/gstreamer/GstElementHarnessTests.cpp
        Tests/WebCore/gstreamer/GstMappedBufferTests.cpp
        Tests/WebCore/gstreamer/GStreamerTests.cpp
        Tests/WebCore/ios/PreviewConverterTests.cpp
        Tests/WebCore/win/BitmapImageTests.cpp
        Tests/WebCore/win/DIBPixelDataTests.cpp
        Tests/WebCore/win/LinkedFontsTests.cpp
        Tests/WebCore/win/WebCoreBundleTests.cpp
        Tests/WebCore/AbortableTaskQueueTests.cpp
        Tests/WebCore/AffineTransformTests.cpp
        Tests/WebCore/AnimationFrameRateTests.cpp
        Tests/WebCore/ApduTests.cpp
        Tests/WebCore/ApplicationManifestParserTests.cpp
        Tests/WebCore/ASN1Utilities.cpp
        Tests/WebCore/CalculationValueTests.cpp
        Tests/WebCore/CARingBufferTests.cpp
        Tests/WebCore/CBORReaderTests.cpp
        Tests/WebCore/CBORValueTests.cpp
        Tests/WebCore/CBORWriterTests.cpp
        Tests/WebCore/ColorTests.cpp
        Tests/WebCore/ComplexTextControllerTests.cpp
        Tests/WebCore/ContentExtensionsTests.cpp
        Tests/WebCore/ContextMenuActionTests.cpp
        Tests/WebCore/Cookies.cpp
        Tests/WebCore/CryptoDigestTests.cpp
        Tests/WebCore/CSSParserTests.cpp
        Tests/WebCore/CtapPinTests.cpp
        Tests/WebCore/CtapRequestTests.cpp
        Tests/WebCore/CtapResponseTests.cpp
        Tests/WebCore/CurlMultipartHandleTests.cpp
        Tests/WebCore/DecimalTests.cpp
        Tests/WebCore/DFACombinerTests.cpp
        Tests/WebCore/DFAMinimizerTests.cpp
        Tests/WebCore/DisplayListRecorderTests.cpp
        Tests/WebCore/DisplayListTests.cpp
        Tests/WebCore/DNSTests.cpp
        Tests/WebCore/DocumentOrderTests.cpp
        Tests/WebCore/FidoHidMessageTests.cpp
        Tests/WebCore/FidoTestDataTests.h
        Tests/WebCore/FileMonitorTests.cpp
        Tests/WebCore/FloatPointTests.cpp
        Tests/WebCore/FloatQuadTests.cpp
        Tests/WebCore/FloatRectTests.cpp
        Tests/WebCore/FloatSizeTests.cpp
        Tests/WebCore/FontCascadeTests.cpp
        Tests/WebCore/FontShadowTests.cpp
        Tests/WebCore/GridPositionTests.cpp
        Tests/WebCore/HTMLParserIdiomsTests.cpp
        Tests/WebCore/HTTPHeaderFieldTests.cpp
        Tests/WebCore/HTTPHeaderMapTests.cpp
        Tests/WebCore/HTTPParsersTests.cpp
        Tests/WebCore/HysteresisActivityTests.cpp
        Tests/WebCore/ImageBufferTests.cpp
        Tests/WebCore/IntPointTests.cpp
        Tests/WebCore/IntRectTests.cpp
        Tests/WebCore/IntSizeTests.cpp
        Tests/WebCore/IPAddressTests.cpp
        Tests/WebCore/ISOBoxTests.cpp
        Tests/WebCore/KeyedCodingTests.cpp
        Tests/WebCore/LayoutUnitTests.cpp
        Tests/WebCore/LineBreakingTests.mm
        Tests/WebCore/LoggingTests.cpp
        Tests/WebCore/LoginStatusTests.cpp
        Tests/WebCore/MarkedTextTests.cpp
        Tests/WebCore/MIMESnifferTests.cpp
        Tests/WebCore/MIMETypeRegistryTests.cpp
        Tests/WebCore/MonospaceFontTests.cpp
        Tests/WebCore/NowPlayingInfoTests.cpp
        Tests/WebCore/OpenSSLHelperTests.cpp
        Tests/WebCore/ParsedContentRangeTests.cpp
        Tests/WebCore/ParsedContentTypeTests.cpp
        Tests/WebCore/PlatformCAAnimationKeyPathTests.cpp
        Tests/WebCore/PrivateClickMeasurementTests.cpp
        Tests/WebCore/PublicSuffixTests.cpp
        Tests/WebCore/PushDatabaseTests.cpp
        Tests/WebCore/PushMessageCryptoTests.cpp
        Tests/WebCore/RegistrableDomainTests.cpp
        Tests/WebCore/RenderStyleChangeTests.cpp
        Tests/WebCore/RoundedRectTestsTests.cpp
        Tests/WebCore/RTCRtpSFrameTransformerTests.cpp
        Tests/WebCore/SampleMapTests.cpp
        Tests/WebCore/SecurityOriginTests.cpp
        Tests/WebCore/SerializedScriptValueTests.cpp
        Tests/WebCore/ServiceWorkerRoutinesTests.cpp
        Tests/WebCore/SharedBufferTest.cpp
        Tests/WebCore/SharedBufferTests.cpp
        Tests/WebCore/StringUtilitiesTests.mm
        Tests/WebCore/StringWithDirectionTests.cpp
        Tests/WebCore/STUNMessageParsingTest.cpp
        Tests/WebCore/SVGImageCastsTests.cpp
        Tests/WebCore/TextBoundariesTests.cpp
        Tests/WebCore/TextCodecTests.cpp
        Tests/WebCore/TimeRangesTests.cpp
        Tests/WebCore/TransformationMatrixTests.cpp
        Tests/WebCore/U2fCommandConstructorTests.cpp
        Tests/WebCore/URLParserTextEncodingTests.cpp
        Tests/WebCore/UserAgentQuirksTests.cpp
        Tests/WebCore/WritingModeTests.cpp
        Tests/WebCore/YouTubePluginReplacementTests.cpp
    )

    set(TestWebCore_LIBRARIES
        WebKit::gtest
    )
    set(TestWebCore_FRAMEWORKS
        JavaScriptCore
        PAL
        WTF
        WebCore
    )
    if (NOT USE_SYSTEM_MALLOC)
        list(APPEND TestWebCore_FRAMEWORKS bmalloc)
    endif ()

    set(TestWebCore_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
    )

    set(TestWebCore_DEFINITIONS
      WEBKIT_SRC_DIR="${CMAKE_SOURCE_DIR}"
    )

    WEBKIT_EXECUTABLE_DECLARE(TestWebCore)
    target_compile_definitions(TestWebCore PUBLIC TestWebCore_DEFINITIONS)
endif ()

# TestWebKitLegacy definitions
if (ENABLE_WEBKIT_LEGACY)
    set(TestWebKitLegacy_SOURCES
        TestsController.cpp
    )

    set(TestWebKitLegacy_LIBRARIES
        WebKit::gtest
    )

    set(TestWebKitLegacy_FRAMEWORKS
        JavaScriptCore
        PAL
        WTF
        WebCore
    )
    if (NOT USE_SYSTEM_MALLOC)
        list(APPEND TestWebKitLegacy_FRAMEWORKS bmalloc)
    endif ()
    if (NOT APPLE)
        list(APPEND TestWebKitLegacy_FRAMEWORKS WebKitLegacy)
    endif ()

    set(TestWebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
    )

    WEBKIT_EXECUTABLE_DECLARE(TestWebKitLegacy)
endif ()

# TestWebKit definitions
if (ENABLE_WEBKIT)
    set(TestWebKit_SOURCES
        Utilities.cpp

        Tests/WebKit/AboutBlankLoad.cpp
        Tests/WebKit/CanHandleRequest.cpp
        Tests/WebKit/DOMWindowExtensionBasic.cpp
        Tests/WebKit/DOMWindowExtensionNoCache.cpp
        Tests/WebKit/DidNotHandleKeyDown.cpp
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
        ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${PAL_FRAMEWORK_HEADERS_DIR}
        ${TESTWEBKITAPI_DIR}
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
        ${WebKit_FRAMEWORK_HEADERS_DIR}
    )

    set(TestWebKit_LIBRARIES
        TestWebKitAPIBase
        WebKit::gtest
    )
    set(TestWebKit_FRAMEWORKS
        WebKit
    )

    set(TestWebKit_DEPENDENCIES
        TestWebKitAPIInjectedBundle
    )

    add_library(TestWebKitAPIBase STATIC
        JavaScriptTest.cpp
        PlatformUtilities.cpp
        TestsController.cpp
    )
    target_compile_definitions(TestWebKitAPIBase PRIVATE BUILDING_TestWebKit)
    target_include_directories(TestWebKitAPIBase PRIVATE ${TestWebKit_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(TestWebKitAPIBase PRIVATE WebKit::WebKit WebKit::gtest)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIBase ${TestWebKitAPI_DISABLED_WARNINGS} -Wno-deprecated-declarations)
    endif ()

    set(TestWebKitAPIInjectedBundle_SOURCES
        InjectedBundleController.cpp
        InjectedBundleMain.cpp
        PlatformUtilities.cpp
        Utilities.cpp

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
    target_link_libraries(TestWebKitAPIInjectedBundle PRIVATE WebKit::WebKit WebKit::gtest)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIInjectedBundle ${TestWebKitAPI_DISABLED_WARNINGS})
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
