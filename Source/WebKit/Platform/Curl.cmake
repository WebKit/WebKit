list(APPEND WebKit_SOURCES
    NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

    NetworkProcess/cache/NetworkCacheDataCurl.cpp
    NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

    NetworkProcess/curl/NetworkDataTaskCurl.cpp
    NetworkProcess/curl/NetworkProcessCurl.cpp
    NetworkProcess/curl/NetworkProcessMainCurl.cpp
    NetworkProcess/curl/NetworkSessionCurl.cpp
    NetworkProcess/curl/WebSocketTaskCurl.cpp

    Shared/API/c/curl/WKCertificateInfoCurl.cpp

    UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

    WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp
)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/curl/WebCoreArgumentCodersCurl.serialization.in
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/curl"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/curl/WKCertificateInfoCurl.h

    UIProcess/API/C/curl/WKProtectionSpaceCurl.h
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.h
)
