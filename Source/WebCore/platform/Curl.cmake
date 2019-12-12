list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/network/curl"
)

list(APPEND WebCore_SOURCES
    platform/network/curl/AuthenticationChallengeCurl.cpp
    platform/network/curl/CertificateInfoCurl.cpp
    platform/network/curl/CookieJarCurl.cpp
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
    platform/network/curl/OpenSSLHelper.cpp
    platform/network/curl/ProtectionSpaceCurl.cpp
    platform/network/curl/ProxyServerCurl.cpp
    platform/network/curl/PublicSuffixCurl.cpp
    platform/network/curl/ResourceErrorCurl.cpp
    platform/network/curl/ResourceHandleCurl.cpp
    platform/network/curl/ResourceRequestCurl.cpp
    platform/network/curl/ResourceResponseCurl.cpp
    platform/network/curl/SocketStreamHandleImplCurl.cpp
    platform/network/curl/SynchronousLoaderClientCurl.cpp
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/network/curl/AuthenticationChallenge.h
    platform/network/curl/CertificateInfo.h
    platform/network/curl/CookieJarCurl.h
    platform/network/curl/CookieJarDB.h
    platform/network/curl/CookieUtil.h
    platform/network/curl/CurlCacheEntry.h
    platform/network/curl/CurlCacheManager.h
    platform/network/curl/CurlContext.h
    platform/network/curl/CurlDownload.h
    platform/network/curl/CurlFormDataStream.h
    platform/network/curl/CurlMultipartHandle.h
    platform/network/curl/CurlMultipartHandleClient.h
    platform/network/curl/CurlProxySettings.h
    platform/network/curl/CurlRequest.h
    platform/network/curl/CurlRequestClient.h
    platform/network/curl/CurlRequestScheduler.h
    platform/network/curl/CurlRequestSchedulerClient.h
    platform/network/curl/CurlResourceHandleDelegate.h
    platform/network/curl/CurlResponse.h
    platform/network/curl/CurlSSLHandle.h
    platform/network/curl/CurlSSLVerifier.h
    platform/network/curl/DNSResolveQueueCurl.h
    platform/network/curl/DownloadBundle.h
    platform/network/curl/OpenSSLHelper.h
    platform/network/curl/ProtectionSpaceCurl.h
    platform/network/curl/ResourceError.h
    platform/network/curl/ResourceRequest.h
    platform/network/curl/ResourceResponse.h
    platform/network/curl/SocketStreamHandleImpl.h
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

list(APPEND WebCore_LIBRARIES
    LibPSL::LibPSL
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)
