include(PlatformIOS.cmake)

list(APPEND PAL_PUBLIC_HEADERS
    cocoa/CompositorServicesSoftLink.h

    spi/cocoa/CompositorServicesSPI.h
)

list(APPEND PAL_SOURCES
    cocoa/CompositorServicesSoftLink.mm
)
