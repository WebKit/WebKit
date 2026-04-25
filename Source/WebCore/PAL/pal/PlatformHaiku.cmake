add_definitions(-D_DEFAULT_SOURCE=1)

list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    system/ClockGeneric.cpp

    system/haiku/SoundHaiku.cpp

    text/KillRing.cpp
)

list(APPEND PAL_LIBRARIES crypto)
