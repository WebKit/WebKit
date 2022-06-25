/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "FontPaletteValues.h"
#include "FontSelectionAlgorithm.h"
#include "FontTaggedSettings.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

class FontCreationContextRareData : public RefCounted<FontCreationContextRareData> {
public:
    static Ref<FontCreationContextRareData> create(const FontFeatureSettings& fontFaceFeatures, const FontPaletteValues& fontPaletteValues)
    {
        return adoptRef(*new FontCreationContextRareData(fontFaceFeatures, fontPaletteValues));
    }

    const FontFeatureSettings& fontFaceFeatures() const
    {
        return m_fontFaceFeatures;
    }

    const FontPaletteValues& fontPaletteValues() const
    {
        return m_fontPaletteValues;
    }

    bool operator==(const FontCreationContextRareData& other) const
    {
        return m_fontFaceFeatures == other.m_fontFaceFeatures
            && m_fontPaletteValues == other.m_fontPaletteValues;
    }

    bool operator!=(const FontCreationContextRareData& other) const
    {
        return !(*this == other);
    }

private:
    FontCreationContextRareData(const FontFeatureSettings& fontFaceFeatures, const FontPaletteValues& fontPaletteValues)
        : m_fontFaceFeatures(fontFaceFeatures)
        , m_fontPaletteValues(fontPaletteValues)
    {
    }

    FontFeatureSettings m_fontFaceFeatures;
    FontPaletteValues m_fontPaletteValues;
    // FIXME: Add support for font-feature-values.
};

class FontCreationContext {
public:
    FontCreationContext() = default;

    FontCreationContext(const FontFeatureSettings& fontFaceFeatures, const FontSelectionSpecifiedCapabilities& fontFaceCapabilities, const FontPaletteValues& fontPaletteValues)
        : m_fontFaceCapabilities(fontFaceCapabilities)
    {
        if (!fontFaceFeatures.isEmpty() || fontPaletteValues)
            m_rareData = FontCreationContextRareData::create(fontFaceFeatures, fontPaletteValues);
    }

    const FontFeatureSettings* fontFaceFeatures() const
    {
        return m_rareData ? &m_rareData->fontFaceFeatures() : nullptr;
    }

    const FontSelectionSpecifiedCapabilities& fontFaceCapabilities() const
    {
        return m_fontFaceCapabilities;
    }

    const FontPaletteValues* fontPaletteValues() const
    {
        return m_rareData ? &m_rareData->fontPaletteValues() : nullptr;
    }

    bool operator==(const FontCreationContext& other) const
    {
        return m_fontFaceCapabilities == other.m_fontFaceCapabilities
            && arePointingToEqualData(m_rareData, other.m_rareData);
    }

    bool operator!=(const FontCreationContext& other) const
    {
        return !(*this == other);
    }

private:
    FontSelectionSpecifiedCapabilities m_fontFaceCapabilities;
    RefPtr<FontCreationContextRareData> m_rareData;
};

inline void add(Hasher& hasher, const FontCreationContext& fontCreationContext)
{
    if (fontCreationContext.fontFaceFeatures())
        add(hasher, *fontCreationContext.fontFaceFeatures());
    add(hasher, fontCreationContext.fontFaceCapabilities().tied());
    if (fontCreationContext.fontPaletteValues())
        add(hasher, *fontCreationContext.fontPaletteValues());
}

}
