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

#import "config.h"
#import "SettingsBase.h"

#import <wtf/NeverDestroyed.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/spi/ios/UIKitSPI.h>
#import <pal/system/ios/Device.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

#if PLATFORM(MAC)

void SettingsBase::initializeDefaultFontFamilies()
{
    setStandardFontFamily("Songti TC"_s, USCRIPT_TRADITIONAL_HAN);
    setStandardFontFamily("Songti SC"_s, USCRIPT_SIMPLIFIED_HAN);
    setStandardFontFamily("Hiragino Mincho ProN"_s, USCRIPT_KATAKANA_OR_HIRAGANA);
    setStandardFontFamily("AppleMyungjo"_s, USCRIPT_HANGUL);

    setStandardFontFamily("Times"_s, USCRIPT_COMMON);
    setFixedFontFamily("Courier"_s, USCRIPT_COMMON);
    setSerifFontFamily("Times"_s, USCRIPT_COMMON);
    setSansSerifFontFamily("Helvetica"_s, USCRIPT_COMMON);
}

#else

void SettingsBase::initializeDefaultFontFamilies()
{
    setStandardFontFamily("PingFang TC"_s, USCRIPT_TRADITIONAL_HAN);
    setStandardFontFamily("PingFang SC"_s, USCRIPT_SIMPLIFIED_HAN);
    setStandardFontFamily("Hiragino Mincho ProN"_s, USCRIPT_KATAKANA_OR_HIRAGANA);
    setStandardFontFamily("Apple SD Gothic Neo"_s, USCRIPT_HANGUL);

    setStandardFontFamily("Times"_s, USCRIPT_COMMON);
    setFixedFontFamily("Courier"_s, USCRIPT_COMMON);
    setSerifFontFamily("Times"_s, USCRIPT_COMMON);
    setSansSerifFontFamily("Helvetica"_s, USCRIPT_COMMON);
}

#endif

#if ENABLE(MEDIA_SOURCE)

bool SettingsBase::platformDefaultMediaSourceEnabled()
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

uint64_t SettingsBase::defaultMaximumSourceBufferSize()
{
#if PLATFORM(IOS_FAMILY)
    // iOS Devices have lower memory limits, enforced by jetsam rates, and a very limited
    // ability to swap. Allow SourceBuffers to store up to 105MB each, roughly a third of
    // the limit on macOS, and approximately equivalent to the limit on Firefox.
    return 110376422;
#endif
    // For other platforms, allow SourceBuffers to store up to 304MB each, enough for approximately five minutes
    // of 1080p video and stereo audio.
    return 318767104;
}

#endif

} // namespace WebCore
