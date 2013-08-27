/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DOMSettableTokenList.h"

namespace WebCore {

PassRefPtr<DOMSettableTokenList> DOMSettableTokenList::create()
{
    return adoptRef(new DOMSettableTokenList());
}

void DOMSettableTokenList::ref()
{
    RefCounted<DOMSettableTokenList>::ref();
}

void DOMSettableTokenList::deref()
{
    RefCounted<DOMSettableTokenList>::deref();
}

unsigned DOMSettableTokenList::length() const
{
    return m_tokens.size();
}

const AtomicString DOMSettableTokenList::item(unsigned index) const
{
    if (index >= length())
        return AtomicString();
    return m_tokens[index];
}

bool DOMSettableTokenList::containsInternal(const AtomicString& token) const
{
    return m_tokens.contains(token);
}

AtomicString DOMSettableTokenList::value() const
{
    return m_value;
}

void DOMSettableTokenList::setValue(const AtomicString& value)
{
    m_value = value;
    m_tokens.set(value, false);
}

} // namespace WebCore
