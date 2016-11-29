/*
 * Copyright (C) 2010 Adam Barth. All Rights Reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLSourceTracker.h"

#include "HTMLTokenizer.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void HTMLSourceTracker::startToken(SegmentedString& currentInput, HTMLTokenizer& tokenizer)
{
    if (!m_started) {
        if (tokenizer.numberOfBufferedCharacters())
            m_previousSource = tokenizer.bufferedCharacters();
        else
            m_previousSource.clear();
        m_started = true;
    } else
        m_previousSource.append(m_currentSource);

    m_currentSource = currentInput;
    m_tokenStart = m_currentSource.numberOfCharactersConsumed() - m_previousSource.length();
    tokenizer.setTokenAttributeBaseOffset(m_tokenStart);
}

void HTMLSourceTracker::endToken(SegmentedString& currentInput, HTMLTokenizer& tokenizer)
{
    ASSERT(m_started);
    m_started = false;

    m_tokenEnd = currentInput.numberOfCharactersConsumed() - tokenizer.numberOfBufferedCharacters();
    m_cachedSourceForToken = String();
}

String HTMLSourceTracker::source(const HTMLToken& token)
{
    ASSERT(!m_started);

    if (token.type() == HTMLToken::EndOfFile)
        return String(); // Hides the null character we use to mark the end of file.

    if (!m_cachedSourceForToken.isEmpty())
        return m_cachedSourceForToken;

    unsigned length = m_tokenEnd - m_tokenStart;

    StringBuilder source;
    source.reserveCapacity(length);

    unsigned i = 0;
    for ( ; i < length && !m_previousSource.isEmpty(); ++i) {
        source.append(m_previousSource.currentCharacter());
        m_previousSource.advance();
    }
    for ( ; i < length; ++i) {
        ASSERT(!m_currentSource.isEmpty());
        source.append(m_currentSource.currentCharacter());
        m_currentSource.advance();
    }

    m_cachedSourceForToken = source.toString();
    return m_cachedSourceForToken;
}

String HTMLSourceTracker::source(const HTMLToken& token, unsigned attributeStart, unsigned attributeEnd)
{
    return source(token).substring(attributeStart, attributeEnd - attributeStart);
}

}
