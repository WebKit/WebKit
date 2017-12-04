list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    system/glib/SleepDisablerGLib.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND PAL_SOURCES
        crypto/tasn1/Utilities.cpp
    )
endif ()
