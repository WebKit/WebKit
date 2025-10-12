/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(CORE_MATERIAL)

#include <WebCore/FloatRoundedRect.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class AppleVisualEffect : uint8_t {
    None,
    BlurUltraThinMaterial,
    BlurThinMaterial,
    BlurMaterial,
    BlurThickMaterial,
    BlurChromeMaterial,
#if HAVE(MATERIAL_HOSTING)
    GlassMaterial,
    GlassClearMaterial,
    GlassSubduedMaterial,
    GlassMediaControlsMaterial,
    GlassSubduedMediaControlsMaterial,
#endif
    VibrancyLabel,
    VibrancySecondaryLabel,
    VibrancyTertiaryLabel,
    VibrancyQuaternaryLabel,
    VibrancyFill,
    VibrancySecondaryFill,
    VibrancyTertiaryFill,
    VibrancySeparator,
};

WEBCORE_EXPORT bool appleVisualEffectNeedsBackdrop(AppleVisualEffect);
WEBCORE_EXPORT bool appleVisualEffectAppliesFilter(AppleVisualEffect);
#if HAVE(MATERIAL_HOSTING)
WEBCORE_EXPORT bool appleVisualEffectIsHostedMaterial(AppleVisualEffect);
#endif

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, AppleVisualEffect);

struct AppleVisualEffectData {
    AppleVisualEffect effect { AppleVisualEffect::None };
    AppleVisualEffect contextEffect { AppleVisualEffect::None };

    enum class ColorScheme : uint8_t { Light, Dark };
    ColorScheme colorScheme { ColorScheme::Light };

    std::optional<FloatRoundedRect> borderRect;

    bool operator==(const AppleVisualEffectData&) const = default;
};

WTF::TextStream& operator<<(WTF::TextStream&, AppleVisualEffectData::ColorScheme);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, AppleVisualEffectData);

} // namespace WebCore

#endif // HAVE(CORE_MATERIAL)
