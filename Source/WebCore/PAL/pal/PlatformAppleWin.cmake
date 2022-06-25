list(APPEND PAL_PUBLIC_HEADERS
    avfoundation/MediaTimeAVFoundation.h

    cf/CoreMediaSoftLink.h

    spi/cf/CFLocaleSPI.h
    spi/cf/CFNetworkConnectionCacheSPI.h
    spi/cf/CFUtilitiesSPI.h
    spi/cf/CoreAudioSPI.h
    spi/cf/CoreMediaSPI.h
    spi/cf/MediaAccessibilitySPI.h

    spi/win/CFNetworkSPIWin.h
    spi/win/CoreTextSPIWin.h
)

list(APPEND PAL_PUBLIC_HEADERS
    spi/cg/CoreGraphicsSPI.h
    spi/cg/ImageIOSPI.h
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal/spi/cg"
)

list(APPEND PAL_SOURCES
    avfoundation/MediaTimeAVFoundation.cpp

    cf/CoreMediaSoftLink.cpp

    crypto/win/CryptoDigestWin.cpp

    spi/cf/CFNetworkSPIWin.cpp
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal/avfoundation"
    "${PAL_DIR}/pal/cf"
    "${PAL_DIR}/pal/spi/cf"
    "${PAL_DIR}/pal/spi/win"
)
