/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "UnrealizedCoreTextFont.h"

#include "FontCreationContext.h"
#include "FontDescription.h"

#include <optional>

namespace WebCore {

static std::optional<CGFloat> getCGFloatValue(CFNumberRef number)
{
    ASSERT(number);
    ASSERT(CFGetTypeID(number) == CFNumberGetTypeID());
    CGFloat value = 0;
    auto success = CFNumberGetValue(number, kCFNumberCGFloatType, &value);
    if (success && value)
        return value;
    return std::nullopt;
}

CGFloat UnrealizedCoreTextFont::getSize() const
{
    if (auto sizeAttribute = static_cast<CFNumberRef>(CFDictionaryGetValue(m_attributes.get(), kCTFontSizeAttribute))) {
        if (auto result = getCGFloatValue(sizeAttribute))
            return *result;
    }

    auto sizeAttribute = WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) {
        return adoptCF(static_cast<CFNumberRef>(CTFontCopyAttribute(font.get(), kCTFontSizeAttribute)));
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) {
        return adoptCF(static_cast<CFNumberRef>(CTFontDescriptorCopyAttribute(fontDescriptor.get(), kCTFontSizeAttribute)));
    });
    if (sizeAttribute) {
        if (auto result = getCGFloatValue(sizeAttribute.get()))
            return *result;
    }

    return 0;
}

RetainPtr<CTFontRef> UnrealizedCoreTextFont::realize() const
{
    return WTF::switchOn(m_baseFont, [this](const RetainPtr<CTFontRef>& font) -> RetainPtr<CTFontRef> {
        if (!font)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return font;
        auto modification = adoptCF(CTFontDescriptorCreateWithAttributes(m_attributes.get()));
        return adoptCF(CTFontCreateCopyWithAttributes(font.get(), getSize(), nullptr, modification.get()));
    }, [this](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> RetainPtr<CTFontRef> {
        if (!fontDescriptor)
            return nullptr;
        if (!CFDictionaryGetCount(m_attributes.get()))
            return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), getSize(), nullptr));
        auto updatedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), m_attributes.get()));
        return adoptCF(CTFontCreateWithFontDescriptor(updatedFontDescriptor.get(), getSize(), nullptr));
    });
}

UnrealizedCoreTextFont::operator bool() const
{
    return WTF::switchOn(m_baseFont, [](const RetainPtr<CTFontRef>& font) -> bool {
        return font;
    }, [](const RetainPtr<CTFontDescriptorRef>& fontDescriptor) -> bool {
        return fontDescriptor;
    });
}

static void modifyFromContext(CFMutableDictionaryRef attributes, const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, bool applyWeightWidthSlopeVariations)
{
    // FIXME: Implement this
    UNUSED_PARAM(attributes);
    UNUSED_PARAM(fontDescription);
    UNUSED_PARAM(fontCreationContext);
    UNUSED_PARAM(applyWeightWidthSlopeVariations);
}

void UnrealizedCoreTextFont::modifyFromContext(const FontDescription& fontDescription, const FontCreationContext& fontCreationContext, bool applyWeightWidthSlopeVariations)
{
    modify([&](CFMutableDictionaryRef attributes) {
        WebCore::modifyFromContext(attributes, fontDescription, fontCreationContext, applyWeightWidthSlopeVariations);
    });
}

} // namespace WebCore
