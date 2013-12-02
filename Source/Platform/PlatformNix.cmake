set(Platform_INCLUDES
    nix
    ${CMAKE_BINARY_DIR} # For cmakeconfig.h
    ${WEBCORE_DIR}/platform/graphics/ # For IntRect.h
    ${WTF_DIR} # For config.h
)

set(Platform_HEADERS
    nix/public/AudioDestinationConsumer.h
    nix/public/AudioDevice.h
    nix/public/Canvas.h
    nix/public/Color.h
    nix/public/Common.h
    nix/public/FFTFrame.h
    nix/public/Gamepad.h
    nix/public/Gamepads.h
    nix/public/MediaConstraints.h
    nix/public/MediaPlayer.h
    nix/public/MediaStream.h
    nix/public/MediaStreamAudioSource.h
    nix/public/MediaStreamCenter.h
    nix/public/MediaStreamSource.h
    nix/public/MultiChannelPCMData.h
    nix/public/Platform.h
    nix/public/PrivatePtr.h
    nix/public/Rect.h
    nix/public/Size.h
    nix/public/ThemeEngine.h
)

set(Platform_SOURCES
    nix/src/DefaultWebThemeEngine.cpp
    nix/src/Platform.cpp
)

install(FILES ${Platform_HEADERS} DESTINATION include/${WebKit2_OUTPUT_NAME}-${PROJECT_VERSION_MAJOR}/NixPlatform)
