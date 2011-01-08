/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GlyphPageTreeNode.h"

#include "SimpleFontData.h"
#include <wtf/Assertions.h>
#include <wtf/unicode/Unicode.h>


namespace WebCore {

bool GlyphPage::fill(unsigned offset, unsigned length, UChar* characterBuffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    bool isUtf16 = bufferLength != GlyphPage::size;
    bool haveGlyphs = false;

    for (unsigned i = 0; i < GlyphPage::size; i++) {
        UChar32 character;

        if (isUtf16) {
            UChar lead = characterBuffer[i * 2];
            UChar trail = characterBuffer[i * 2 + 1];
            character = U16_GET_SUPPLEMENTARY(lead, trail);
        } else
            character = characterBuffer[i];

        if (!character)
            setGlyphDataForIndex(offset + i, 0, 0);
        else {
            haveGlyphs = true;
            setGlyphDataForIndex(offset + i, character, fontData);
        }
    }

    return haveGlyphs;
}

} // namespace WebCore

