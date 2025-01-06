list(APPEND WebKit_SOURCES
    Shared/API/c/skia/WKImageSkia.cpp

    UIProcess/Automation/skia/WebAutomationSessionSkia.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Shared/skia"
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/skia/WKImageSkia.h
)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/harfbuzz/WebCoreArgumentCodersHarfBuzz.serialization.in

    Shared/skia/CoreIPCSkColorSpace.serialization.in
    Shared/skia/CoreIPCSkData.serialization.in
    Shared/skia/CoreIPCSkFontStyleSlant.serialization.in
    Shared/skia/CoreIPCSkString.serialization.in
    Shared/skia/Skia.serialization.in
    Shared/skia/WebCoreArgumentCodersSkia.serialization.in
)

list(APPEND WebKit_LIBRARIES
    Skia::Skia
)
