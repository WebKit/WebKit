list(APPEND PAL_PUBLIC_HEADERS
)

list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    crypto/win/CryptoDigestWin.cpp

    system/ClockGeneric.cpp

    system/win/SoundWin.cpp

    text/KillRing.cpp

    win/LoggingWin.cpp
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_LIBRARIES_DIR}/include"
)

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})
