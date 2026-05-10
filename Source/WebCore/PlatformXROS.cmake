include(PlatformIOS.cmake)

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/html/shadow/spatialImageControls.css
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    platform/graphics/cg/SpatialImageTypes.h

    platform/graphics/cocoa/ShareableGainMap.h
)

set(CSS_VALUE_PLATFORM_DEFINES "${CSS_VALUE_PLATFORM_DEFINES} HAVE_CORE_ANIMATION_SEPARATED_LAYERS")

# libwebrtc is a static library in the CMake build but a dylib in the Xcode build.
# webrtc_voice_engine.cc references CreateAudioDeviceModule which requires
# audio_device_impl.cc / audio_device_mac.cc (macOS-only). This code is dead on
# visionOS, so allow the symbol to be undefined.
target_link_options(WebCore PRIVATE
    "LINKER:-U,__ZN6webrtc23CreateAudioDeviceModuleERKNS_11EnvironmentENS_17AudioDeviceModule10AudioLayerE"
)
