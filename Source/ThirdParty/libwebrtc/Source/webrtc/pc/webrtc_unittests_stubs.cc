// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Stubs for symbols referenced by webrtc_unittests' test helpers but not
// available in the WebKit-vendored libwebrtc tree.  Returning nullptr lets
// the link succeed; tests that exercise these paths need real implementations
// to pass (Phase-2 work).
//
// - TestAudioDeviceModule::Create / CreateDiscardRenderer / CreatePulsedNoiseCapturer:
//     test_audio_device.cc upstream depends on AudioDeviceModuleImpl, which is a
//     platform-specific class not in WebKit's libwebrtc API.
// - webrtc::test::CreateObjCDecoderFactory / CreateObjCEncoderFactory:
//     defined under sdk/objc/components/video_codec, which is iOS-only.

// - webrtc::CreateDav1dDecoder:
//     defined in modules/video_coding/codecs/av1/dav1d_decoder.cc which depends
//     on third_party/dav1d headers not vendored in WebKit's tree.

#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/test_audio_device.h"

namespace webrtc {

scoped_refptr<AudioDeviceModule> TestAudioDeviceModule::Create(
    const Environment&,
    std::unique_ptr<TestAudioDeviceModule::Capturer>,
    std::unique_ptr<TestAudioDeviceModule::Renderer>,
    float) {
    return nullptr;
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateDiscardRenderer(int, int) {
    return nullptr;
}

std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer>
TestAudioDeviceModule::CreatePulsedNoiseCapturer(int16_t, int, int) {
    return nullptr;
}

std::unique_ptr<VideoDecoder> CreateDav1dDecoder(const Environment&) {
    return nullptr;
}

namespace test {

std::unique_ptr<VideoDecoderFactory> CreateObjCDecoderFactory() {
    return nullptr;
}

std::unique_ptr<VideoEncoderFactory> CreateObjCEncoderFactory() {
    return nullptr;
}

}  // namespace test

}  // namespace webrtc

