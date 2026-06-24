list(APPEND PAL_PUBLIC_HEADERS
    module.modulemap

    avfoundation/MediaTimeAVFoundation.h
    avfoundation/OutputContext.h
    avfoundation/OutputDevice.h

    cf/AudioToolboxSoftLink.h
    cf/CoreAudioExtras.h
    cf/CoreMediaSoftLink.h
    cf/CoreTextSoftLink.h
    cf/OTSVGTable.h
    cf/VideoToolboxSoftLink.h

    cg/CoreGraphicsSoftLink.h

    cocoa/ARKitSoftLink.h
    cocoa/AVFAudioSoftLink.h
    cocoa/AVFoundationSoftLink.h
    cocoa/AccessibilitySoftLink.h
    cocoa/AppSSOSoftLink.h
    cocoa/ContactsSoftLink.h
    cocoa/CoreMLSoftLink.h
    cocoa/CoreMaterialSoftLink.h
    cocoa/CoreTelephonySoftLink.h
    cocoa/CryptoKitPrivateSoftLink.h
    cocoa/DataDetectorsCoreSoftLink.h
    cocoa/EnhancedSecurityCocoa.h
    cocoa/LinkPresentationSoftLink.h
    cocoa/LockdownModeCocoa.h
    cocoa/MediaToolboxSoftLink.h
    cocoa/NaturalLanguageSoftLink.h
    cocoa/OpenGLSoftLinkCocoa.h
    cocoa/PassKitSoftLink.h
    cocoa/QuartzCoreSoftLink.h
    cocoa/RevealSoftLink.h
    cocoa/ScreenTimeSoftLink.h
    cocoa/SpeechSoftLink.h
    cocoa/TranslationUIServicesSoftLink.h
    cocoa/UsageTrackingSoftLink.h
    cocoa/VisionKitCoreSoftLink.h
    cocoa/VisionSoftLink.h
    cocoa/WebContentRestrictionsSoftLink.h
    cocoa/WebPrivacySoftLink.h
    cocoa/WritingToolsUISoftLink.h

    crypto/CryptoAlgorithmAESGCMCocoa.h
    crypto/CryptoAlgorithmAESKWCocoaBridging.h
    crypto/CryptoAlgorithmEd25519CocoaBridging.h
    crypto/CryptoAlgorithmHKDFCocoaBridging.h
    crypto/CryptoAlgorithmHMACCocoaBridging.h
    crypto/CryptoAlgorithmX25519CocoaBridging.h
    crypto/CryptoEDKeyBridging.h
    crypto/PlatformECKey.h

    graphics/cocoa/WebAVContentKeyGrouping.h
    graphics/cocoa/WebAVContentKeyReportGroupExtras.h

    ios/UIKitSoftLink.h

    mac/DataDetectorsSoftLink.h
    mac/LookupSoftLink.h
    mac/QuickLookUISoftLink.h
    mac/ScreenCaptureKitSoftLink.h

    spi/cf/CFNetworkConnectionCacheSPI.h
    spi/cf/CFNetworkSPI.h
    spi/cf/CFNotificationCenterSPI.h
    spi/cf/CFUtilitiesSPI.h
    spi/cf/CoreAudioSPI.h
    spi/cf/CoreMediaSPI.h
    spi/cf/CoreTextSPI.h
    spi/cf/CoreVideoSPI.h
    spi/cf/MediaAccessibilitySPI.h
    spi/cf/VideoToolboxSPI.h

    spi/cg/CoreGraphicsSPI.h
    spi/cg/ImageIOSPI.h

    spi/cocoa/AVAssetWriterSPI.h
    spi/cocoa/AVFoundationSPI.h
    spi/cocoa/AVKitSPI.h
    spi/cocoa/AVStreamDataParserSPI.h
    spi/cocoa/AXSpeechManagerSPI.h
    spi/cocoa/AccessibilitySupportSPI.h
    spi/cocoa/AccessibilitySupportSoftLink.h
    spi/cocoa/AppSSOSPI.h
    spi/cocoa/AudioToolboxSPI.h
    spi/cocoa/AuthKitSPI.h
    spi/cocoa/CommonCryptoSPI.h
    spi/cocoa/ContactsSPI.h
    spi/cocoa/CoreCryptoSPI.h
    spi/cocoa/CoreMaterialSPI.h
    spi/cocoa/CoreServicesSPI.h
    spi/cocoa/CoreTelephonySPI.h
    spi/cocoa/CryptoKitPrivateSPI.h
    spi/cocoa/DataDetectorsCoreSPI.h
    spi/cocoa/FeatureFlagsSPI.h
    spi/cocoa/FilePortSPI.h
    spi/cocoa/FoundationSPI.h
    spi/cocoa/IOKitSPI.h
    spi/cocoa/IOPMLibSPI.h
    spi/cocoa/IOPSLibSPI.h
    spi/cocoa/LaunchServicesSPI.h
    spi/cocoa/LinkPresentationSPI.h
    spi/cocoa/MediaToolboxSPI.h
    spi/cocoa/MetalSPI.h
    spi/cocoa/NEFilterSourceSPI.h
    spi/cocoa/NSAccessibilitySPI.h
    spi/cocoa/NSAttributedStringSPI.h
    spi/cocoa/NSButtonCellSPI.h
    spi/cocoa/NSCalendarDateSPI.h
    spi/cocoa/NSExtensionSPI.h
    spi/cocoa/NSFileManagerSPI.h
    spi/cocoa/NSFileSizeFormatterSPI.h
    spi/cocoa/NSKeyedUnarchiverSPI.h
    spi/cocoa/NSProgressSPI.h
    spi/cocoa/NSStringSPI.h
    spi/cocoa/NSTouchBarSPI.h
    spi/cocoa/NSURLConnectionSPI.h
    spi/cocoa/NSURLDownloadSPI.h
    spi/cocoa/NSURLFileTypeMappingsSPI.h
    spi/cocoa/NSUserDefaultsSPI.h
    spi/cocoa/NSXPCConnectionSPI.h
    spi/cocoa/NetworkSPI.h
    spi/cocoa/NotifySPI.h
    spi/cocoa/PassKitInstallmentsSPI.h
    spi/cocoa/PassKitSPI.h
    spi/cocoa/QuartzCoreSPI.h
    spi/cocoa/RevealSPI.h
    spi/cocoa/SQLite3SPI.h
    spi/cocoa/SecKeyProxySPI.h
    spi/cocoa/ServersSPI.h
    spi/cocoa/SpeechSPI.h
    spi/cocoa/TCCSPI.h
    spi/cocoa/TranslationUIServicesSPI.h
    spi/cocoa/UIFoundationSPI.h
    spi/cocoa/URLFormattingSPI.h
    spi/cocoa/UniformTypeIdentifiersSPI.h
    spi/cocoa/VisionKitCoreSPI.h
    spi/cocoa/WebContentRestrictionsSPI.h
    spi/cocoa/WebFilterEvaluatorSPI.h
    spi/cocoa/WebPrivacySPI.h
    spi/cocoa/WritingToolsSPI.h
    spi/cocoa/WritingToolsUISPI.h
    spi/cocoa/pthreadSPI.h

    spi/ios/BrowserEngineKitSPI.h
    spi/ios/DataDetectorsUISPI.h
    spi/ios/DataDetectorsUISoftLink.h
    spi/ios/GraphicsServicesSPI.h
    spi/ios/MobileGestaltSPI.h
    spi/ios/UIKitSPI.h

    spi/mac/CoreUISPI.h
    spi/mac/DataDetectorsSPI.h
    spi/mac/IOKitSPIMac.h
    spi/mac/MediaRemoteSPI.h
    spi/mac/PowerLogSPI.h
    spi/mac/QuickLookMacSPI.h

    system/cocoa/RegexHelper.h
    system/cocoa/SleepDisablerCocoa.h

    system/ios/UserInterfaceIdiom.h

    system/mac/DefaultSearchProvider.h
    system/mac/PopupMenu.h
    system/mac/SystemSleepListenerMac.h
    system/mac/WebPanel.h
)

list(APPEND PAL_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
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
