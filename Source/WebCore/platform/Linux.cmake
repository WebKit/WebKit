list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/linux"
    "${WEBCORE_DIR}/platform/gamepad/linux"
)

list(APPEND WebCore_SOURCES
    platform/gamepad/linux/GamepadDeviceLinux.cpp
    platform/linux/MemoryPressureHandlerLinux.cpp
)
