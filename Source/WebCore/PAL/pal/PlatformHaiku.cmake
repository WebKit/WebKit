list(APPEND PAL_PUBLIC_HEADERS
    crypto/gcrypt/Handle.h
    crypto/gcrypt/Initialization.h
    crypto/gcrypt/Utilities.h
)

list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    crypto/tasn1/Utilities.cpp

    system/ClockGeneric.cpp

	system/haiku/SoundHaiku.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

if (ENABLE_WEB_CRYPTO)
    list(APPEND PAL_PUBLIC_HEADERS
        crypto/tasn1/Utilities.h
    )

    list(APPEND PAL_SOURCES
        crypto/tasn1/Utilities.cpp
    )
endif ()

