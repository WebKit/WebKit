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

#pragma once

#include "CSSParserObserver.h"

namespace WebCore {

class CSSParserObserverWrapper {
public:
    explicit CSSParserObserverWrapper(CSSParserObserver& observer)
        : m_observer(observer)
    { }

    unsigned startOffset(const CSSParserTokenRange&);
    unsigned previousTokenStartOffset(const CSSParserTokenRange&);
    unsigned endOffset(const CSSParserTokenRange&); // Includes trailing comments

    void skipCommentsBefore(const CSSParserTokenRange&, bool leaveDirectlyBefore);
    void yieldCommentsBefore(const CSSParserTokenRange&);

    CSSParserObserver& observer() { return m_observer; }
    void addComment(unsigned startOffset, unsigned endOffset, unsigned tokensBefore)
    {
        CommentPosition position = {startOffset, endOffset, tokensBefore};
        m_commentOffsets.append(position);
    }
    void addToken(unsigned startOffset) { m_tokenOffsets.append(startOffset); }
    void finalizeConstruction(CSSParserToken* firstParserToken)
    {
        m_firstParserToken = firstParserToken;
        m_commentIterator = m_commentOffsets.begin();
    }

private:
    CSSParserObserver& m_observer;
    Vector<unsigned> m_tokenOffsets;
    CSSParserToken* m_firstParserToken;

    struct CommentPosition {
        unsigned startOffset;
        unsigned endOffset;
        unsigned tokensBefore;
    };

    Vector<CommentPosition> m_commentOffsets;
    Vector<CommentPosition>::iterator m_commentIterator;
};

} // namespace WebCore
