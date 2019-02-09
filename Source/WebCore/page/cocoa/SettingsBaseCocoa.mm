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

void SettingsBase::initializeDefaultFontFamilies()
{
    setStandardFontFamily("Songti TC", USCRIPT_TRADITIONAL_HAN);
    setStandardFontFamily("Songti SC", USCRIPT_SIMPLIFIED_HAN);
    setStandardFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setStandardFontFamily("AppleMyungjo", USCRIPT_HANGUL);

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
    setStandardFontFamily(sansSerifTraditionalHanFontFamily(), USCRIPT_TRADITIONAL_HAN);
    setStandardFontFamily(sansSerifSimplifiedHanFontFamily(), USCRIPT_SIMPLIFIED_HAN);
    setStandardFontFamily("Hiragino Mincho ProN", USCRIPT_KATAKANA_OR_HIRAGANA);
    setStandardFontFamily("Apple SD Gothic Neo", USCRIPT_HANGUL);

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
