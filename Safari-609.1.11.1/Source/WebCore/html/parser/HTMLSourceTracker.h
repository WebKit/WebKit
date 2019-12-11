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

#pragma once

#include "SegmentedString.h"

namespace WebCore {

class HTMLToken;
class HTMLTokenizer;

class HTMLSourceTracker {
    WTF_MAKE_NONCOPYABLE(HTMLSourceTracker);
public:
    HTMLSourceTracker() = default;

    void startToken(SegmentedString&, HTMLTokenizer&);
    void endToken(SegmentedString&, HTMLTokenizer&);

    String source(const HTMLToken&);
    String source(const HTMLToken&, unsigned attributeStart, unsigned attributeEnd);

private:
    bool m_started { false };

    unsigned m_tokenStart;
    unsigned m_tokenEnd;

    SegmentedString m_previousSource;
    SegmentedString m_currentSource;

    String m_cachedSourceForToken;
};

} // namespace WebCore
