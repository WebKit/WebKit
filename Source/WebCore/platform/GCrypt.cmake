if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND WebCore_SOURCES
        crypto/gcrypt/CryptoAlgorithmAES_CBCGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmAES_CFBGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmAES_CTRGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmAES_GCMGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmAES_KWGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmECDHGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmECDSAGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmHKDFGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmHMACGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmPBKDF2GCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmRSAES_PKCS1_v1_5GCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmRSASSA_PKCS1_v1_5GCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmRSA_OAEPGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmRSA_PSSGCrypt.cpp
        crypto/gcrypt/CryptoAlgorithmRegistryGCrypt.cpp
        crypto/gcrypt/CryptoKeyECGCrypt.cpp
        crypto/gcrypt/CryptoKeyRSAGCrypt.cpp
        crypto/gcrypt/SerializedCryptoKeyWrapGCrypt.cpp
    )
endif ()

list(APPEND WebCore_LIBRARIES
    ${LIBGCRYPT_LIBRARIES}
)
list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${LIBGCRYPT_INCLUDE_DIRS}
)
