list(APPEND PAL_PUBLIC_HEADERS
    avfoundation/MediaTimeAVFoundation.h
    avfoundation/OutputContext.h
    avfoundation/OutputDevice.h

    cf/AudioToolboxSoftLink.h
    cf/CoreMediaSoftLink.h
    cf/CoreTextSoftLink.h
    cf/OTSVGTable.h
    cf/VideoToolboxSoftLink.h

    cg/CoreGraphicsSoftLink.h

    cocoa/AppSSOSoftLink.h
    cocoa/AVFoundationSoftLink.h
    cocoa/CoreMLSoftLink.h
    cocoa/CryptoKitPrivateSoftLink.h
    cocoa/DataDetectorsCoreSoftLink.h
    cocoa/MediaToolboxSoftLink.h
    cocoa/NaturalLanguageSoftLink.h
    cocoa/NetworkConnectionIntegritySoftLink.h
    cocoa/OpenGLSoftLinkCocoa.h
    cocoa/PassKitSoftLink.h
    cocoa/RevealSoftLink.h
    cocoa/SpeechSoftLink.h
    cocoa/UsageTrackingSoftLink.h
    cocoa/VisionKitCoreSoftLink.h

    mac/DataDetectorsSoftLink.h
    mac/LookupSoftLink.h
    mac/QuickLookUISoftLink.h

    spi/cf/CFLocaleSPI.h
    spi/cf/CFNetworkConnectionCacheSPI.h
    spi/cf/CFNetworkSPI.h
    spi/cf/CFNotificationCenterSPI.h
    spi/cf/CFUtilitiesSPI.h
    spi/cf/CoreAudioSPI.h
    spi/cf/CoreMediaSPI.h
    spi/cf/CoreTextSPI.h
    spi/cf/CoreVideoSPI.h
    spi/cf/MediaAccessibilitySPI.h

    spi/cg/CoreGraphicsSPI.h
    spi/cg/ImageIOSPI.h

    spi/cocoa/AVAssetWriterSPI.h
    spi/cocoa/AVFoundationSPI.h
    spi/cocoa/AVKitSPI.h
    spi/cocoa/AXSpeechManagerSPI.h
    spi/cocoa/AccessibilitySupportSPI.h
    spi/cocoa/AccessibilitySupportSoftLink.h
    spi/cocoa/AppSSOSPI.h
    spi/cocoa/AuthKitSPI.h
    spi/cocoa/AudioToolboxSPI.h
    spi/cocoa/CFNSURLConnectionSPI.h
    spi/cocoa/CommonCryptoSPI.h
    spi/cocoa/CoreServicesSPI.h
    spi/cocoa/CryptoKitPrivateSPI.h
    spi/cocoa/DataDetectorsCoreSPI.h
    spi/cocoa/FeatureFlagsSPI.h
    spi/cocoa/FilePortSPI.h
    spi/cocoa/IOKitSPI.h
    spi/cocoa/IOPMLibSPI.h
    spi/cocoa/IOPSLibSPI.h
    spi/cocoa/IOReturnSPI.h
    spi/cocoa/IOSurfaceSPI.h
    spi/cocoa/IOTypesSPI.h
    spi/cocoa/LaunchServicesSPI.h
    spi/cocoa/MediaToolboxSPI.h
    spi/cocoa/MetalSPI.h
    spi/cocoa/NEFilterSourceSPI.h
    spi/cocoa/NSAccessibilitySPI.h
    spi/cocoa/NSAttributedStringSPI.h
    spi/cocoa/NSButtonCellSPI.h
    spi/cocoa/NSCalendarDateSPI.h
    spi/cocoa/NSColorSPI.h
    spi/cocoa/NSExtensionSPI.h
    spi/cocoa/NSFileManagerSPI.h
    spi/cocoa/NSFileSizeFormatterSPI.h
    spi/cocoa/NSProgressSPI.h
    spi/cocoa/NSStringSPI.h
    spi/cocoa/NSTouchBarSPI.h
    spi/cocoa/NSURLConnectionSPI.h
    spi/cocoa/NSURLDownloadSPI.h
    spi/cocoa/NSURLFileTypeMappingsSPI.h
    spi/cocoa/NSUserDefaultsSPI.h
    spi/cocoa/NSXPCConnectionSPI.h
    spi/cocoa/NotifySPI.h
    spi/cocoa/PassKitInstallmentsSPI.h
    spi/cocoa/PassKitSPI.h
    spi/cocoa/QuartzCoreSPI.h
    spi/cocoa/RevealSPI.h
    spi/cocoa/SceneKitSPI.h
    spi/cocoa/SecKeyProxySPI.h
    spi/cocoa/ServersSPI.h
    spi/cocoa/SpeechSPI.h
    spi/cocoa/TCCSPI.h
    spi/cocoa/URLFormattingSPI.h
    spi/cocoa/VisionKitCoreSPI.h
    spi/cocoa/WebFilterEvaluatorSPI.h
    spi/cocoa/pthreadSPI.h

    spi/ios/DataDetectorsUISPI.h
    spi/ios/GraphicsServicesSPI.h

    spi/mac/CoreUISPI.h
    spi/mac/DataDetectorsSPI.h
    spi/mac/HIServicesSPI.h
    spi/mac/HIToolboxSPI.h
    spi/mac/IOKitSPIMac.h
    spi/mac/LookupSPI.h
    spi/mac/MediaRemoteSPI.h
    spi/mac/NSAppearanceSPI.h
    spi/mac/NSApplicationSPI.h
    spi/mac/NSCellSPI.h
    spi/mac/NSColorWellSPI.h
    spi/mac/NSEventSPI.h
    spi/mac/NSFontSPI.h
    spi/mac/NSGraphicsSPI.h
    spi/mac/NSImageSPI.h
    spi/mac/NSImmediateActionGestureRecognizerSPI.h
    spi/mac/NSMenuSPI.h
    spi/mac/NSPasteboardSPI.h
    spi/mac/NSPopoverColorWellSPI.h
    spi/mac/NSPopoverSPI.h
    spi/mac/NSResponderSPI.h
    spi/mac/NSScrollViewSPI.h
    spi/mac/NSScrollerImpSPI.h
    spi/mac/NSScrollingInputFilterSPI.h
    spi/mac/NSScrollingMomentumCalculatorSPI.h
    spi/mac/NSServicesRolloverButtonCellSPI.h
    spi/mac/NSSharingServicePickerSPI.h
    spi/mac/NSSharingServiceSPI.h
    spi/mac/NSSpellCheckerSPI.h
    spi/mac/NSTextFinderSPI.h
    spi/mac/NSTextInputContextSPI.h
    spi/mac/NSUndoManagerSPI.h
    spi/mac/NSViewSPI.h
    spi/mac/NSWindowSPI.h
    spi/mac/PIPSPI.h
    spi/mac/QuickLookMacSPI.h
    spi/mac/SystemPreviewSPI.h
    spi/mac/TelephonyUtilitiesSPI.h

    system/cocoa/SleepDisablerCocoa.h

    system/mac/DefaultSearchProvider.h
    system/mac/PopupMenu.h
    system/mac/SystemSleepListenerMac.h
    system/mac/WebPanel.h
)

list(APPEND PAL_SOURCES
    avfoundation/MediaTimeAVFoundation.cpp
    avfoundation/OutputContext.mm
    avfoundation/OutputDevice.mm

    cf/AudioToolboxSoftLink.cpp
    cf/CoreMediaSoftLink.cpp
    cf/CoreTextSoftLink.cpp
    cf/OTSVGTable.cpp
    cf/VideoToolboxSoftLink.cpp

    cg/CoreGraphicsSoftLink.cpp

    cocoa/AppSSOSoftLink.mm
    cocoa/AVFoundationSoftLink.mm
    cocoa/CoreMLSoftLink.mm
    cocoa/CryptoKitPrivateSoftLink.mm
    cocoa/DataDetectorsCoreSoftLink.mm
    cocoa/FileSizeFormatterCocoa.mm
    cocoa/Gunzip.cpp
    cocoa/MediaToolboxSoftLink.cpp
    cocoa/NaturalLanguageSoftLink.mm
    cocoa/NetworkConnectionIntegritySoftLink.mm
    cocoa/OpenGLSoftLinkCocoa.mm
    cocoa/PassKitSoftLink.mm
    cocoa/RevealSoftLink.mm
    cocoa/SpeechSoftLink.mm
    cocoa/UsageTrackingSoftLink.mm
    cocoa/VisionKitCoreSoftLink.mm

    crypto/commoncrypto/CryptoDigestCommonCrypto.cpp

    mac/DataDetectorsSoftLink.mm
    mac/LookupSoftLink.mm
    mac/QuickLookUISoftLink.mm

    spi/cocoa/AccessibilitySupportSoftLink.cpp

    system/ClockGeneric.cpp

    system/cocoa/SleepDisablerCocoa.cpp

    system/mac/DefaultSearchProvider.cpp
    system/mac/PopupMenu.mm
    system/mac/SoundMac.mm
    system/mac/SystemSleepListenerMac.mm
    system/mac/WebPanel.mm

    text/ios/TextEncodingRegistryIOS.mm

    text/mac/KillRingMac.mm
    text/mac/TextEncodingRegistryMac.mm
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${PAL_DIR}/pal/avfoundation"
    "${PAL_DIR}/pal/cf"
    "${PAL_DIR}/pal/cocoa"
    "${PAL_DIR}/pal/spi/cf"
    "${PAL_DIR}/pal/spi/cg"
    "${PAL_DIR}/pal/spi/cocoa"
    "${PAL_DIR}/pal/spi/mac"
)
