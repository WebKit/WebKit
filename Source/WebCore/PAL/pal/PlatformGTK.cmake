list(APPEND PAL_SOURCES
    crypto/gcrypt/CryptoDigestGCrypt.cpp

    crypto/tasn1/Utilities.cpp

    system/gtk/SoundGtk.cpp

    text/KillRingNone.cpp
)

list(APPEND PAL_SYSTEM_INCLUDE_DIRECTORIES
    ${GDK_INCLUDE_DIRS}
)
