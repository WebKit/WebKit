list(APPEND PAL_PUBLIC_HEADERS
)

list(APPEND PAL_SOURCES
    crypto/openssl/CryptoDigestOpenSSL.cpp
)

list(APPEND PAL_PRIVATE_LIBRARIES
    OpenSSL::Crypto
)
