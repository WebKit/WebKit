list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/network/curl"
)

list(APPEND WebCore_SOURCES
    platform/network/curl/CookieJarCurl.cpp
    platform/network/curl/CredentialStorageCurl.cpp
    platform/network/curl/CurlCacheEntry.cpp
    platform/network/curl/CurlCacheManager.cpp
    platform/network/curl/CurlContext.cpp
    platform/network/curl/CurlDownload.cpp
    platform/network/curl/CurlJobManager.cpp
    platform/network/curl/DNSCurl.cpp
    platform/network/curl/FormDataStreamCurl.cpp
    platform/network/curl/MultipartHandle.cpp
    platform/network/curl/ProxyServerCurl.cpp
    platform/network/curl/ResourceHandleCurl.cpp
    platform/network/curl/SSLHandle.cpp
    platform/network/curl/SocketStreamHandleImplCurl.cpp
    platform/network/curl/SynchronousLoaderClientCurl.cpp
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
