/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GlyphMap.h"
#include "FontData.h"
#include "WebCoreSystemInterface.h"

namespace WebCore
{

bool GlyphMap::fillPage(GlyphPage* page, UChar* buffer, unsigned bufferLength, const FontData* fontData)
{
    // Use an array of long so we get good enough alignment.
    long glyphVector[(GLYPH_VECTOR_SIZE + sizeof(long) - 1) / sizeof(long)];
    
    OSStatus status = wkInitializeGlyphVector(cGlyphPageSize, &glyphVector);
    if (status != noErr)
        // This should never happen, perhaps indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        return false;

    wkConvertCharToGlyphs(fontData->m_styleGroup, buffer, bufferLength, &glyphVector);

    unsigned numGlyphs = wkGetGlyphVectorNumGlyphs(&glyphVector);
    if (numGlyphs != cGlyphPageSize) {
        // This should never happen, perhaps indicates a bad font?
        // If it does happen, the font substitution code will find an alternate font.
        wkClearGlyphVector(&glyphVector);
        return false;
    }

    ATSLayoutRecord* glyphRecord = (ATSLayoutRecord*)wkGetGlyphVectorFirstRecord(glyphVector);
    for (unsigned i = 0; i < cGlyphPageSize; i++) {
        page->setGlyphDataForIndex(i, glyphRecord->glyphID, fontData);
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + wkGetGlyphVectorRecordSize(glyphVector));
    }
    wkClearGlyphVector(&glyphVector);

    return true;
}

}