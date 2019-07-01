list(APPEND PAL_PUBLIC_HEADERS
)

list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp

    crypto/win/CryptoDigestWin.cpp
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_LIBRARIES_DIR}/include"
)
