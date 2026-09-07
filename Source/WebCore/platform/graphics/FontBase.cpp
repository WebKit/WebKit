
/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontBase.h"

#if ENABLE(MATHML)
#include "OpenTypeMathData.h"
#endif

namespace WebCore {

FontBase::FontBase(const FontPlatformData& platformData, Origin origin, IsInterstitial interstitial, Visibility visibility, IsOrientationFallback orientationFallback, std::optional<RenderingResourceIdentifier> renderingResourceIdentifier)
    : m_platformData(platformData)
    , m_attributes({ renderingResourceIdentifier, origin, interstitial, visibility, orientationFallback })
{
    platformInit();
}

FontBase::~FontBase() = default;

FontBase::FontBase(IsSystemFallbackFontPlaceholder isSystemFontFallbackPlaceholder)
    : m_isSystemFontFallbackPlaceholder(isSystemFontFallbackPlaceholder == IsSystemFallbackFontPlaceholder::Yes)
{
    // This ctor is to be used only for representing a system font fallback placeholder (createSystemFallbackFontPlaceholder)
    ASSERT(isSystemFontFallbackPlaceholder == IsSystemFallbackFontPlaceholder::Yes);
}

#if ENABLE(MATHML)
const OpenTypeMathData* FontBase::mathData() const
{
    if (isInterstitial())
        return nullptr;
    if (!m_mathData) {
        Ref mathData = OpenTypeMathData::create(m_platformData);
        if (mathData->hasMathData())
            m_mathData = WTF::move(mathData);
    }
    return m_mathData.get();
}
#endif

RenderingResourceIdentifier FontInternalAttributes::ensureRenderingResourceIdentifier() const
{
    if (!renderingResourceIdentifier)
        renderingResourceIdentifier = RenderingResourceIdentifier::generate();
    return *renderingResourceIdentifier;
}

RenderingResourceIdentifier FontBase::renderingResourceIdentifier() const
{
    return m_attributes.ensureRenderingResourceIdentifier();
}

} // namespace WebCore
