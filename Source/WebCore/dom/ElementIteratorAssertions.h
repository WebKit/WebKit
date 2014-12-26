/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef ElementIteratorAssertions_h
#define ElementIteratorAssertions_h

#include "Document.h"
#include "Element.h"

namespace WebCore {

class ElementIteratorAssertions {
public:
    ElementIteratorAssertions();
    ElementIteratorAssertions(const Element* first);
    bool domTreeHasMutated() const;
    void dropEventDispatchAssertion();

private:
    const Document* m_document;
    uint64_t m_initialDOMTreeVersion;
    OwnPtr<NoEventDispatchAssertion> m_noEventDispatchAssertion;
};

inline ElementIteratorAssertions::ElementIteratorAssertions()
    : m_document(nullptr)
    , m_initialDOMTreeVersion(0)
{
}

inline ElementIteratorAssertions::ElementIteratorAssertions(const Element* first)
    : m_document(first ? &first->document() : nullptr)
    , m_initialDOMTreeVersion(m_document ? m_document->domTreeVersion() : 0)
    , m_noEventDispatchAssertion(m_document ? adoptPtr(new NoEventDispatchAssertion) : nullptr)
{
}

inline bool ElementIteratorAssertions::domTreeHasMutated() const
{
    return m_initialDOMTreeVersion && m_document && m_document->domTreeVersion() != m_initialDOMTreeVersion;
}

inline void ElementIteratorAssertions::dropEventDispatchAssertion()
{
    m_noEventDispatchAssertion = nullptr;
}

}

#endif
