include(PlatformCocoa.cmake)

list(APPEND PAL_PUBLIC_HEADERS
    ios/AVRoutingSoftLink.h
    ios/ManagedConfigurationSoftLink.h
    ios/QuickLookSoftLink.h
    ios/SystemStatusSoftLink.h

    spi/ios/AXRuntimeSPI.h
    spi/ios/BarcodeSupportSPI.h
    spi/ios/CelestialSPI.h
    spi/ios/CoreUISPI.h
    spi/ios/IOKitSPIIOS.h
    spi/ios/ManagedConfigurationSPI.h
    spi/ios/MediaPlayerSPI.h
    spi/ios/MobileKeyBagSPI.h
    spi/ios/OpenGLESSPI.h
    spi/ios/QuickLookSPI.h
    spi/ios/SBSStatusBarSPI.h
    spi/ios/SystemPreviewSPI.h

    system/ios/Device.h
)

list(APPEND PAL_SOURCES
    ios/AVRoutingSoftLink.mm
    ios/ManagedConfigurationSoftLink.mm
    ios/QuickLookSoftLink.mm
    ios/SystemStatusSoftLink.mm
    ios/UIKitSoftLink.mm

    spi/ios/DataDetectorsUISoftLink.mm

    system/Sound.cpp

    system/ios/Device.cpp
    system/ios/SleepDisablerIOS.mm
    system/ios/UserInterfaceIdiom.mm

    text/KillRing.cpp
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal/spi/ios"
    "${PAL_DIR}/pal/system/cocoa"
    "${PAL_DIR}/pal/system/ios"
)

target_compile_options(PAL PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})
