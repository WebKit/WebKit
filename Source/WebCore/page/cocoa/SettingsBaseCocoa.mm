/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "SettingsBase.h"

#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS_FAMILY)
#include "Device.h"
#include <pal/ios/UIKitSoftLink.h>
#include <pal/spi/ios/UIKitSPI.h>
#endif

namespace WebCore {

static inline const char* sansSerifTraditionalHanFontFamily()
{
    return "PingFang TC";
}

static inline const char* sansSerifSimplifiedHanFontFamily()
{
    return "PingFang SC";
}

#if PLATFORM(MAC)

static bool osakaMonoIsInstalled()
{
    int one = 1;
    RetainPtr<CFNumberRef> yes = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one));
    CFTypeRef keys[] = { kCTFontEnabledAttribute, kCTFontNameAttribute };
    CFTypeRef values[] = { yes.get(), CFSTR("Osaka-Mono") };
    RetainPtr<CFDictionaryRef> attributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys, values, WTF_ARRAY_LENGTH(values), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr<CTFontDescriptorRef> descriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
    RetainPtr<CFSetRef> mandatoryAttributes = adoptCF(CFSetCreate(kCFAllocatorDefault, keys, WTF_ARRAY_LENGTH(keys), &kCFTypeSetCallBacks));
    return adoptCF(CTFontDescriptorCreateMatchingFontDescriptor(descriptor.get(), mandatoryAttributes.get()));
}

void SettingsBase::initializeDefaultFontFamilies()
{
    setStandardFontFamily("Songti TC", USCRIPT_TRADITIONAL_HAN);
    setSerifFontFamily("Songti TC", USCRIPT_TRADITIONAL_HAN);
    setFixedFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setSansSerifFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setCursiveFontFamily("Kaiti TC", USCRIPT_TRADITIONAL_HAN);

    setStandardFontFamily("Songti SC", USCRIPT_SIMPLIFIED_HAN);
    setSerifFontFamily("Songti SC", USCRIPT_SIMPLIFIED_HAN);
    setFixedFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setSansSerifFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setCursiveFontFamily("Kaiti SC", USCRIPT_SIMPLIFIED_HAN);

    setStandardFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setFixedFontFamily(osakaMonoIsInstalled() ? "Osaka-Mono" : "Hiragino Sans", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSerifFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSansSerifFontFamily("Hiragino Kaku Gothic ProN", USCRIPT_KATAKANA_OR_HIRAGANA);

    setStandardFontFamily("AppleMyungjo", USCRIPT_HANGUL);
    setSerifFontFamily("AppleMyungjo", USCRIPT_HANGUL);
    setFixedFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);
    setSansSerifFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);

    setStandardFontFamily("Times", USCRIPT_COMMON);
    setFixedFontFamily("Courier", USCRIPT_COMMON);
    setSerifFontFamily("Times", USCRIPT_COMMON);
    setSansSerifFontFamily("Helvetica", USCRIPT_COMMON);
}

bool SettingsBase::platformDefaultMediaSourceEnabled()
{
    return true;
}

#else

void SettingsBase::initializeDefaultFontFamilies()
{
    // There is no serif Chinese font in default iOS installation.
    setStandardFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setSerifFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setFixedFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setSansSerifFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);

    // There is no serif Chinese font in default iOS installation.
    setStandardFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setSerifFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setFixedFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setSansSerifFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);

    setStandardFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setFixedFontFamily("Hiragino Kaku Gothic ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSerifFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setSansSerifFontFamily("Hiragino Kaku Gothic ProN", USCRIPT_KATAKANA_OR_HIRAGANA);

    // There is no serif Korean font in default iOS installation.
    setStandardFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);
    setSerifFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);
    setFixedFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);
    setSansSerifFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);

    setStandardFontFamily("Times", USCRIPT_COMMON);
    setFixedFontFamily("Courier", USCRIPT_COMMON);
    setSerifFontFamily("Times", USCRIPT_COMMON);
    setSansSerifFontFamily("Helvetica", USCRIPT_COMMON);
}

bool SettingsBase::defaultTextAutosizingEnabled()
{
    return !deviceHasIPadCapability() || [[PAL::getUIApplicationClass() sharedApplication] _isClassic];
}

#endif

const String& SettingsBase::defaultMediaContentTypesRequiringHardwareSupport()
{
    static NeverDestroyed<String> defaultMediaContentTypes { "video/mp4;codecs=hvc1:video/mp4;codecs=hev1" };
    return defaultMediaContentTypes;
}

} // namespace WebCore
