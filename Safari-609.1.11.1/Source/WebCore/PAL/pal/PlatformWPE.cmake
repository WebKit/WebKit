list(APPEND PAL_PUBLIC_HEADERS
    crypto/gcrypt/Handle.h
    crypto/gcrypt/Initialization.h
    crypto/gcrypt/Utilities.h

    system/glib/SleepDisablerGLib.h
)

list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    system/glib/SleepDisablerGLib.cpp

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

list(APPEND PAL_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
