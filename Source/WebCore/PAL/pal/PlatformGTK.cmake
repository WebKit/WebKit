list(APPEND PAL_PUBLIC_HEADERS
    system/glib/SleepDisablerGLib.h
)

list(APPEND PAL_SOURCES
    system/ClockGeneric.cpp
    system/Sound.cpp

    system/glib/SleepDisablerGLib.cpp

    text/KillRing.cpp
)

if (USE_OPENSSL)
    list(APPEND PAL_SOURCES crypto/openssl/CryptoDigestOpenSSL.cpp)

    list(APPEND PAL_LIBRARIES OpenSSL::Crypto)
else ()
    list(APPEND PAL_PUBLIC_HEADERS
        crypto/gcrypt/Handle.h
        crypto/gcrypt/Initialization.h
        crypto/gcrypt/Utilities.h

        crypto/tasn1/Utilities.h
    )

    list(APPEND PAL_SOURCES
        crypto/gcrypt/CryptoDigestGCrypt.cpp

        crypto/tasn1/Utilities.cpp
    )
endif ()

list(APPEND PAL_LIBRARIES
    GLib::GLib
)
