list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/network/curl"
)

list(APPEND WebCore_SOURCES
    platform/network/curl/AuthenticationChallengeCurl.cpp
    platform/network/curl/CertificateInfo.cpp
    platform/network/curl/CookieJarCurlDatabase.cpp
    platform/network/curl/CookieJarDB.cpp
    platform/network/curl/CookieStorageCurl.cpp
    platform/network/curl/CookieUtil.cpp
    platform/network/curl/CredentialStorageCurl.cpp
    platform/network/curl/CurlCacheEntry.cpp
    platform/network/curl/CurlCacheManager.cpp
    platform/network/curl/CurlContext.cpp
    platform/network/curl/CurlDownload.cpp
    platform/network/curl/CurlFormDataStream.cpp
    platform/network/curl/CurlMultipartHandle.cpp
    platform/network/curl/CurlProxySettings.cpp
    platform/network/curl/CurlRequest.cpp
    platform/network/curl/CurlRequestScheduler.cpp
    platform/network/curl/CurlResourceHandleDelegate.cpp
    platform/network/curl/CurlSSLHandle.cpp
    platform/network/curl/CurlSSLVerifier.cpp
    platform/network/curl/DNSResolveQueueCurl.cpp
    platform/network/curl/NetworkStorageSessionCurl.cpp
    platform/network/curl/ProxyServerCurl.cpp
    platform/network/curl/PublicSuffixCurl.cpp
    platform/network/curl/ResourceErrorCurl.cpp
    platform/network/curl/ResourceHandleCurl.cpp
    platform/network/curl/ResourceResponseCurl.cpp
    platform/network/curl/SocketStreamHandleImplCurl.cpp
    platform/network/curl/SynchronousLoaderClientCurl.cpp
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${CURL_INCLUDE_DIRS}
    ${LIBPSL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    ${CURL_LIBRARIES}
    ${LIBPSL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
