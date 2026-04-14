set(TESTWEBKITAPI_DIR "${TOOLS_DIR}/TestWebKitAPI")
set(TestWebKitAPI_DISABLED_WARNINGS
    -Wno-dangling-else
    -Wno-sign-compare
    -Wno-undef
    -Wno-unused-parameter
)

macro(WEBKIT_TEST _target)
    WEBKIT_EXECUTABLE(${_target})
    target_precompile_headers(${_target} PRIVATE ${TESTWEBKITAPI_DIR}/Helpers/TestWebKitAPIPrefix.h)
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
    Helpers/Counters.cpp
    Runner/TestsController.cpp
    Helpers/Utilities.cpp

    Tests/WTF/AtomString.cpp
    Tests/WTF/Base64.cpp
    Tests/WTF/BitSet.cpp
    Tests/WTF/BitVector.cpp
    Tests/WTF/BloomFilter.cpp
    Tests/WTF/BoxPtr.cpp
    Tests/WTF/BumpPointerAllocator.cpp
    Tests/WTF/CString.cpp
    Tests/WTF/CStringView.cpp
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
    Tests/WTF/GenerationalSet.cpp
    Tests/WTF/HashCountedSet.cpp
    Tests/WTF/HashMap.cpp
    Tests/WTF/HashSet.cpp
    Tests/WTF/Hasher.cpp
    Tests/WTF/HexNumber.cpp
    Tests/WTF/IndexSparseSet.cpp
    Tests/WTF/InlineMap.cpp
    Tests/WTF/Int128.cpp
    Tests/WTF/IntegerToStringConversion.cpp
    Tests/WTF/IntervalSet.cpp
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
    Tests/WTF/MemoryDump.cpp
    Tests/WTF/MetaAllocator.cpp
    Tests/WTF/MoveOnlyLifecycleLogger.cpp
    Tests/WTF/NakedPtr.cpp
    Tests/WTF/NativePromise.cpp
    Tests/WTF/NeverDestroyed.cpp
    Tests/WTF/OptionCountedSet.cpp
    Tests/WTF/OptionSet.cpp
    Tests/WTF/Packed.cpp
    Tests/WTF/PackedRef.cpp
    Tests/WTF/PackedRefPtr.cpp
    Tests/WTF/ParkingLot.cpp
    Tests/WTF/PreciseSum.cpp
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
    Tests/WTF/SegmentedVector.cpp
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
    Tests/WTF/TruncateFloat.cpp
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
    bmalloc
)

WEBKIT_EXECUTABLE_DECLARE(TestWTF)

# TestJavaScriptCore definitions
if (ENABLE_JAVASCRIPTCORE)
    set(TestJavaScriptCore_SOURCES
        Runner/TestsController.cpp

        Tests/JavaScriptCore/DisallowVMEntry.cpp
        Tests/JavaScriptCore/InspectorConsoleMessage.cpp
        Tests/JavaScriptCore/MarkedVector.cpp
        Tests/JavaScriptCore/PropertySlot.cpp
    )

    set(TestJavaScriptCore_LIBRARIES
        WebKit::gtest
    )
    set(TestJavaScriptCore_FRAMEWORKS
        JavaScriptCore
        WTF
        bmalloc
    )

    set(TestJavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES
        ${CMAKE_BINARY_DIR}
        ${TESTWEBKITAPI_DIR}
    )

    WEBKIT_EXECUTABLE_DECLARE(TestJavaScriptCore)
endif ()

# TestWebCore definitions
if (ENABLE_WEBCORE)
    set(TestWebCore_SOURCES
        Helpers/GraphicsTestUtilities.cpp
        Runner/TestsController.cpp
        Helpers/Utilities.cpp
        Helpers/WebCoreTestUtilities.cpp

        Tests/WebCore/AffineTransform.cpp
        Tests/WebCore/AudioSampleFormat.cpp
        Tests/WebCore/CSSParser.cpp
        Tests/WebCore/CSSTokenizerTests.cpp
        Tests/WebCore/ColorTests.cpp
        Tests/WebCore/ComplexTextController.cpp
        Tests/WebCore/ContextMenuAction.cpp
        Tests/WebCore/CookieJar.cpp
        Tests/WebCore/Decimal.cpp
        Tests/WebCore/FileMonitor.cpp
        Tests/WebCore/FloatPointTests.cpp
        Tests/WebCore/FloatRectTests.cpp
        Tests/WebCore/FloatSegmentTest.cpp
        Tests/WebCore/FloatSizeTests.cpp
        Tests/WebCore/GridPosition.cpp
        Tests/WebCore/HTMLParserIdioms.cpp
        Tests/WebCore/HTTPParsers.cpp
        Tests/WebCore/IDBSerializationTest.cpp
        Tests/WebCore/ImageBufferTests.cpp
        Tests/WebCore/IntPointTests.cpp
        Tests/WebCore/IntRectTests.cpp
        Tests/WebCore/IntSizeTests.cpp
        Tests/WebCore/KeyedCoding.cpp
        Tests/WebCore/LayoutRoundedRectTests.cpp
        Tests/WebCore/LayoutUnitTests.cpp
        Tests/WebCore/MIMESniffer.cpp
        Tests/WebCore/MIMETypeRegistry.cpp
        Tests/WebCore/MediaReorderQueue.cpp
        Tests/WebCore/MixedContentChecker.cpp
        Tests/WebCore/MonospaceFontTests.cpp
        Tests/WebCore/NowPlayingInfoTests.cpp
        Tests/WebCore/ParsedContentRange.cpp
        Tests/WebCore/PathTests.cpp
        Tests/WebCore/PixelBufferConversionTests.cpp
        Tests/WebCore/PlatformDynamicRangeLimitTests.cpp
        Tests/WebCore/PublicSuffix.cpp
        Tests/WebCore/RegionTests.cpp
        Tests/WebCore/RenderStyleChange.cpp
        Tests/WebCore/SecurityOrigin.cpp
        Tests/WebCore/ShareableBitmap.cpp
        Tests/WebCore/SharedBuffer.cpp
        Tests/WebCore/SharedBufferTest.cpp
        Tests/WebCore/StyleGradient.cpp
        Tests/WebCore/TimeRanges.cpp
        Tests/WebCore/TransformationMatrix.cpp
        Tests/WebCore/URLParserTextEncoding.cpp
        Tests/WebCore/WritingModeTests.cpp
    )

    if (USE_SKIA)
        list(APPEND TestWebCore_SOURCES
            Tests/WebCore/skia/SkiaTextureAtlasPackerTests.cpp
        )
    endif ()

    set(TestWebCore_LIBRARIES
        WebKit::gtest
    )
    set(TestWebCore_FRAMEWORKS
        JavaScriptCore
        PAL
        WTF
        WebCore
        bmalloc
    )

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
        Runner/TestsController.cpp
    )

    set(TestWebKitLegacy_LIBRARIES
        WebKit::gtest
    )

    set(TestWebKitLegacy_FRAMEWORKS
        JavaScriptCore
        PAL
        WTF
        WebCore
        bmalloc
    )
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
        Helpers/Utilities.cpp

        Tests/WebKit/WKPage/AboutBlankLoad.cpp
        Tests/WebKit/WKPage/CanHandleRequest.cpp
        Tests/WebKit/WKPage/DOMWindowExtensionBasic.cpp
        Tests/WebKit/WKPage/DOMWindowExtensionNoCache.cpp
        Tests/WebKit/WKPage/DidNotHandleKeyDown.cpp
        Tests/WebKit/WKPage/DocumentStartUserScriptAlertCrash.cpp
        Tests/WebKit/WKPage/DownloadDecideDestinationCrash.cpp
        Tests/WebKit/WKPage/EvaluateJavaScript.cpp
        Tests/WebKit/WKPage/FailedLoad.cpp
        Tests/WebKit/WKPage/Find.cpp
        Tests/WebKit/WKPage/FirstMeaningfulPaintMilestone.cpp
        Tests/WebKit/WKPage/ForceRepaint.cpp
        Tests/WebKit/WKPage/FrameMIMETypeHTML.cpp
        Tests/WebKit/WKPage/FrameMIMETypePNG.cpp
        Tests/WebKit/WKPage/Geolocation.cpp
        Tests/WebKit/WKPage/GetInjectedBundleInitializationUserDataCallback.cpp
        Tests/WebKit/WKPage/HitTestResultNodeHandle.cpp
        Tests/WebKit/WKPage/InjectedBundleBasic.cpp
        Tests/WebKit/WKPage/InjectedBundleFrameHitTest.cpp
        Tests/WebKit/WKPage/InjectedBundleInitializationUserDataCallbackWins.cpp
        Tests/WebKit/WKPage/LoadAlternateHTMLStringWithNonDirectoryURL.cpp
        Tests/WebKit/WKPage/LoadCanceledNoServerRedirectCallback.cpp
        Tests/WebKit/WKPage/LoadPageOnCrash.cpp
        Tests/WebKit/WKPage/MouseMoveAfterCrash.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayout.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutFails.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutForImages.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutFrames.cpp
        Tests/WebKit/WKPage/PageLoadBasic.cpp
        Tests/WebKit/WKPage/PageLoadDidChangeLocationWithinPageForFrame.cpp
        Tests/WebKit/WKPage/PageLoadEmptyURL.cpp
        Tests/WebKit/WKPage/PageLoadState.cpp
        Tests/WebKit/WKPage/ParentFrame.cpp
        Tests/WebKit/WKPage/PendingAPIRequestURL.cpp
        Tests/WebKit/WKPage/PreventEmptyUserAgent.cpp
        Tests/WebKit/WKPage/PrivateBrowsingPushStateNoHistoryCallback.cpp
        Tests/WebKit/WKPage/ProvisionalURLAfterWillSendRequestCallback.cpp
        Tests/WebKit/WKPage/ReloadPageAfterCrash.cpp
        Tests/WebKit/WKPage/ResizeWindowAfterCrash.cpp
        Tests/WebKit/WKPage/RestoreSessionStateContainingFormData.cpp
        Tests/WebKit/WKPage/TextFieldDidBeginAndEndEditing.cpp
        Tests/WebKit/WKPage/UserMedia.cpp
        Tests/WebKit/WKPage/UserMessage.cpp
        Tests/WebKit/WKPage/WKPageCopySessionStateWithFiltering.cpp
        Tests/WebKit/WKPage/WKPageGetScaleFactorNotZero.cpp
        Tests/WebKit/WKPage/WKPreferences.cpp
        Tests/WebKit/WKPage/WKRetainPtr.cpp
        Tests/WebKit/WKPage/WKString.cpp
        Tests/WebKit/WKPage/WKStringJSString.cpp
        Tests/WebKit/WKPage/WKURL.cpp
        Tests/WebKit/WKPage/WillSendSubmitEvent.cpp
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
        Helpers/JavaScriptTest.cpp
        Helpers/PlatformUtilities.cpp
        Runner/TestsController.cpp
    )
    target_compile_definitions(TestWebKitAPIBase PRIVATE BUILDING_TestWebKit)
    target_include_directories(TestWebKitAPIBase PRIVATE ${TestWebKit_PRIVATE_INCLUDE_DIRECTORIES})
    target_link_libraries(TestWebKitAPIBase PRIVATE WebKit::WebKit WebKit::gtest)

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(TestWebKitAPIBase ${TestWebKitAPI_DISABLED_WARNINGS} -Wno-deprecated-declarations)
    endif ()

    set(TestWebKitAPIInjectedBundle_SOURCES
        InjectedBundle/InjectedBundleController.cpp
        InjectedBundle/InjectedBundleMain.cpp
        Helpers/PlatformUtilities.cpp
        Helpers/Utilities.cpp

        Tests/WebKit/WKPage/CanHandleRequest_Bundle.cpp
        Tests/WebKit/WKPage/DidAssociateFormControls_Bundle.cpp
        Tests/WebKit/WKPage/DOMWindowExtensionBasic_Bundle.cpp
        Tests/WebKit/WKPage/DOMWindowExtensionNoCache_Bundle.cpp
        Tests/WebKit/WKPage/DocumentStartUserScriptAlertCrash_Bundle.cpp
        Tests/WebKit/WKPage/GetInjectedBundleInitializationUserDataCallback_Bundle.cpp
        Tests/WebKit/WKPage/HitTestResultNodeHandle_Bundle.cpp
        Tests/WebKit/WKPage/InjectedBundleBasic_Bundle.cpp
        Tests/WebKit/WKPage/InjectedBundleFrameHitTest_Bundle.cpp
        Tests/WebKit/WKPage/InjectedBundleInitializationUserDataCallbackWins_Bundle.cpp
        Tests/WebKit/WKPage/LoadCanceledNoServerRedirectCallback_Bundle.cpp
        Tests/WebKit/WKPage/MouseMoveAfterCrash_Bundle.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutFails_Bundle.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutForImages_Bundle.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayoutFrames_Bundle.cpp
        Tests/WebKit/WKPage/NewFirstVisuallyNonEmptyLayout_Bundle.cpp
        Tests/WebKit/WKPage/FirstMeaningfulPaintMilestone_Bundle.cpp
        Tests/WebKit/WKPage/ParentFrame_Bundle.cpp
        Tests/WebKit/WKPage/ProvisionalURLAfterWillSendRequestCallback_Bundle.cpp
        Tests/WebKit/WKPage/ResponsivenessTimerDoesntFireEarly_Bundle.cpp
        Tests/WebKit/WKPage/TextFieldDidBeginAndEndEditing_Bundle.cpp
        Tests/WebKit/WKPage/UserMessage_Bundle.cpp
        Tests/WebKit/WKPage/WillLoad_Bundle.cpp
        Tests/WebKit/WKPage/WillSendSubmitEvent_Bundle.cpp
    )
    if (APPLE)
        list(APPEND TestWebKitAPIInjectedBundle_SOURCES
            Tests/WebKit/WKPage/DOMWindowExtensionBasic_Bundle.cpp

            Helpers/cocoa/UtilitiesCocoa.mm

            InjectedBundle/mac/InjectedBundleControllerMac.mm
            Helpers/mac/PlatformUtilitiesMac.mm
        )
    endif ()
    # macOS: MODULE + BUNDLE TRUE (PlatformMac) emits a CFBundle matching Xcode's MH_BUNDLE.
    # Other ports: SHARED dylib/so.
    if (APPLE)
        set(TestWebKitAPIInjectedBundle_LIBRARY_TYPE MODULE)
    else ()
        set(TestWebKitAPIInjectedBundle_LIBRARY_TYPE SHARED)
    endif ()
    add_library(TestWebKitAPIInjectedBundle ${TestWebKitAPIInjectedBundle_LIBRARY_TYPE} ${TestWebKitAPIInjectedBundle_SOURCES})

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
