/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FontCache.h"

#include <public/Platform.h>
#include <public/linux/WebFontFamily.h>
#include <public/linux/WebFontInfo.h>
#include <public/linux/WebSandboxSupport.h>

namespace WebCore {

void FontCache::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters, const char* preferredLocale, FontCache::SimpleFontFamily* family)
{
    WebKit::WebFontFamily webFamily;
    if (WebKit::Platform::current()->sandboxSupport())
        WebKit::Platform::current()->sandboxSupport()->getFontFamilyForCharacters(characters, numCharacters, preferredLocale, &webFamily);
    else
        WebKit::WebFontInfo::familyForChars(characters, numCharacters, preferredLocale, &webFamily);
    family->name = String::fromUTF8(webFamily.name.data(), webFamily.name.length());
    family->isBold = webFamily.isBold;
    family->isItalic = webFamily.isItalic;
}

}
