include(PlatformIOS.cmake)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/ModelProcess/cocoa"
    "${WEBKIT_DIR}/Platform/spi/visionos"
    "${WEBKIT_DIR}/UIProcess/Cocoa/Separated"
    "${WEBKIT_DIR}/WebKitSwift"
    "${WEBKIT_DIR}/WebKitSwift/AVKit"
    "${WEBKIT_DIR}/WebKitSwift/LinearMediaKit"
    "${WEBKIT_DIR}/WebKitSwift/Preview"
    "${WEBKIT_DIR}/WebKitSwift/RealityKit"
    "${WEBKIT_DIR}/WebKitSwift/StageMode"
    "${WEBKIT_DIR}/WebKitSwift/TextAnimation"
    "${CMAKE_BINARY_DIR}/WebKitAdditions-staging/WebKitAdditions"
)

list(APPEND WebKit_SWIFT_EXTRA_OPTIONS
    "-Xcc" "-I${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}"
)

target_include_directories(WebKitSwift PRIVATE
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)
