list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    system/SoundNone.cpp

    text/KillRingNone.cpp
)

if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND PAL_SOURCES
        crypto/tasn1/Utilities.cpp
    )
endif ()
