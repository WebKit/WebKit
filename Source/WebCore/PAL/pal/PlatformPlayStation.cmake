list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

list(APPEND PAL_LIBRARIES OpenSSL::Crypto)
