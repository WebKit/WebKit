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

#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ComplexTextController.h>
#include <WebCore/FontCascade.h>
#include <WebCore/WidthIterator.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace TestWebKitAPI {

#if PLATFORM(COCOA)

// FIXME when rdar://119399924 is resolved.
#if PLATFORM(MAC)
TEST(MonospaceFontsTest, DISABLED_EnsureMonospaceFontInvariants)
#else
TEST(MonospaceFontsTest, EnsureMonospaceFontInvariants)
#endif
{
    RetainPtr collection = CTFontCollectionCreateFromAvailableFonts(nullptr);
    RetainPtr results = adoptCF(CTFontCollectionCreateMatchingFontDescriptors(collection.get()));
    if (results) {
        for (unsigned i = 0, count = CFArrayGetCount(results.get()); i < count; ++i) {
            RetainPtr fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(results.get(), i));
            auto ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 16.0, nullptr));
            FontPlatformData platformData(ctFont.get(), 16.0);
            FontCascade fontCascade(platformData);
            if (fontCascade.canTakeFixedPitchFastContentMeasuring()) {
                for (uint32_t character = 0; character <= UINT16_MAX; ++character) {
                    const char16_t ch = character;
                    StringView content(span(ch));
                    if (fontCascade.codePath(TextRun(content)) == FontCascade::CodePath::Complex)
                        continue;
                    constexpr bool whitespaceIsCollapsed = false;
                    if (!WidthIterator::characterCanUseSimplifiedTextMeasuring(character, whitespaceIsCollapsed))
                        continue;
                    auto glyphData = fontCascade.glyphDataForCharacter(character, false);
                    if (!glyphData.isValid() || glyphData.font != &fontCascade.primaryFont())
                        continue;
                    fontCascade.fonts()->widthCache().clear();
                    float width = fontCascade.widthForSimpleTextWithFixedPitch(content, whitespaceIsCollapsed);
                    fontCascade.fonts()->widthCache().clear();
                    float originalWidth = fontCascade.widthForTextUsingSimplifiedMeasuring(content);
                    EXPECT_EQ(originalWidth , width);
                }
                {
                    const char16_t characters[] {
                        ' ', ' ', 'a'
                    };
                    StringView content(characters);
                    constexpr bool whitespaceIsCollapsed = false;
                    fontCascade.fonts()->widthCache().clear();
                    float width = fontCascade.widthForSimpleTextWithFixedPitch(content, whitespaceIsCollapsed);
                    fontCascade.fonts()->widthCache().clear();
                    float originalWidth = fontCascade.widthForTextUsingSimplifiedMeasuring(content);
                    EXPECT_EQ(originalWidth , width);
                }
            }
        }
    }
}

#endif

}
