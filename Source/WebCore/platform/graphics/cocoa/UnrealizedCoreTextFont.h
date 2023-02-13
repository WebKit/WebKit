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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <variant>
#include <wtf/RetainPtr.h>

namespace WebCore {

class FontCreationContext;
class FontDescription;

class UnrealizedCoreTextFont {
public:
    UnrealizedCoreTextFont(RetainPtr<CTFontRef>&& baseFont)
        : m_baseFont(WTFMove(baseFont))
    {
    }

    UnrealizedCoreTextFont(RetainPtr<CTFontDescriptorRef>&& baseFont)
        : m_baseFont(WTFMove(baseFont))
    {
    }

    template <typename T>
    void modify(T&& functor)
    {
        if (static_cast<bool>(*this))
            functor(m_attributes.get());
    }

    void setSize(CGFloat size)
    {
        if (static_cast<bool>(*this))
            CFDictionarySetValue(m_attributes.get(), kCTFontSizeAttribute, adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &size)).get());
    }

    operator bool() const;

    void modifyFromContext(const FontDescription&, const FontCreationContext&, bool applyWeightWidthSlopeVariations = true);
    RetainPtr<CTFontRef> realize() const;

private:
    CGFloat getSize() const;

    std::variant<RetainPtr<CTFontRef>, RetainPtr<CTFontDescriptorRef>> m_baseFont;
    RetainPtr<CFMutableDictionaryRef> m_attributes { adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) };
};

} // namespace WebCore
