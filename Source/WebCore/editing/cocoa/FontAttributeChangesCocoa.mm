/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "FontAttributeChanges.h"

#if PLATFORM(COCOA)

#import "Font.h"
#import "FontCache.h"
#import "FontDescription.h"

namespace WebCore {

const String& FontChanges::platformFontFamilyNameForCSS() const
{
    // The family name may not be specific enough to get us the font specified.
    // In some cases, the only way to get exactly what we are looking for is to use
    // the Postscript name. If we don't find a font with the same Postscript name,
    // then we'll have to use the Postscript name to make the CSS specific enough.
    // This logic was originally from WebHTMLView, and is now in WebCore so that it can
    // be shared between WebKitLegacy and WebKit.
    auto cfFontName = m_fontName.createCFString();
    RetainPtr<CFStringRef> fontNameFromDescription;

    FontDescription description;
    description.setIsItalic(m_italic.valueOr(false));
    description.setWeight(FontSelectionValue { m_bold.valueOr(false) ? 900 : 500 });
    if (auto font = FontCache::singleton().fontForFamily(description, m_fontFamily))
        fontNameFromDescription = adoptCF(CTFontCopyPostScriptName(font->getCTFont()));

    if (fontNameFromDescription && CFStringCompare(cfFontName.get(), fontNameFromDescription.get(), 0) == kCFCompareEqualTo)
        return m_fontFamily;

    return m_fontName;
}

} // namespace WebCore

#endif
