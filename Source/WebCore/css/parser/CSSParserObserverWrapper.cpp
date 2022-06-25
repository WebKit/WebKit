// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSParserObserverWrapper.h"

#include "CSSParserTokenRange.h"

namespace WebCore {

unsigned CSSParserObserverWrapper::startOffset(const CSSParserTokenRange& range)
{
    return m_tokenOffsets[range.begin() - m_firstParserToken];
}

unsigned CSSParserObserverWrapper::previousTokenStartOffset(const CSSParserTokenRange& range)
{
    if (range.begin() == m_firstParserToken)
        return 0;
    return m_tokenOffsets[range.begin() - m_firstParserToken - 1];
}

unsigned CSSParserObserverWrapper::endOffset(const CSSParserTokenRange& range)
{
    return m_tokenOffsets[range.end() - m_firstParserToken];
}

void CSSParserObserverWrapper::skipCommentsBefore(const CSSParserTokenRange& range, bool leaveDirectlyBefore)
{
    unsigned startIndex = range.begin() - m_firstParserToken;
    if (!leaveDirectlyBefore)
        startIndex++;
    while (m_commentIterator < m_commentOffsets.end() && m_commentIterator->tokensBefore < startIndex)
        m_commentIterator++;
}

void CSSParserObserverWrapper::yieldCommentsBefore(const CSSParserTokenRange& range)
{
    unsigned startIndex = range.begin() - m_firstParserToken;
    while (m_commentIterator < m_commentOffsets.end() && m_commentIterator->tokensBefore <= startIndex) {
        m_observer.observeComment(m_commentIterator->startOffset, m_commentIterator->endOffset);
        m_commentIterator++;
    }
}

} // namespace WebCore
