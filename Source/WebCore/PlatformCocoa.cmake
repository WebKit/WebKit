set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebCore)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(WebCore PROPERTIES
        INSTALL_NAME_DIR "${WebCore_INSTALL_NAME_DIR}"
    )
    target_link_options(WebCore PRIVATE
        -compatibility_version 1.0.0
        -current_version ${WEBKIT_MAC_VERSION}
    )
endif ()

set(WebCore_POST_BUILD_COMMAND
    codesign --force --sign - ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework
)

make_directory("${CMAKE_BINARY_DIR}/WebCore/Modules")
configure_file(${WEBCORE_DIR}/WebCore.modulemap ${CMAKE_BINARY_DIR}/WebCore/Modules/module.modulemap COPYONLY)
set(_webcore_fw "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebCore.framework")
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    make_directory("${_webcore_fw}")
    if (NOT EXISTS "${_webcore_fw}/PrivateHeaders")
        file(CREATE_LINK "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore"
                         "${_webcore_fw}/PrivateHeaders" SYMBOLIC)
    endif ()
    if (NOT EXISTS "${_webcore_fw}/Modules")
        file(CREATE_LINK "${CMAKE_BINARY_DIR}/WebCore/Modules"
                         "${_webcore_fw}/Modules" SYMBOLIC)
    endif ()
else ()
    make_directory("${_webcore_fw}/Versions/A")
    if (NOT EXISTS "${_webcore_fw}/Versions/Current")
        file(CREATE_LINK "A" "${_webcore_fw}/Versions/Current" SYMBOLIC)
    endif ()
    if (NOT EXISTS "${_webcore_fw}/Versions/A/PrivateHeaders")
        file(CREATE_LINK "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore"
                         "${_webcore_fw}/Versions/A/PrivateHeaders" SYMBOLIC)
    endif ()
    if (NOT EXISTS "${_webcore_fw}/Versions/A/Modules")
        file(CREATE_LINK "${CMAKE_BINARY_DIR}/WebCore/Modules"
                         "${_webcore_fw}/Versions/A/Modules" SYMBOLIC)
    endif ()
    if (NOT EXISTS "${_webcore_fw}/PrivateHeaders")
        file(CREATE_LINK "Versions/Current/PrivateHeaders"
                         "${_webcore_fw}/PrivateHeaders" SYMBOLIC)
    endif ()
    if (NOT EXISTS "${_webcore_fw}/Modules")
        file(CREATE_LINK "Versions/Current/Modules"
                         "${_webcore_fw}/Modules" SYMBOLIC)
    endif ()
endif ()
unset(_webcore_fw)

target_compile_options(WebCore PRIVATE
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/WebCorePrefix.h>")

target_compile_options(WebCore PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})

target_link_options(WebCore PRIVATE -weak_framework BrowserEngineKit)

target_link_options(WebCore PRIVATE
    -Wl,-unexported_symbols_list,${WEBCORE_DIR}/Configurations/WebCore.unexp
)

find_library(ACCELERATE_LIBRARY Accelerate)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COMPRESSION_LIBRARY Compression)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(COREMEDIA_LIBRARY CoreMedia)
find_library(FONTPARSER_LIBRARY FontParser HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks/FontServices.framework)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(LIBACCESSIBILITY_LIBRARY Accessibility PATHS ${CMAKE_OSX_SYSROOT}/usr/lib NO_CMAKE_FIND_ROOT_PATH NO_DEFAULT_PATH)
find_library(ACCESSIBILITYSUPPORT_LIBRARY AccessibilitySupport HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(METAL_LIBRARY Metal)
find_library(NETWORKEXTENSION_LIBRARY NetworkExtension)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(SECURITY_LIBRARY Security)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)
find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
find_library(VIDEOTOOLBOX_LIBRARY VideoToolbox)
find_library(XML2_LIBRARY XML2)

# SQLite3::SQLite3 and ZLIB::ZLIB are declared in OptionsCocoa.cmake; only search
# if missing (e.g. ANGLE/WebCore configured standalone).
if (NOT TARGET SQLite3::SQLite3)
    find_package(SQLite3 REQUIRED)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif ()
if (NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)
# FIXME: Test building on iOS and then enable on iOS.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
        "SourcesCMakeCocoa.txt"
    )
endif ()

list(APPEND WebCore_LIBRARIES
    ${ACCELERATE_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AVFOUNDATION_LIBRARY}
    ${CFNETWORK_LIBRARY}
    ${COMPRESSION_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${COREMEDIA_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${LIBACCESSIBILITY_LIBRARY}
    ${METAL_LIBRARY}
    ${NETWORKEXTENSION_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARIES}
    ${SYSTEMCONFIGURATION_LIBRARY}
    ${UNIFORMTYPEIDENTIFIERS_LIBRARY}
    ${VIDEOTOOLBOX_LIBRARY}
    ${XML2_LIBRARY}
)

if (ACCESSIBILITYSUPPORT_LIBRARY)
    list(APPEND WebCore_LIBRARIES ${ACCESSIBILITYSUPPORT_LIBRARY})
endif ()

if (USE_LIBWEBRTC)
    list(APPEND WebCore_PRIVATE_LIBRARIES webrtc opus vpx webm yuv libsrtp webrtc_objc_categories)
else ()
    set(_webm_parser_dir "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/libwebm/webm_parser")
    file(GLOB _webm_parser_srcs "${_webm_parser_dir}/src/*.cc")
    add_library(WebMParser OBJECT ${_webm_parser_srcs})
    target_include_directories(WebMParser PRIVATE "${_webm_parser_dir}/include" "${_webm_parser_dir}")
    target_compile_definitions(WebMParser PRIVATE WEBRTC_WEBKIT_BUILD)
    target_compile_options(WebMParser PRIVATE -w)
    list(APPEND WebCore_PRIVATE_LIBRARIES WebMParser)
    unset(_webm_parser_dir)
    unset(_webm_parser_srcs)
endif ()

if (ENABLE_AV1)
    list(APPEND WebCore_PRIVATE_LIBRARIES dav1d)
endif ()

if (NOT ENABLE_WEBGPU)
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        list(APPEND WebCore_PRIVATE_LIBRARIES "-Wl,-undefined,dynamic_lookup")
    endif ()
else ()
    list(APPEND WebCore_LIBRARIES "$<TARGET_LINKER_FILE:WebGPU>")
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/WebGPU/Headers")
endif ()

set(WebCore_EXTRA_LINK_OPTIONS "SHELL:-Wl,-force_load $<TARGET_FILE:PAL>")

find_library(COREUI_FRAMEWORK CoreUI HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (COREUI_FRAMEWORK)
    list(APPEND WebCore_LIBRARIES ${COREUI_FRAMEWORK})
endif ()

find_library(DATADETECTORSCORE_FRAMEWORK DataDetectorsCore HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (DATADETECTORSCORE_FRAMEWORK)
    list(APPEND WebCore_LIBRARIES ${DATADETECTORSCORE_FRAMEWORK})
endif ()

set(_libwebrtc_fwd "${CMAKE_BINARY_DIR}/libwebrtc-forwarding")
file(MAKE_DIRECTORY "${_libwebrtc_fwd}/libwebrtc")
foreach (_h opus_defines.h opus_types.h)
    if (NOT EXISTS "${_libwebrtc_fwd}/libwebrtc/${_h}")
        file(CREATE_LINK
            "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/opus/src/include/${_h}"
            "${_libwebrtc_fwd}/libwebrtc/${_h}"
            SYMBOLIC)
    endif ()
endforeach ()
if (NOT EXISTS "${_libwebrtc_fwd}/webm")
    file(CREATE_LINK
        "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/libwebm"
        "${_libwebrtc_fwd}/webm"
        SYMBOLIC)
endif ()
unset(_libwebrtc_fwd)
unset(_h)

list(APPEND WebCore_PRIVATE_DEFINITIONS WEBRTC_WEBKIT_BUILD)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source/WebCore/accessibility/cocoa"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/Modules/ShapeDetection/Implementation/Cocoa"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/Modules/speech/cocoa"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/platform/video-codecs/cocoa"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/platform/xr/cocoa"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/platform/mediastream"
    "${CMAKE_SOURCE_DIR}/Source/WebCore/platform/mediastream/cocoa"
    "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/libwebm/webm_parser/include"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source/third_party/libwebm"
    "${CMAKE_BINARY_DIR}/libwebrtc-forwarding"
    "${WEBCORE_DIR}/Modules/applepay-ams-ui"
    "${WEBCORE_DIR}/Modules/webauthn/apdu"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/crypto/cocoa"
    "${WEBCORE_DIR}/crypto/mac"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/html/shadow/cocoa"
    "${WEBCORE_DIR}/layout/tableformatting"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/cocoa"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/scrolling/cocoa"
    "${WEBCORE_DIR}/page/writing-tools"
    "${WEBCORE_DIR}/platform/audio/cocoa"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/gamepad/cocoa"
    "${WEBCORE_DIR}/platform/graphics/angle"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa/controls"
    "${WEBCORE_DIR}/platform/graphics/coreimage"
    "${WEBCORE_DIR}/platform/graphics/coretext"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/cv"
    "${WEBCORE_DIR}/platform/graphics/gpu"
    "${WEBCORE_DIR}/platform/graphics/gpu/cocoa"
    "${WEBCORE_DIR}/platform/graphics/gpu/legacy"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/re"
    "${WEBCORE_DIR}/platform/image-decoders"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediarecorder/cocoa"
    "${WEBCORE_DIR}/platform/mediastream/cocoa"
    "${WEBCORE_DIR}/platform/mediastream/libwebrtc"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/text/cf"
    "${WEBCORE_DIR}/platform/text/cocoa"
    "${WEBCORE_DIR}/platform/spi/cf"
    "${WEBCORE_DIR}/platform/spi/cg"
    "${WEBCORE_DIR}/platform/spi/cocoa"
    "${WEBCORE_DIR}/platform/video-codecs"
    "${WEBCORE_DIR}/rendering/cocoa"
    "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}"
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    "${CMAKE_OSX_SYSROOT}/usr/include/libxslt"
    "${CMAKE_OSX_SYSROOT}/usr/include/libxml2"
)

list(APPEND WebCore_SOURCES
    Modules/geolocation/cocoa/GeolocationPositionDataCocoa.mm

    Modules/paymentrequest/MerchantValidationEvent.cpp

    Modules/webaudio/MediaStreamAudioSourceCocoa.cpp

    Modules/webauthn/AuthenticationExtensionsClientInputs.cpp
    Modules/webauthn/AuthenticationExtensionsClientOutputs.cpp

    dom/SlotAssignment.cpp

    editing/TextListParser.cpp

    editing/cocoa/AlternativeTextUIController.mm
    editing/cocoa/AutofillElements.cpp

    editing/mac/UniversalAccessZoom.mm

    html/HTMLSlotElement.cpp

    html/shadow/SpatialImageControls.cpp

    loader/cocoa/PrivateClickMeasurementCocoa.mm

    page/ViewportConfiguration.cpp

    platform/CPUMonitor.cpp
    platform/DictationCaretAnimator.cpp
    platform/LocalizedStrings.cpp
    platform/OpacityCaretAnimator.cpp
    platform/ScrollableArea.cpp

    platform/audio/AudioSession.cpp

    platform/audio/cocoa/AudioBusCocoa.mm
    platform/audio/cocoa/AudioDecoderCocoa.cpp
    platform/audio/cocoa/AudioEncoderCocoa.cpp
    platform/audio/cocoa/AudioSessionCocoa.mm
    platform/audio/cocoa/FFTFrameCocoa.cpp
    platform/audio/cocoa/WebAudioBufferList.cpp

    platform/cf/KeyedDecoderCF.cpp
    platform/cf/KeyedEncoderCF.cpp
    platform/cf/MainThreadSharedTimerCF.cpp
    platform/cf/MediaAccessibilitySoftLink.cpp
    platform/cf/SharedBufferCF.cpp

    platform/cocoa/ContentFilterUnblockHandlerCocoa.mm
    platform/cocoa/CoreVideoSoftLink.cpp
    platform/cocoa/FileMonitorCocoa.mm
    platform/cocoa/KeyEventCocoa.mm
    platform/cocoa/LocalizedStringsCocoa.mm
    platform/cocoa/LoggingCocoa.mm
    platform/cocoa/MIMETypeRegistryCocoa.mm
    platform/cocoa/MediaRemoteSoftLink.mm
    platform/cocoa/NetworkExtensionContentFilter.mm
    platform/cocoa/ParentalControlsContentFilter.mm
    platform/cocoa/PasteboardCocoa.mm
    platform/cocoa/SearchPopupMenuCocoa.mm
    platform/cocoa/SharedBufferCocoa.mm
    platform/cocoa/SharedMemoryCocoa.mm
    platform/cocoa/SharedVideoFrameInfo.mm
    platform/cocoa/StringUtilities.mm
    platform/cocoa/SystemBattery.mm
    platform/cocoa/SystemVersion.mm
    platform/cocoa/TelephoneNumberDetectorCocoa.cpp
    platform/cocoa/ThemeCocoa.mm
    platform/cocoa/VideoFullscreenCaptions.mm
    platform/cocoa/VideoToolboxSoftLink.cpp
    platform/cocoa/WebAVPlayerLayer.mm
    platform/cocoa/WebCoreNSErrorExtras.mm
    platform/cocoa/WebCoreNSURLExtras.mm
    platform/cocoa/WebCoreObjCExtras.mm
    platform/cocoa/WebNSAttributedStringExtras.mm

    platform/gamepad/cocoa/CoreHapticsSoftLink.mm
    platform/gamepad/cocoa/GameControllerHapticEffect.mm
    platform/gamepad/cocoa/GameControllerHapticEngines.mm
    platform/gamepad/cocoa/GameControllerSoftLink.mm

    platform/graphics/DisplayRefreshMonitor.cpp
    platform/graphics/DisplayRefreshMonitorManager.cpp
    platform/graphics/FourCC.cpp

    platform/graphics/avfoundation/AVTrackPrivateAVFObjCImpl.mm
    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.mm
    platform/graphics/avfoundation/CDMFairPlayStreaming.cpp
    platform/graphics/avfoundation/InbandMetadataTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/InbandTextTrackPrivateAVF.cpp
    platform/graphics/avfoundation/LegacyCDMPrivateAVFObjC.mm
    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.mm
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.cpp
    platform/graphics/avfoundation/MediaSelectionGroupAVFObjC.mm
    platform/graphics/avfoundation/WebAVSampleBufferListener.mm

    platform/graphics/avfoundation/objc/AVAssetTrackUtilities.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/AudioTrackPrivateMediaSourceAVFObjC.cpp
    platform/graphics/avfoundation/objc/CDMInstanceFairPlayStreamingAVFObjC.mm
    platform/graphics/avfoundation/objc/CDMSessionAVContentKeySession.mm
    platform/graphics/avfoundation/objc/CDMSessionAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.mm
    platform/graphics/avfoundation/objc/InbandTextTrackPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateAVFoundationObjC.mm
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSampleAVFObjC.mm
    platform/graphics/avfoundation/objc/MediaSourcePrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/QueuedVideoOutput.mm
    platform/graphics/avfoundation/objc/SourceBufferPrivateAVFObjC.mm
    platform/graphics/avfoundation/objc/VideoTrackPrivateAVFObjC.cpp
    platform/graphics/avfoundation/objc/VideoTrackPrivateMediaSourceAVFObjC.mm
    platform/graphics/avfoundation/objc/WebCoreAVFResourceLoader.mm

    platform/graphics/ca/GraphicsLayerCA.cpp
    platform/graphics/ca/LayerPool.cpp
    platform/graphics/ca/PlatformCAAnimation.cpp
    platform/graphics/ca/PlatformCALayer.mm
    platform/graphics/ca/TileController.cpp
    platform/graphics/ca/TileCoverageMap.cpp
    platform/graphics/ca/TileGrid.cpp
    platform/graphics/ca/TransformationMatrixCA.cpp

    platform/graphics/ca/cocoa/GraphicsLayerAsyncContentsDisplayDelegateCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.mm
    platform/graphics/ca/cocoa/PlatformCAFiltersCocoa.mm
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.mm
    platform/graphics/ca/cocoa/PlatformDynamicRangeLimitCocoa.mm
    platform/graphics/ca/cocoa/WebSystemBackdropLayer.mm
    platform/graphics/ca/cocoa/WebTiledBackingLayer.mm

    platform/graphics/cg/CGSubimageCacheWithTimer.cpp
    platform/graphics/cg/ColorCG.cpp
    platform/graphics/cg/ColorSpaceCG.cpp
    platform/graphics/cg/FloatPointCG.cpp
    platform/graphics/cg/FloatRectCG.cpp
    platform/graphics/cg/FloatSizeCG.cpp
    platform/graphics/cg/GradientCG.cpp
    platform/graphics/cg/GradientRendererCG.cpp
    platform/graphics/cg/GraphicsContextCG.cpp
    platform/graphics/cg/GraphicsContextGLCG.cpp
    platform/graphics/cg/IOSurfacePool.cpp
    platform/graphics/cg/ImageBufferCGBackend.cpp
    platform/graphics/cg/ImageBufferCGBitmapBackend.cpp
    platform/graphics/cg/ImageBufferIOSurfaceBackend.cpp
    platform/graphics/cg/ImageDecoderCG.cpp
    platform/graphics/cg/IntPointCG.cpp
    platform/graphics/cg/IntRectCG.cpp
    platform/graphics/cg/IntSizeCG.cpp
    platform/graphics/cg/NativeImageCG.cpp
    platform/graphics/cg/PDFDocumentImage.cpp
    platform/graphics/cg/PathCG.cpp
    platform/graphics/cg/PatternCG.cpp
    platform/graphics/cg/ShareableSpatialImage.h
    platform/graphics/cg/SpatialImageTypes.h
    platform/graphics/cg/TransformationMatrixCG.cpp
    platform/graphics/cg/UTIRegistry.mm

    platform/graphics/cocoa/ANGLEUtilitiesCocoa.mm
    platform/graphics/cocoa/CMUtilities.mm
    platform/graphics/cocoa/FloatRectCocoa.mm
    platform/graphics/cocoa/FontCacheCoreText.cpp
    platform/graphics/cocoa/FontCocoa.cpp
    platform/graphics/cocoa/FontDatabase.cpp
    platform/graphics/cocoa/FontDescriptionCocoa.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreText.cpp
    platform/graphics/cocoa/FontFamilySpecificationCoreTextCache.cpp
    platform/graphics/cocoa/FontPlatformDataCocoa.mm
    platform/graphics/cocoa/GraphicsContextCocoa.mm
    platform/graphics/cocoa/GraphicsContextGLCocoa.mm
    platform/graphics/cocoa/IOSurface.mm
    platform/graphics/cocoa/IOSurfaceDrawingBuffer.cpp
    platform/graphics/cocoa/IOSurfacePoolCocoa.mm
    platform/graphics/cocoa/IntRectCocoa.mm
    platform/graphics/cocoa/MediaPlayerEnumsCocoa.mm
    platform/graphics/cocoa/TextTransformCocoa.cpp
    platform/graphics/cocoa/UnrealizedCoreTextFont.cpp
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.mm
    platform/graphics/cocoa/WebCoreCALayerExtras.mm
    platform/graphics/cocoa/WebCoreDecompressionSession.mm
    platform/graphics/cocoa/WebLayer.mm
    platform/graphics/cocoa/WebMAudioUtilitiesCocoa.mm
    platform/graphics/cocoa/WebProcessGraphicsContextGLCocoa.mm

    platform/graphics/coretext/ComplexTextControllerCoreText.mm
    platform/graphics/coretext/FontCascadeCoreText.cpp
    platform/graphics/coretext/FontCoreText.cpp
    platform/graphics/coretext/FontCustomPlatformDataCoreText.cpp
    platform/graphics/coretext/FontPlatformDataCoreText.cpp
    platform/graphics/coretext/GlyphPageCoreText.cpp
    platform/graphics/coretext/SimpleFontDataCoreText.cpp

    platform/graphics/cv/CVUtilities.mm
    platform/graphics/cv/GraphicsContextGLCVCocoa.mm
    platform/graphics/cv/ImageRotationSessionVT.mm
    platform/graphics/cv/PixelBufferConformerCV.cpp
    platform/graphics/cv/PixelBufferConformerCV.mm

    platform/graphics/opentype/OpenTypeCG.cpp
    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/mac/WebCoreView.mm

    platform/mediarecorder/MediaRecorderPrivateWriter.cpp

    platform/mediastream/cocoa/CoreAudioCaptureUnit.mm
    platform/mediastream/cocoa/MockRealtimeVideoSourceCocoa.mm
    platform/mediastream/cocoa/RealtimeOutgoingVideoSourceCocoa.cpp

    platform/mediastream/libwebrtc/LibWebRTCAudioModule.cpp
    platform/mediastream/libwebrtc/LibWebRTCDav1dDecoder.cpp

    platform/network/cf/CertificateInfoCFNet.cpp
    platform/network/cf/DNSResolveQueueCFNet.cpp
    platform/network/cf/FormDataStreamCFNet.mm
    platform/network/cf/NetworkStorageSessionCFNet.cpp
    platform/network/cf/ResourceRequestCFNet.cpp

    platform/network/cocoa/AuthenticationCocoa.mm
    platform/network/cocoa/BlobDataFileReferenceCocoa.mm
    platform/network/cocoa/CookieCocoa.mm
    platform/network/cocoa/CookieStorageCocoa.mm
    platform/network/cocoa/CookieStorageObserver.mm
    platform/network/cocoa/CredentialCocoa.mm
    platform/network/cocoa/CredentialStorageCocoa.mm
    platform/network/cocoa/FormDataStreamCocoa.mm
    platform/network/cocoa/NetworkLoadMetrics.mm
    platform/network/cocoa/NetworkStorageSessionCocoa.mm
    platform/network/cocoa/ProtectionSpaceCocoa.mm
    platform/network/cocoa/ResourceErrorCocoa.mm
    platform/network/cocoa/ResourceHandleCocoa.mm
    platform/network/cocoa/ResourceRequestCocoa.mm
    platform/network/cocoa/ResourceResponseCocoa.mm
    platform/network/cocoa/SynchronousLoaderClient.mm
    platform/network/cocoa/UTIUtilities.mm
    platform/network/cocoa/WebCoreNSURLSession.mm
    platform/network/cocoa/WebCoreResourceHandleAsOperationQueueDelegate.mm
    platform/network/cocoa/WebCoreURLResponse.mm

    platform/text/cf/HyphenationCF.cpp

    platform/text/cocoa/LocaleCocoa.mm
    platform/text/cocoa/TextBoundaries.mm

    rendering/TextAutoSizing.cpp

    rendering/cocoa/RenderThemeCocoa.mm

    testing/MockContentFilter.cpp
    testing/MockContentFilterManager.cpp
    testing/MockContentFilterSettings.cpp
    testing/MockParentalControlsURLFilter.mm

    workers/service/ServiceWorkerRoute.mm
)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

list(REMOVE_ITEM WebCore_PRIVATE_FRAMEWORK_HEADERS
    ${WebCore_DERIVED_SOURCES_DIR}/CSSPropertyParsing.h
    ${WebCore_DERIVED_SOURCES_DIR}/CSSSelectorInlines.h
    ${WebCore_DERIVED_SOURCES_DIR}/JSReadableStreamDefaultController.h
    ${WebCore_DERIVED_SOURCES_DIR}/UserAgentParts.h
    ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h
    ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltinInternals.h

    Modules/WebGPU/GPUAdapter.h
    Modules/WebGPU/GPUAdapterInfo.h
    Modules/WebGPU/GPUAddressMode.h
    Modules/WebGPU/GPUAutoLayoutMode.h
    Modules/WebGPU/GPUBindGroup.h
    Modules/WebGPU/GPUBindGroupDescriptor.h
    Modules/WebGPU/GPUBindGroupEntry.h
    Modules/WebGPU/GPUBindGroupLayout.h
    Modules/WebGPU/GPUBindGroupLayoutDescriptor.h
    Modules/WebGPU/GPUBindGroupLayoutEntry.h
    Modules/WebGPU/GPUBlendComponent.h
    Modules/WebGPU/GPUBlendFactor.h
    Modules/WebGPU/GPUBlendOperation.h
    Modules/WebGPU/GPUBlendState.h
    Modules/WebGPU/GPUBuffer.h
    Modules/WebGPU/GPUBufferBinding.h
    Modules/WebGPU/GPUBufferBindingLayout.h
    Modules/WebGPU/GPUBufferBindingType.h
    Modules/WebGPU/GPUBufferDescriptor.h
    Modules/WebGPU/GPUBufferMapState.h
    Modules/WebGPU/GPUBufferUsage.h
    Modules/WebGPU/GPUCanvasAlphaMode.h
    Modules/WebGPU/GPUCanvasConfiguration.h
    Modules/WebGPU/GPUCanvasToneMapping.h
    Modules/WebGPU/GPUCanvasToneMappingMode.h
    Modules/WebGPU/GPUColorDict.h
    Modules/WebGPU/GPUColorTargetState.h
    Modules/WebGPU/GPUColorWrite.h
    Modules/WebGPU/GPUCommandBuffer.h
    Modules/WebGPU/GPUCommandBufferDescriptor.h
    Modules/WebGPU/GPUCommandEncoder.h
    Modules/WebGPU/GPUCommandEncoderDescriptor.h
    Modules/WebGPU/GPUCompareFunction.h
    Modules/WebGPU/GPUCompilationInfo.h
    Modules/WebGPU/GPUCompilationMessage.h
    Modules/WebGPU/GPUCompilationMessageType.h
    Modules/WebGPU/GPUComputePassDescriptor.h
    Modules/WebGPU/GPUComputePassEncoder.h
    Modules/WebGPU/GPUComputePassTimestampWrites.h
    Modules/WebGPU/GPUComputePipeline.h
    Modules/WebGPU/GPUComputePipelineDescriptor.h
    Modules/WebGPU/GPUCullMode.h
    Modules/WebGPU/GPUDepthStencilState.h
    Modules/WebGPU/GPUDevice.h
    Modules/WebGPU/GPUDeviceDescriptor.h
    Modules/WebGPU/GPUDeviceLostInfo.h
    Modules/WebGPU/GPUDeviceLostReason.h
    Modules/WebGPU/GPUError.h
    Modules/WebGPU/GPUErrorFilter.h
    Modules/WebGPU/GPUExtent3DDict.h
    Modules/WebGPU/GPUExternalTexture.h
    Modules/WebGPU/GPUExternalTextureBindingLayout.h
    Modules/WebGPU/GPUExternalTextureDescriptor.h
    Modules/WebGPU/GPUFeatureName.h
    Modules/WebGPU/GPUFilterMode.h
    Modules/WebGPU/GPUFragmentState.h
    Modules/WebGPU/GPUFrontFace.h
    Modules/WebGPU/GPUImageCopyBuffer.h
    Modules/WebGPU/GPUImageCopyExternalImage.h
    Modules/WebGPU/GPUImageCopyTexture.h
    Modules/WebGPU/GPUImageCopyTextureTagged.h
    Modules/WebGPU/GPUImageDataLayout.h
    Modules/WebGPU/GPUIndexFormat.h
    Modules/WebGPU/GPULoadOp.h
    Modules/WebGPU/GPUMapMode.h
    Modules/WebGPU/GPUMultisampleState.h
    Modules/WebGPU/GPUObjectDescriptorBase.h
    Modules/WebGPU/GPUOrigin2DDict.h
    Modules/WebGPU/GPUOrigin3DDict.h
    Modules/WebGPU/GPUOutOfMemoryError.h
    Modules/WebGPU/GPUPipelineDescriptorBase.h
    Modules/WebGPU/GPUPipelineError.h
    Modules/WebGPU/GPUPipelineErrorInit.h
    Modules/WebGPU/GPUPipelineErrorReason.h
    Modules/WebGPU/GPUPipelineLayout.h
    Modules/WebGPU/GPUPipelineLayoutDescriptor.h
    Modules/WebGPU/GPUPowerPreference.h
    Modules/WebGPU/GPUPrimitiveState.h
    Modules/WebGPU/GPUPrimitiveTopology.h
    Modules/WebGPU/GPUProgrammableStage.h
    Modules/WebGPU/GPUQuerySet.h
    Modules/WebGPU/GPUQuerySetDescriptor.h
    Modules/WebGPU/GPUQueryType.h
    Modules/WebGPU/GPUQueue.h
    Modules/WebGPU/GPURenderBundle.h
    Modules/WebGPU/GPURenderBundleDescriptor.h
    Modules/WebGPU/GPURenderBundleEncoder.h
    Modules/WebGPU/GPURenderBundleEncoderDescriptor.h
    Modules/WebGPU/GPURenderPassColorAttachment.h
    Modules/WebGPU/GPURenderPassDepthStencilAttachment.h
    Modules/WebGPU/GPURenderPassDescriptor.h
    Modules/WebGPU/GPURenderPassEncoder.h
    Modules/WebGPU/GPURenderPassLayout.h
    Modules/WebGPU/GPURenderPassTimestampWrites.h
    Modules/WebGPU/GPURenderPipeline.h
    Modules/WebGPU/GPURenderPipelineDescriptor.h
    Modules/WebGPU/GPURequestAdapterOptions.h
    Modules/WebGPU/GPUSampler.h
    Modules/WebGPU/GPUSamplerBindingLayout.h
    Modules/WebGPU/GPUSamplerBindingType.h
    Modules/WebGPU/GPUSamplerDescriptor.h
    Modules/WebGPU/GPUShaderModule.h
    Modules/WebGPU/GPUShaderModuleCompilationHint.h
    Modules/WebGPU/GPUShaderModuleDescriptor.h
    Modules/WebGPU/GPUShaderStage.h
    Modules/WebGPU/GPUStencilFaceState.h
    Modules/WebGPU/GPUStencilOperation.h
    Modules/WebGPU/GPUStorageTextureAccess.h
    Modules/WebGPU/GPUStorageTextureBindingLayout.h
    Modules/WebGPU/GPUStoreOp.h
    Modules/WebGPU/GPUSupportedFeatures.h
    Modules/WebGPU/GPUSupportedLimits.h
    Modules/WebGPU/GPUTexture.h
    Modules/WebGPU/GPUTextureAspect.h
    Modules/WebGPU/GPUTextureBindingLayout.h
    Modules/WebGPU/GPUTextureDescriptor.h
    Modules/WebGPU/GPUTextureDimension.h
    Modules/WebGPU/GPUTextureSampleType.h
    Modules/WebGPU/GPUTextureView.h
    Modules/WebGPU/GPUTextureViewDescriptor.h
    Modules/WebGPU/GPUTextureViewDimension.h
    Modules/WebGPU/GPUUncapturedErrorEvent.h
    Modules/WebGPU/GPUUncapturedErrorEventInit.h
    Modules/WebGPU/GPUValidationError.h
    Modules/WebGPU/GPUVertexAttribute.h
    Modules/WebGPU/GPUVertexBufferLayout.h
    Modules/WebGPU/GPUVertexFormat.h
    Modules/WebGPU/GPUVertexState.h
    Modules/WebGPU/GPUVertexStepMode.h

    Modules/WebGPU/Implementation/WebGPUAdapterImpl.h
    Modules/WebGPU/Implementation/WebGPUBindGroupImpl.h
    Modules/WebGPU/Implementation/WebGPUBindGroupLayoutImpl.h
    Modules/WebGPU/Implementation/WebGPUBufferImpl.h
    Modules/WebGPU/Implementation/WebGPUCommandBufferImpl.h
    Modules/WebGPU/Implementation/WebGPUCommandEncoderImpl.h
    Modules/WebGPU/Implementation/WebGPUCompositorIntegrationImpl.h
    Modules/WebGPU/Implementation/WebGPUComputePassEncoderImpl.h
    Modules/WebGPU/Implementation/WebGPUComputePipelineImpl.h
    Modules/WebGPU/Implementation/WebGPUConvertToBackingContext.h
    Modules/WebGPU/Implementation/WebGPUDeviceImpl.h
    Modules/WebGPU/Implementation/WebGPUDowncastConvertToBackingContext.h
    Modules/WebGPU/Implementation/WebGPUExternalTextureImpl.h
    Modules/WebGPU/Implementation/WebGPUImpl.h
    Modules/WebGPU/Implementation/WebGPUPipelineLayoutImpl.h
    Modules/WebGPU/Implementation/WebGPUPresentationContextImpl.h
    Modules/WebGPU/Implementation/WebGPUPtr.h
    Modules/WebGPU/Implementation/WebGPUQuerySetImpl.h
    Modules/WebGPU/Implementation/WebGPUQueueImpl.h
    Modules/WebGPU/Implementation/WebGPURenderBundleEncoderImpl.h
    Modules/WebGPU/Implementation/WebGPURenderBundleImpl.h
    Modules/WebGPU/Implementation/WebGPURenderPassEncoderImpl.h
    Modules/WebGPU/Implementation/WebGPURenderPipelineImpl.h
    Modules/WebGPU/Implementation/WebGPUSamplerImpl.h
    Modules/WebGPU/Implementation/WebGPUShaderModuleImpl.h
    Modules/WebGPU/Implementation/WebGPUTextureImpl.h
    Modules/WebGPU/Implementation/WebGPUTextureViewImpl.h
    Modules/WebGPU/Implementation/WebGPUXRBindingImpl.h
    Modules/WebGPU/Implementation/WebGPUXRProjectionLayerImpl.h
    Modules/WebGPU/Implementation/WebGPUXRSubImageImpl.h
    Modules/WebGPU/Implementation/WebGPUXRViewImpl.h

    Modules/compression/CompressionStreamEncoder.h
    Modules/compression/DecompressionStreamDecoder.h
    Modules/compression/Formats.h

    Modules/encryptedmedia/CDM.h

    Modules/encryptedmedia/legacy/LegacyCDMPrivateClearKey.h

    Modules/fetch/FetchBodyConsumer.h
    Modules/fetch/FetchBodySource.h
    Modules/fetch/FetchRequestInit.h
    Modules/fetch/FetchResponse.h

    Modules/filesystem/StorageManagerFileSystem.h

    Modules/indexeddb/IDBActiveDOMObjectInlines.h
    Modules/indexeddb/IDBCursor.h
    Modules/indexeddb/IDBIndex.h
    Modules/indexeddb/IDBObjectStore.h

    Modules/mediasession/MediaImage.h
    Modules/mediasession/MediaMetadata.h
    Modules/mediasession/MediaMetadataInit.h

    Modules/mediastream/RTCIceCandidate.h
    Modules/mediastream/RTCRtpTransceiver.h
    Modules/mediastream/RTCSessionDescription.h
    Modules/mediastream/RTCStatsReport.h

    Modules/permissions/MainThreadPermissionObserver.h
    Modules/permissions/MainThreadPermissionObserverIdentifier.h

    Modules/reporting/ReportingObserver.h
    Modules/reporting/ReportingObserverCallback.h

    Modules/storage/StorageManager.h

    Modules/streams/ReadableStreamSource.h
    Modules/streams/ReadableStreamToSharedBufferSink.h

    Modules/webcodecs/WebCodecsBase.h

    Modules/webxr/XRHitTestTrackableType.h

    accessibility/AXAttributeCacheScope.h
    accessibility/AXComputedObjectAttributeCache.h
    accessibility/AXGeometryManager.h
    accessibility/AXListHelpers.h
    accessibility/AXLogger.h
    accessibility/AXNotifications.h
    accessibility/AXObjectCacheInlines.h
    accessibility/AccessibilityMenuListPopup.h
    accessibility/AccessibilityNodeObject.h
    accessibility/AccessibilityObjectInlines.h
    accessibility/AccessibilityRenderObject.h

    animation/CustomAnimationOptions.h
    animation/KeyframeAnimationOptions.h
    animation/KeyframeEffect.h

    bindings/js/CachedModuleScriptLoader.h
    bindings/js/JSDOMConvertXPathNSResolver.h
    bindings/js/JSEventListener.h
    bindings/js/JSShadowRealmGlobalScopeBase.h
    bindings/js/ReadableStreamDefaultController.h
    bindings/js/WebAssemblyCachedScriptSourceProvider.h
    bindings/js/WebAssemblyScriptSourceCode.h

    contentextensions/SerializedNFA.h

    css/CSSComputedStyleDeclaration.h
    css/CSSCounterValue.h
    css/CSSFontPaletteValuesRule.h
    css/CSSQuadValue.h
    css/CSSRectValue.h
    css/CSSRegisteredCustomProperty.h
    css/DOMCSSPaintWorklet.h
    css/Quad.h
    css/ShorthandSerializer.h
    css/StyleRuleImport.h

    dom/CaretPosition.h
    dom/CheckVisibilityOptions.h
    dom/CommandEvent.h
    dom/EventTargetConcrete.h
    dom/LoadableClassicScript.h
    dom/NameValidation.h
    dom/PopoverData.h
    dom/ScriptRunner.h
    dom/StartViewTransitionOptions.h
    dom/ToggleEvent.h
    dom/ViewTransition.h
    dom/ViewTransitionTypeSet.h
    dom/ViewTransitionUpdateCallback.h

    editing/EditCommand.h

    html/HTMLArticleElement.h
    html/HTMLAudioElement.h
    html/Origin.h
    html/PDFDocument.h

    layout/FormattingState.h

    layout/floats/FloatingContext.h

    layout/formattingContexts/FormattingContext.h
    layout/formattingContexts/FormattingGeometry.h
    layout/formattingContexts/FormattingQuirks.h

    layout/formattingContexts/block/BlockFormattingContext.h
    layout/formattingContexts/block/BlockFormattingGeometry.h
    layout/formattingContexts/block/BlockFormattingQuirks.h
    layout/formattingContexts/block/BlockFormattingState.h

    layout/formattingContexts/block/tablewrapper/TableWrapperBlockFormattingContext.h
    layout/formattingContexts/block/tablewrapper/TableWrapperBlockFormattingQuirks.h

    layout/formattingContexts/flex/FlexFormattingContext.h
    layout/formattingContexts/flex/FlexFormattingUtils.h

    layout/formattingContexts/grid/AxisConstraint.h
    layout/formattingContexts/grid/GridAreaLines.h
    layout/formattingContexts/grid/GridFormattingContext.h
    layout/formattingContexts/grid/GridLayoutConstraints.h
    layout/formattingContexts/grid/GridTypeAliases.h

    layout/formattingContexts/inline/InlineContentAligner.h
    layout/formattingContexts/inline/InlineContentCache.h
    layout/formattingContexts/inline/InlineContentConstrainer.h
    layout/formattingContexts/inline/InlineFormattingConstraints.h
    layout/formattingContexts/inline/InlineFormattingContext.h
    layout/formattingContexts/inline/InlineFormattingUtils.h
    layout/formattingContexts/inline/InlineLineBoxBuilder.h
    layout/formattingContexts/inline/InlineLineBoxVerticalAligner.h
    layout/formattingContexts/inline/IntrinsicWidthHandler.h
    layout/formattingContexts/inline/TextOnlySimpleLineBuilder.h

    layout/formattingContexts/inline/display/InlineDisplayContentBuilder.h
    layout/formattingContexts/inline/display/InlineDisplayLineBuilder.h

    layout/formattingContexts/inline/invalidation/InlineDamage.h
    layout/formattingContexts/inline/invalidation/InlineInvalidation.h

    layout/formattingContexts/inline/ruby/RubyFormattingContext.h

    layout/formattingContexts/table/TableFormattingGeometry.h
    layout/formattingContexts/table/TableFormattingQuirks.h

    layout/integration/LayoutIntegrationBoxGeometryUpdater.h
    layout/integration/LayoutIntegrationBoxTreeUpdater.h

    layout/integration/flex/LayoutIntegrationFlexLayout.h

    layout/integration/grid/LayoutIntegrationGridLayout.h

    layout/integration/inline/LayoutIntegrationInlineContentBuilder.h
    layout/integration/inline/LayoutIntegrationInlineContentPainter.h
    layout/integration/inline/LayoutIntegrationLineLayout.h
    layout/integration/inline/LineSelection.h

    layout/layouttree/LayoutContainingBlockChainIterator.h

    loader/FrameMemoryMonitor.h
    loader/LinkPreloadResourceClients.h

    loader/archive/ArchiveResourceCollection.h

    loader/archive/mhtml/MHTMLArchive.h

    loader/cache/CachedCSSStyleSheet.h
    loader/cache/CachedFontLoadRequest.h

    page/DOMSelection.h
    page/GetComposedRangesOptions.h
    page/LocalFrameViewInlines.h
    page/NavigationNavigationType.h
    page/NavigatorLoginStatus.h
    page/NavigatorUAData.h
    page/ShadowRealmGlobalScope.h
    page/UADataValues.h
    page/UALowEntropyJSON.h
    page/WebKitNamespace.h

    platform/AbortableTaskQueue.h
    platform/PlatformTouchEvent.h
    platform/PlatformTouchPoint.h
    platform/StringEntropyHelpers.h

    platform/encryptedmedia/CDMUtilities.h

    platform/generic/ScrollbarsControllerGeneric.h

    platform/graphics/ColorInterpolation.h
    platform/graphics/ColorNormalization.h
    platform/graphics/Damage.h
    platform/graphics/FloatPolygon.h
    platform/graphics/FontFamilySpecificationNull.h
    platform/graphics/FontRenderOptions.h
    platform/graphics/GraphicsLayerTransform.h
    platform/graphics/PathStream.h
    platform/graphics/PlatformDisplay.h
    platform/graphics/TrackBuffer.h

    platform/graphics/angle/ANGLEHeaders.h

    platform/graphics/egl/GLContext.h
    platform/graphics/egl/GLContextWrapper.h
    platform/graphics/egl/GLDisplay.h
    platform/graphics/egl/GLFence.h

    platform/graphics/opentype/OpenTypeVerticalData.h

    platform/graphics/transforms/TransformState.h

    platform/mediarecorder/MediaRecorderPrivateAVFImpl.h

    platform/mediastream/AudioTrackPrivateMediaStream.h
    platform/mediastream/RTCIceConnectionState.h
    platform/mediastream/RTCPeerConnectionHandlerClient.h
    platform/mediastream/VideoTrackPrivateMediaStream.h

    platform/mock/mediasource/MockMediaSourcePrivate.h
    platform/mock/mediasource/MockSourceBufferPrivate.h

    platform/network/DNSResolveQueue.h

    platform/video-codecs/BitReader.h

    platform/xr/XRHitTestSourceIdentifier.h

    rendering/EllipsisBoxPainter.h
    rendering/LineClampUpdater.h
    rendering/RenderFrame.h
    rendering/RenderLayoutState.h
    rendering/RenderListItem.h
    rendering/RenderMediaInlines.h
    rendering/RenderModel.h

    rendering/shapes/PolygonLayoutShape.h

    rendering/svg/legacy/LegacyRenderSVGModelObject.h

    style/StyleTreeResolver.h

    svg/SVGAngle.h
    svg/SVGElement.h
    svg/SVGLengthList.h
    svg/SVGNumberList.h
    svg/SVGPreserveAspectRatio.h

    workers/WorkerAnimationController.h

    workers/service/InstallEvent.h

    workers/shared/SharedWorkerScriptLoader.h

    xml/CustomXPathNSResolver.h
)


list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    Modules/ShapeDetection/Implementation/Cocoa/BarcodeDetectorImplementation.h
    Modules/ShapeDetection/Implementation/Cocoa/FaceDetectorImplementation.h
    Modules/ShapeDetection/Implementation/Cocoa/TextDetectorImplementation.h

    Modules/airplay/WebMediaSessionManager.h
    Modules/airplay/WebMediaSessionManagerClient.h

    Modules/applepay/ApplePayAutomaticReloadPaymentRequest.h
    Modules/applepay/ApplePayCouponCodeUpdate.h
    Modules/applepay/ApplePayDateComponents.h
    Modules/applepay/ApplePayDateComponentsRange.h
    Modules/applepay/ApplePayDeferredPaymentRequest.h
    Modules/applepay/ApplePayDetailsUpdateBase.h
    Modules/applepay/ApplePayError.h
    Modules/applepay/ApplePayErrorCode.h
    Modules/applepay/ApplePayErrorContactField.h
    Modules/applepay/ApplePayLineItem.h
    Modules/applepay/ApplePayPaymentMethodUpdate.h
    Modules/applepay/ApplePayPaymentOrderDetails.h
    Modules/applepay/ApplePayPaymentTiming.h
    Modules/applepay/ApplePayPaymentTokenContext.h
    Modules/applepay/ApplePayRecurringPaymentDateUnit.h
    Modules/applepay/ApplePayRecurringPaymentRequest.h
    Modules/applepay/ApplePaySetupConfiguration.h
    Modules/applepay/ApplePaySetupFeatureWebCore.h
    Modules/applepay/ApplePayShippingContactEditingMode.h
    Modules/applepay/ApplePayShippingContactUpdate.h
    Modules/applepay/ApplePayShippingMethod.h
    Modules/applepay/ApplePayShippingMethodUpdate.h
    Modules/applepay/PaymentInstallmentConfigurationWebCore.h
    Modules/applepay/PaymentSessionError.h
    Modules/applepay/PaymentSummaryItems.h

    Modules/encryptedmedia/legacy/LegacyCDM.h
    Modules/encryptedmedia/legacy/LegacyCDMPrivate.h

    Modules/mediasession/MediaPositionState.h
    Modules/mediasession/MediaSession.h
    Modules/mediasession/MediaSessionAction.h
    Modules/mediasession/MediaSessionActionDetails.h
    Modules/mediasession/MediaSessionActionHandler.h
    Modules/mediasession/MediaSessionCoordinator.h
    Modules/mediasession/MediaSessionCoordinatorPrivate.h
    Modules/mediasession/MediaSessionCoordinatorState.h
    Modules/mediasession/MediaSessionPlaybackState.h
    Modules/mediasession/MediaSessionReadyState.h
    Modules/mediasession/NavigatorMediaSession.h

    Modules/webauthn/AuthenticatorAssertionResponse.h
    Modules/webauthn/AuthenticatorAttachment.h
    Modules/webauthn/AuthenticatorAttestationResponse.h
    Modules/webauthn/AuthenticatorResponse.h

    Modules/webauthn/fido/Pin.h

    accessibility/cocoa/WebAccessibilityObjectWrapperBase.h

    accessibility/ios/AXRemoteTokenIOS.h

    bridge/objc/WebScriptObject.h
    bridge/objc/WebScriptObjectPrivate.h

    crypto/CommonCryptoUtilities.h
    crypto/CryptoAlgorithmIdentifier.h
    crypto/CryptoKey.h
    crypto/CryptoKeyPair.h
    crypto/CryptoKeyType.h
    crypto/CryptoKeyUsage.h

    crypto/keys/CryptoAesKeyAlgorithm.h
    crypto/keys/CryptoEcKeyAlgorithm.h
    crypto/keys/CryptoHmacKeyAlgorithm.h
    crypto/keys/CryptoKeyAES.h
    crypto/keys/CryptoKeyAlgorithm.h
    crypto/keys/CryptoKeyEC.h
    crypto/keys/CryptoKeyHMAC.h
    crypto/keys/CryptoRsaHashedKeyAlgorithm.h
    crypto/keys/CryptoRsaKeyAlgorithm.h

    dom/EventLoop.h
    dom/WindowEventLoop.h

    editing/ICUSearcher.h

    editing/cocoa/AlternativeTextContextController.h
    editing/cocoa/AlternativeTextUIController.h
    editing/cocoa/AttributedString.h
    editing/cocoa/AutofillElements.h
    editing/cocoa/DataDetection.h
    editing/cocoa/DataDetectorType.h
    editing/cocoa/EditingHTMLConverter.h
    editing/cocoa/NodeHTMLConverter.h
    editing/cocoa/TextAttachmentForSerialization.h

    loader/archive/cf/LegacyWebArchive.h

    loader/cache/CachedRawResource.h

    loader/mac/LoaderNSURLExtras.h

    page/CaptionUserPreferencesMediaAF.h

    page/cocoa/ContentChangeObserver.h
    page/cocoa/DOMTimerHoldingTank.h
    page/cocoa/DataDetectionResultsStorage.h
    page/cocoa/DataDetectorElementInfo.h
    page/cocoa/ImageOverlayDataDetectionResultIdentifier.h
    page/cocoa/WebTextIndicatorLayer.h

    page/ios/WebEventRegion.h

    page/scrolling/ScrollingStateOverflowScrollProxyNode.h

    page/scrolling/cocoa/ScrollingTreeFixedNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreeOverflowScrollProxyNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreePositionedNodeCocoa.h
    page/scrolling/cocoa/ScrollingTreeStickyNodeCocoa.h

    page/writing-tools/TextEffectController.h

    platform/CaptionPreferencesDelegate.h
    platform/FrameRateMonitor.h
    platform/MainThreadSharedTimer.h
    platform/PictureInPictureSupport.h
    platform/PlatformContentFilter.h
    platform/ScrollAlignment.h
    platform/ScrollAnimation.h
    platform/ScrollSnapAnimatorState.h
    platform/ScrollingEffectsController.h
    platform/SharedTimer.h
    platform/SystemSoundManager.h
    platform/TextRecognitionResult.h
    platform/WebCoreMainThread.h

    platform/audio/cocoa/AudioOutputUnitAdaptor.h
    platform/audio/cocoa/AudioSampleBufferList.h
    platform/audio/cocoa/AudioSampleDataConverter.h
    platform/audio/cocoa/AudioSampleDataSource.h
    platform/audio/cocoa/AudioUtilitiesCocoa.h
    platform/audio/cocoa/CAAudioStreamDescription.h
    platform/audio/cocoa/CARingBuffer.h
    platform/audio/cocoa/MediaSessionManagerCocoa.h
    platform/audio/cocoa/SpatialAudioExperienceHelper.h
    platform/audio/cocoa/WebAudioBufferList.h

    platform/audio/ios/MediaSessionHelperIOS.h
    platform/audio/ios/MediaSessionManagerIOS.h

    platform/cf/MediaAccessibilitySoftLink.h

    platform/cocoa/AppleVisualEffect.h
    platform/cocoa/CocoaView.h
    platform/cocoa/CocoaWritingToolsTypes.h
    platform/cocoa/CoreLocationGeolocationProvider.h
    platform/cocoa/CoreVideoExtras.h
    platform/cocoa/CoreVideoSoftLink.h
    platform/cocoa/LocalCurrentGraphicsContext.h
    platform/cocoa/NSURLUtilities.h
    platform/cocoa/NetworkExtensionContentFilter.h
    platform/cocoa/ParentalControlsContentFilter.h
    platform/cocoa/ParentalControlsURLFilter.h
    platform/cocoa/ParentalControlsURLFilterParameters.h
    platform/cocoa/PlatformTextAlternatives.h
    platform/cocoa/PlatformViewController.h
    platform/cocoa/PlaybackSessionModel.h
    platform/cocoa/PlaybackSessionModel.serialization.in
    platform/cocoa/PlaybackSessionModelMediaElement.h
    platform/cocoa/PowerSourceNotifier.h
    platform/cocoa/SearchPopupMenuCocoa.h
    platform/cocoa/SharedVideoFrameInfo.h
    platform/cocoa/StringUtilities.h
    platform/cocoa/SystemBattery.h
    platform/cocoa/SystemVersion.h
    platform/cocoa/VideoFullscreenCaptions.h
    platform/cocoa/VideoPresentationLayerProvider.h
    platform/cocoa/VideoPresentationModel.h
    platform/cocoa/VideoPresentationModelVideoElement.h
    platform/cocoa/WebAVPlayerLayer.h
    platform/cocoa/WebAVPlayerLayerView.h
    platform/cocoa/WebCoreNSURLExtras.h
    platform/cocoa/WebCoreObjCExtras.h
    platform/cocoa/WebKitAvailability.h
    platform/cocoa/WebNSAttributedStringExtras.h

    platform/gamepad/cocoa/GameControllerGamepadProvider.h
    platform/gamepad/cocoa/GameControllerSPI.h
    platform/gamepad/cocoa/GameControllerSoftLink.h

    platform/graphics/ImageDecoder.h
    platform/graphics/ImageDecoderIdentifier.h
    platform/graphics/ImageUtilities.h
    platform/graphics/MIMETypeCache.h
    platform/graphics/MediaPlaybackTargetWirelessPlayback.h
    platform/graphics/MediaSourceTypeSupportedCache.h
    platform/graphics/Model.h

    platform/graphics/angle/ANGLEUtilities.h

    platform/graphics/avfoundation/AudioSourceProviderAVFObjC.h
    platform/graphics/avfoundation/AudioVideoRendererAVFObjC.h
    platform/graphics/avfoundation/MediaPlaybackTargetCocoa.h
    platform/graphics/avfoundation/MediaPlayerPrivateAVFoundation.h
    platform/graphics/avfoundation/SampleBufferDisplayLayer.h
    platform/graphics/avfoundation/WebAVSampleBufferListener.h
    platform/graphics/avfoundation/WebMediaSessionManagerMac.h

    platform/graphics/avfoundation/objc/AVAssetMIMETypeCache.h
    platform/graphics/avfoundation/objc/ImageDecoderAVFObjC.h
    platform/graphics/avfoundation/objc/LocalSampleBufferDisplayLayer.h
    platform/graphics/avfoundation/objc/MediaPlayerPrivateMediaStreamAVFObjC.h
    platform/graphics/avfoundation/objc/MediaSampleAVFObjC.h
    platform/graphics/avfoundation/objc/VideoLayerManagerObjC.h

    platform/graphics/ca/GraphicsLayerCA.h
    platform/graphics/ca/LayerPool.h
    platform/graphics/ca/PlatformCAAnimation.h
    platform/graphics/ca/PlatformCAFilters.h
    platform/graphics/ca/PlatformCALayer.h
    platform/graphics/ca/PlatformCALayerClient.h
    platform/graphics/ca/PlatformCALayerDelegatedContents.h
    platform/graphics/ca/TileController.h

    platform/graphics/ca/cocoa/ContentsFormatCocoa.h
    platform/graphics/ca/cocoa/PlatformCAAnimationCocoa.h
    platform/graphics/ca/cocoa/PlatformCALayerCocoa.h
    platform/graphics/ca/cocoa/PlatformDynamicRangeLimitCocoa.h

    platform/graphics/cg/CGContextStateSaver.h
    platform/graphics/cg/CGWindowUtilities.h
    platform/graphics/cg/ColorSpaceCG.h
    platform/graphics/cg/GradientRendererCG.h
    platform/graphics/cg/GraphicsContextCG.h
    platform/graphics/cg/IOSurfacePool.h
    platform/graphics/cg/IOSurfacePoolIdentifier.h
    platform/graphics/cg/ImageBufferCGBackend.h
    platform/graphics/cg/ImageBufferCGBitmapBackend.h
    platform/graphics/cg/ImageBufferCGPDFDocumentBackend.h
    platform/graphics/cg/ImageBufferIOSurfaceBackend.h
    platform/graphics/cg/ImageDecoderCG.h
    platform/graphics/cg/PDFDocumentImage.h
    platform/graphics/cg/PathCG.h
    platform/graphics/cg/UTIRegistry.h

    platform/graphics/cocoa/AV1UtilitiesCocoa.h
    platform/graphics/cocoa/CMUtilities.h
    platform/graphics/cocoa/CVPixelBufferUtilities.h
    platform/graphics/cocoa/ColorCocoa.h
    platform/graphics/cocoa/DynamicContentScalingDisplayList.h
    platform/graphics/cocoa/FontCacheCoreText.h
    platform/graphics/cocoa/FontCascadeCocoaInlines.h
    platform/graphics/cocoa/FontCocoa.h
    platform/graphics/cocoa/FontDatabase.h
    platform/graphics/cocoa/FontFamilySpecificationCoreText.h
    platform/graphics/cocoa/FontFamilySpecificationCoreTextCache.h
    platform/graphics/cocoa/GraphicsContextGLCocoa.h
    platform/graphics/cocoa/HEVCUtilitiesCocoa.h
    platform/graphics/cocoa/IOSurface.h
    platform/graphics/cocoa/IOSurfaceDrawingBuffer.h
    platform/graphics/cocoa/MediaPlayerEnumsCocoa.h
    platform/graphics/cocoa/NullPlaybackSessionInterface.h
    platform/graphics/cocoa/NullVideoPresentationInterface.h
    platform/graphics/cocoa/PeriodicSharedTimer.h
    platform/graphics/cocoa/ShareableCVPixelBuffer.h
    platform/graphics/cocoa/ShareableCVPixelFormat.h
    platform/graphics/cocoa/ShareableGainMap.h
    platform/graphics/cocoa/SourceBufferParser.h
    platform/graphics/cocoa/SystemFontDatabaseCoreText.h
    platform/graphics/cocoa/TextTrackRepresentationCocoa.h
    platform/graphics/cocoa/VP9UtilitiesCocoa.h
    platform/graphics/cocoa/VideoTargetFactory.h
    platform/graphics/cocoa/WebActionDisablingCALayerDelegate.h
    platform/graphics/cocoa/WebCoreCALayerExtras.h
    platform/graphics/cocoa/WebLayer.h
    platform/graphics/cocoa/WebMAudioUtilitiesCocoa.h

    platform/graphics/cv/CVUtilities.h
    platform/graphics/cv/GraphicsContextGLCV.h
    platform/graphics/cv/ImageRotationSessionVT.h
    platform/graphics/cv/PixelBufferConformerCV.h
    platform/graphics/cv/VideoFrameCV.h

    platform/ios/DeviceOrientationUpdateProvider.h
    platform/ios/KeyEventCodesIOS.h
    platform/ios/LegacyTileCache.h
    platform/ios/LocalCurrentTraitCollection.h
    platform/ios/LocalizedDeviceModel.h
    platform/ios/MotionManagerClient.h
    platform/ios/PlatformEventFactoryIOS.h
    platform/ios/PlaybackSessionInterfaceAVKitLegacy.h
    platform/ios/PlaybackSessionInterfaceIOS.h
    platform/ios/PlaybackSessionInterfaceTVOS.h
    platform/ios/QuickLook.h
    platform/ios/TileControllerMemoryHandlerIOS.h
    platform/ios/UIViewControllerUtilities.h
    platform/ios/VideoPresentationInterfaceAVKitLegacy.h
    platform/ios/VideoPresentationInterfaceIOS.h
    platform/ios/VideoPresentationInterfaceTVOS.h
    platform/ios/WebAVPlayerController.h
    platform/ios/WebBackgroundTaskController.h
    platform/ios/WebCoreMotionManager.h
    platform/ios/WebEvent.h
    platform/ios/WebEventPrivate.h
    platform/ios/WebItemProviderPasteboard.h
    platform/ios/WebSQLiteDatabaseTrackerClient.h
    platform/ios/WebVideoFullscreenControllerAVKit.h

    platform/ios/wak/FloatingPointEnvironment.h
    platform/ios/wak/WAKAppKitStubs.h
    platform/ios/wak/WAKClipView.h
    platform/ios/wak/WAKResponder.h
    platform/ios/wak/WAKScrollView.h
    platform/ios/wak/WAKWindow.h
    platform/ios/wak/WKContentObservation.h
    platform/ios/wak/WKGraphics.h
    platform/ios/wak/WKTypes.h
    platform/ios/wak/WKUtilities.h
    platform/ios/wak/WKView.h
    platform/ios/wak/WKViewPrivate.h
    platform/ios/wak/WebCoreThread.h
    platform/ios/wak/WebCoreThreadInternal.h
    platform/ios/wak/WebCoreThreadMessage.h
    platform/ios/wak/WebCoreThreadRun.h
    platform/ios/wak/WebCoreThreadSystemInterface.h

    platform/mediarecorder/MediaRecorderPrivateOptions.h

    platform/mediastream/AudioMediaStreamTrackRenderer.h
    platform/mediastream/RealtimeIncomingVideoSource.h
    platform/mediastream/RealtimeMediaSourceIdentifier.h

    platform/mediastream/cocoa/AVVideoCaptureSource.h
    platform/mediastream/cocoa/AudioMediaStreamTrackRendererInternalUnit.h
    platform/mediastream/cocoa/AudioMediaStreamTrackRendererUnit.h
    platform/mediastream/cocoa/BaseAudioCaptureUnit.h
    platform/mediastream/cocoa/BaseAudioMediaStreamTrackRendererUnit.h
    platform/mediastream/cocoa/CoreAudioCaptureDeviceManager.h
    platform/mediastream/cocoa/CoreAudioCaptureSource.h
    platform/mediastream/cocoa/CoreAudioCaptureUnit.h
    platform/mediastream/cocoa/DisplayCaptureSourceCocoa.h
    platform/mediastream/cocoa/RealtimeIncomingVideoSourceCocoa.h
    platform/mediastream/cocoa/ScreenCaptureKitCaptureSource.h
    platform/mediastream/cocoa/ScreenCaptureKitSharingSessionManager.h
    platform/mediastream/cocoa/WebAudioSourceProviderCocoa.h

    platform/mediastream/ios/AVAudioSessionCaptureDeviceManager.h

    platform/mediastream/libwebrtc/LibWebRTCProviderCocoa.h
    platform/mediastream/libwebrtc/VideoFrameLibWebRTC.h

    platform/network/cf/AuthenticationChallenge.h
    platform/network/cf/CertificateInfo.h
    platform/network/cf/ResourceError.h
    platform/network/cf/ResourceRequest.h
    platform/network/cf/ResourceRequestCFNet.h
    platform/network/cf/ResourceResponse.h

    platform/network/cocoa/AuthenticationCocoa.h
    platform/network/cocoa/CookieStorageObserver.h
    platform/network/cocoa/CredentialCocoa.h
    platform/network/cocoa/FormDataStreamCocoa.h
    platform/network/cocoa/HTTPCookieAcceptPolicyCocoa.h
    platform/network/cocoa/ProtectionSpaceCocoa.h
    platform/network/cocoa/RangeResponseGenerator.h
    platform/network/cocoa/UTIUtilities.h
    platform/network/cocoa/WebCoreNSURLSession.h
    platform/network/cocoa/WebCoreURLResponse.h

    platform/network/ios/LegacyPreviewLoaderClient.h
    platform/network/ios/WebCoreURLResponseIOS.h

    platform/video-codecs/cocoa/WebRTCVideoDecoder.h

    platform/xr/cocoa/PlatformXRPose.h

    rendering/cocoa/RenderThemeCocoa.h

    rendering/ios/RenderThemeIOS.h

    testing/MockWebAuthenticationConfiguration.h

    testing/cocoa/WebViewVisualIdentificationOverlay.h
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/cocoa/CocoaAccessibilityConstants.h
)

list(APPEND WebCore_IDL_FILES
    Modules/applepay/ApplePayAutomaticReloadPaymentRequest.idl
    Modules/applepay/ApplePayCancelEvent.idl
    Modules/applepay/ApplePayContactField.idl
    Modules/applepay/ApplePayCouponCodeChangedEvent.idl
    Modules/applepay/ApplePayCouponCodeDetails.idl
    Modules/applepay/ApplePayCouponCodeUpdate.idl
    Modules/applepay/ApplePayDateComponents.idl
    Modules/applepay/ApplePayDateComponentsRange.idl
    Modules/applepay/ApplePayDeferredPaymentRequest.idl
    Modules/applepay/ApplePayDetailsUpdateBase.idl
    Modules/applepay/ApplePayDisbursementRequest.idl
    Modules/applepay/ApplePayError.idl
    Modules/applepay/ApplePayErrorCode.idl
    Modules/applepay/ApplePayErrorContactField.idl
    Modules/applepay/ApplePayFeature.idl
    Modules/applepay/ApplePayInstallmentConfiguration.idl
    Modules/applepay/ApplePayInstallmentItem.idl
    Modules/applepay/ApplePayInstallmentItemType.idl
    Modules/applepay/ApplePayInstallmentRetailChannel.idl
    Modules/applepay/ApplePayLaterAvailability.idl
    Modules/applepay/ApplePayLineItem.idl
    Modules/applepay/ApplePayMerchantCapability.idl
    Modules/applepay/ApplePayPayment.idl
    Modules/applepay/ApplePayPaymentAuthorizationResult.idl
    Modules/applepay/ApplePayPaymentAuthorizedEvent.idl
    Modules/applepay/ApplePayPaymentContact.idl
    Modules/applepay/ApplePayPaymentMethod.idl
    Modules/applepay/ApplePayPaymentMethodSelectedEvent.idl
    Modules/applepay/ApplePayPaymentMethodType.idl
    Modules/applepay/ApplePayPaymentMethodUpdate.idl
    Modules/applepay/ApplePayPaymentOrderDetails.idl
    Modules/applepay/ApplePayPaymentPass.idl
    Modules/applepay/ApplePayPaymentRequest.idl
    Modules/applepay/ApplePayPaymentTiming.idl
    Modules/applepay/ApplePayPaymentTokenContext.idl
    Modules/applepay/ApplePayRecurringPaymentDateUnit.idl
    Modules/applepay/ApplePayRecurringPaymentRequest.idl
    Modules/applepay/ApplePayRequestBase.idl
    Modules/applepay/ApplePaySession.idl
    Modules/applepay/ApplePaySessionError.idl
    Modules/applepay/ApplePaySetup.idl
    Modules/applepay/ApplePaySetupConfiguration.idl
    Modules/applepay/ApplePaySetupFeature.idl
    Modules/applepay/ApplePaySetupFeatureState.idl
    Modules/applepay/ApplePaySetupFeatureType.idl
    Modules/applepay/ApplePayShippingContactEditingMode.idl
    Modules/applepay/ApplePayShippingContactSelectedEvent.idl
    Modules/applepay/ApplePayShippingContactUpdate.idl
    Modules/applepay/ApplePayShippingMethod.idl
    Modules/applepay/ApplePayShippingMethodSelectedEvent.idl
    Modules/applepay/ApplePayShippingMethodUpdate.idl
    Modules/applepay/ApplePayValidateMerchantEvent.idl

    Modules/applepay-ams-ui/ApplePayAMSUIRequest.idl

    Modules/applepay/paymentrequest/ApplePayModifier.idl
    Modules/applepay/paymentrequest/ApplePayPaymentCompleteDetails.idl
    Modules/applepay/paymentrequest/ApplePayRequest.idl
)

set(FEATURE_DEFINES_OBJECTIVE_C "LANGUAGE_OBJECTIVE_C=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
set(ADDITIONAL_BINDINGS_DEPENDENCIES
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
    ${WEBCORE_DIR}/Modules/modern-media-controls/media/YouTubeCaptionQuirk.js
)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE WebCore)
list(APPEND WebCoreTestSupport_PRIVATE_HEADERS testing/cocoa/WebArchiveDumpSupport.h)
list(APPEND WebCoreTestSupport_SOURCES
    testing/Internals.mm
    testing/MockApplePaySetupFeature.cpp
    testing/MockContentFilter.cpp
    testing/MockContentFilterSettings.cpp
    testing/MockMediaSessionCoordinator.cpp
    testing/MockPaymentCoordinator.cpp
    testing/MockPreviewLoaderClient.cpp
    testing/ServiceWorkerInternals.mm

    testing/cocoa/CocoaColorSerialization.mm
    testing/cocoa/WebArchiveDumpSupport.mm
)
list(APPEND WebCoreTestSupport_IDL_FILES
    testing/MockPaymentAddress.idl
    testing/MockPaymentContactFields.idl
    testing/MockPaymentCoordinator.idl
    testing/MockPaymentError.idl
)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/WebCore/WebKitAvailability.h)
    file(COPY platform/cocoa/WebKitAvailability.h DESTINATION ${CMAKE_BINARY_DIR}/WebCore)
endif ()

# Mac-only headers referenced from shared PLATFORM(COCOA) code in WebKit.
list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/mac/WebAccessibilityObjectWrapperMac.h

    editing/mac/DictionaryLookup.h
    editing/mac/TextAlternativeWithRange.h
    editing/mac/TextUndoInsertionMarkupMac.h
    editing/mac/UniversalAccessZoom.h

    platform/gamepad/mac/HIDGamepadProvider.h
    platform/gamepad/mac/MultiGamepadProvider.h

    platform/mac/LegacyNSPasteboardTypes.h
    platform/mac/NSScrollerImpDetails.h
    platform/mac/PasteboardWriter.h
    platform/mac/PlatformEventFactoryMac.h
    platform/mac/PlaybackSessionInterfaceMac.h
    platform/mac/ScrollbarThemeMac.h
    platform/mac/VideoPresentationInterfaceMac.h
    platform/mac/WebCoreFullScreenWindow.h
    platform/mac/WebCoreNSFontManagerExtras.h
    platform/mac/WebCoreView.h
)
