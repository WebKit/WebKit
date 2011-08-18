/*
 * Copyright (C) 2010 Adam Barth. All Rights Reserved.
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
#include <wtf/text/StringBuilder.h>

namespace WebCore {

HTMLSourceTracker::HTMLSourceTracker()
{
}

void HTMLSourceTracker::start(const HTMLInputStream& input, HTMLToken& token)
{
    m_sourceFromPreviousSegments = token.type() == HTMLTokenTypes::Uninitialized ? String() : m_sourceFromPreviousSegments + m_source.toString();
    m_source = input.current();
    token.setBaseOffset(input.current().numberOfCharactersConsumed() - m_sourceFromPreviousSegments.length());
}

void HTMLSourceTracker::end(const HTMLInputStream& input, HTMLToken& token)
{
    m_cachedSourceForToken = String();
    // FIXME: This work should really be done by the HTMLTokenizer.
    token.end(input.current().numberOfCharactersConsumed());
}

String HTMLSourceTracker::sourceForToken(const HTMLToken& token)
{
    if (token.type() == HTMLTokenTypes::EndOfFile)
        return String(); // Hides the null character we use to mark the end of file.

    if (!m_cachedSourceForToken.isEmpty())
        return m_cachedSourceForToken;

    ASSERT(!token.startIndex());
    int length = token.endIndex() - token.startIndex();
    StringBuilder source;
    source.reserveCapacity(length);
    source.append(m_sourceFromPreviousSegments);
    for (int i = 0; i < length; ++i) {
        source.append(*m_source);
        m_source.advance();
    }
    m_cachedSourceForToken = source.toString();
    return m_cachedSourceForToken;
}

}
