list(APPEND PAL_PUBLIC_HEADERS
    crypto/tasn1/Utilities.h

    system/glib/SleepDisablerGLib.h
)

list(APPEND PAL_SOURCES
    crypto/tasn1/Utilities.cpp

    system/ClockGeneric.cpp
    system/Sound.cpp

    system/glib/SleepDisablerGLib.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

if (USE_OPENSSL_BACKEND)
    list(APPEND PAL_SOURCES crypto/openssl/CryptoDigestOpenSSL.cpp)

    set(PAL_COMPILE_OPTIONS -DOPENSSL_API_COMPAT=0x10100000L)
else ()
    list(APPEND PAL_PUBLIC_HEADERS
        crypto/gcrypt/Handle.h
        crypto/gcrypt/Initialization.h
        crypto/gcrypt/Utilities.h
    )

    list(APPEND PAL_SOURCES crypto/gcrypt/CryptoDigestGCrypt.cpp)
endif ()

list(APPEND PAL_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
