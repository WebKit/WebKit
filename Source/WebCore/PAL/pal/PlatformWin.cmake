list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    system/ClockGeneric.cpp

    system/win/SoundWin.cpp

    text/KillRing.cpp

    win/LoggingWin.cpp
)

list(APPEND PAL_PRIVATE_LIBRARIES
    OpenSSL::Crypto
)
