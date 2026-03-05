/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Platform.h>
#if ENABLE(VP9) && PLATFORM(COCOA)

#include <WebCore/VP9Utilities.h>

namespace WebCore {

struct PlatformMediaCapabilitiesInfo;
struct PlatformMediaCapabilitiesVideoConfiguration;
class VideoInfo;

WEBCORE_EXPORT bool shouldEnableVP9Decoder();
WEBCORE_EXPORT bool shouldEnableSWVP9Decoder();
WEBCORE_EXPORT void registerWebKitVP9Decoder();
WEBCORE_EXPORT void registerSupplementalVP9Decoder();
bool isVP9DecoderAvailable();
WEBCORE_EXPORT bool vp9HardwareDecoderAvailable();
WEBCORE_EXPORT bool vp9HardwareDecoderAvailableInProcess();
WEBCORE_EXPORT void setVP9HardwareDecoderAvailableInProcess(bool);
bool isVP8DecoderAvailable();
bool isVPCodecConfigurationRecordSupported(const VPCodecConfigurationRecord&);
std::optional<PlatformMediaCapabilitiesInfo> validateVPParameters(const VPCodecConfigurationRecord&, const PlatformMediaCapabilitiesVideoConfiguration&);
std::optional<PlatformMediaCapabilitiesInfo> computeVPParameters(const PlatformMediaCapabilitiesVideoConfiguration&, bool vp9HardwareDecoderAvailable);
bool isVPSoftwareDecoderSmooth(const PlatformMediaCapabilitiesVideoConfiguration&);

struct VP8FrameHeader {
    bool keyframe { false };
    uint8_t version { 0 };
    bool showFrame { true };
    uint32_t partitionSize { 0 };
    uint8_t horizontalScale { 0 };
    uint16_t width { 0 };
    uint8_t verticalScale { 0 };
    uint16_t height;
    bool colorSpace { false };
    bool needsClamping { false };
};

std::optional<VP8FrameHeader> parseVP8FrameHeader(std::span<const uint8_t>);

class VP9TestingOverrides {
public:
    static WEBCORE_EXPORT VP9TestingOverrides& singleton();

    WEBCORE_EXPORT void setHardwareDecoderDisabled(std::optional<bool>&&);
    std::optional<bool> hardwareDecoderDisabled() const { return m_hardwareDecoderDisabled; }
    
    WEBCORE_EXPORT void setVP9HardwareDecoderEnabledOverride(std::optional<bool>&&);
    std::optional<bool> vp9HardwareDecoderEnabledOverride() const { return m_vp9HardwareDecoderEnabledOverride; }

    WEBCORE_EXPORT void setVP9DecoderDisabled(std::optional<bool>&&);
    std::optional<bool> vp9DecoderDisabled() const { return m_vp9DecoderDisabled; }

    WEBCORE_EXPORT void setVP9ScreenSizeAndScale(std::optional<ScreenDataOverrides>&&);
    std::optional<ScreenDataOverrides> vp9ScreenSizeAndScale() const { return m_screenSizeAndScale; }

    WEBCORE_EXPORT void setConfigurationChangedCallback(std::function<void(bool)>&&);
    WEBCORE_EXPORT void resetOverridesToDefaultValues();

    WEBCORE_EXPORT void setSWVPDecodersAlwaysEnabled(bool);
    bool swVPDecodersAlwaysEnabled() const { return m_swVPDecodersAlwaysEnabled; }

    WEBCORE_EXPORT void setShouldEnableVP9Decoder(bool);
    WEBCORE_EXPORT bool shouldEnableVP9Decoder() const;

private:
    std::optional<bool> m_hardwareDecoderDisabled;
    std::optional<bool> m_vp9DecoderDisabled;
    std::optional<bool> m_vp9HardwareDecoderEnabledOverride;
    std::optional<ScreenDataOverrides> m_screenSizeAndScale;
    Function<void(bool)> m_configurationChangedCallback;
    bool m_vp9DecoderEnabled { false };
    bool m_swVPDecodersAlwaysEnabled { false };
};

}

#endif
