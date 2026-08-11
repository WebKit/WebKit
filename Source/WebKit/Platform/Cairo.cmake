list(APPEND WebKit_SOURCES
    Shared/API/c/cairo/WKImageCairo.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/cairo/WKImageCairo.h
)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/cairo/WebCoreFontCairo.serialization.in
)

list(APPEND WebKit_LIBRARIES
    Cairo::Cairo
)
