list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/linux"
)

list(APPEND WebCore_SOURCES
    platform/linux/GamepadDeviceLinux.cpp
    platform/linux/MemoryPressureHandlerLinux.cpp
)
