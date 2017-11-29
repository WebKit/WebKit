list(APPEND PAL_SOURCES
    avfoundation/MediaTimeAVFoundation.cpp

    cf/CoreMediaSoftLink.cpp

    cocoa/FileSizeFormatterCocoa.mm
    cocoa/LoggingCocoa.mm

    crypto/commoncrypto/CryptoDigestCommonCrypto.cpp

    system/cocoa/SleepDisablerCocoa.cpp

    system/mac/ClockCM.mm
    system/mac/DefaultSearchProvider.cpp
    system/mac/PopupMenu.mm
    system/mac/SoundMac.mm
    system/mac/SystemSleepListenerMac.mm
    system/mac/WebPanel.mm

    text/mac/KillRingMac.mm
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal/cf"
    "${PAL_DIR}/pal/spi/cf"
    "${PAL_DIR}/pal/spi/cocoa"
    "${PAL_DIR}/pal/spi/mac"
)
