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
#include "TextSpacing.h"

#if USE(CORE_TEXT)
#include "CoreTextCompositionEngine.h"
#endif
#include "GlyphBuffer.h"
#include "TextRun.h"
#include <wtf/Forward.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

TextSpacingWidth TextSpacingEngine::processTextSpacingAndUpdateState(unsigned glyphBufferIndex, const TextSpacingWidth& width)
{
    if (!m_state)
        return { };
    // Left width is to be added to the left of next glyph on next iteration, so we save it on text-spacing engine state. The right width is to be added to the current glyph on the current iteration. We should also add the left width of the last iteration to the current glyph.
    auto leftWidth = m_state->leftoverWidth;
    auto rightWidth = width.rightWidth;

    // Do we need to differentiate between LTR and RTL if this is for CJK only?
    if (leftWidth)
        m_glyphBuffer.expandAdvanceToLeft(glyphBufferIndex, leftWidth);
    if (rightWidth)
        m_glyphBuffer.expandAdvance(glyphBufferIndex, rightWidth);

    m_state->leftoverWidth = width.leftWidth;
    return { leftWidth, rightWidth };
}

TextSpacingWidth TextSpacingEngine::adjustTextSpacing(unsigned currentCharacterIndex, unsigned glyphIndex)
{
    if (!m_state)
        return { };
    // That's how calculateAdditionalWidth currently maps back to character. But will that work for surrogated pairs?
    UChar character = m_run[currentCharacterIndex];
    UChar nextCharacter = currentCharacterIndex + 1 < m_run.length() ? m_run[currentCharacterIndex + 1] : m_state->firstCharacterOfNextRun;
    // FIXME: get script language
    auto language = TextSpacingSupportedLanguage::Japanese;
    auto width = calculateCJKAdjustingSpacing(language, character, nextCharacter);
    return processTextSpacingAndUpdateState(glyphIndex, width);
}

#if !USE(CORE_TEXT)
// dummy implementation for other platforms
TextSpacingWidth calculateCJKAdjustingSpacing(TextSpacingSupportedLanguage language, UChar32 currentCharacter, UChar32 nextCharacter)
{
    // FIXME: implement CJK composition engine adjusting spacing so we can calculate the spacing to be added between characters rdar://105189659
    UNUSED_PARAM(language);
    UNUSED_PARAM(currentCharacter);
    UNUSED_PARAM(nextCharacter);
    return { };
}
#endif
} // namespace WebCore
