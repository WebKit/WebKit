list(APPEND PAL_PUBLIC_HEADERS
    crypto/gcrypt/Handle.h
    crypto/gcrypt/Initialization.h
    crypto/gcrypt/Utilities.h

    crypto/tasn1/Utilities.h

    system/glib/SleepDisablerGLib.h
)

list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    crypto/tasn1/Utilities.cpp

    system/ClockGeneric.cpp

    system/glib/SleepDisablerGLib.cpp

    system/gtk/SoundGtk.cpp

    text/KillRing.cpp

    unix/LoggingUnix.cpp
)

list(APPEND PAL_LIBRARIES
    GTK::GTK
)
