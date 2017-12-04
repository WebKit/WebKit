list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    system/ClockGeneric.cpp

    system/glib/SleepDisablerGLib.cpp

    system/gtk/SoundGtk.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND PAL_SOURCES
        crypto/tasn1/Utilities.cpp
    )
endif ()

list(APPEND PAL_SYSTEM_INCLUDE_DIRECTORIES
    ${GDK_INCLUDE_DIRS}
)
