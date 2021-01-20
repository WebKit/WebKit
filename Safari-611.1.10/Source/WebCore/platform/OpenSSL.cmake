if (ENABLE_WEB_CRYPTO)
    list(APPEND WebCore_SOURCES
        crypto/openssl/CryptoAlgorithmAES_CBCOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmAES_CFBOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmAES_CTROpenSSL.cpp
        crypto/openssl/CryptoAlgorithmAES_GCMOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmAES_KWOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmECDHOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmECDSAOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmHKDFOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmHMACOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmPBKDF2OpenSSL.cpp
        crypto/openssl/CryptoAlgorithmRSAES_PKCS1_v1_5OpenSSL.cpp
        crypto/openssl/CryptoAlgorithmRSASSA_PKCS1_v1_5OpenSSL.cpp
        crypto/openssl/CryptoAlgorithmRSA_OAEPOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmRSA_PSSOpenSSL.cpp
        crypto/openssl/CryptoAlgorithmRegistryOpenSSL.cpp
        crypto/openssl/CryptoKeyECOpenSSL.cpp
        crypto/openssl/CryptoKeyRSAOpenSSL.cpp
        crypto/openssl/SerializedCryptoKeyWrapOpenSSL.cpp
    )
endif ()
