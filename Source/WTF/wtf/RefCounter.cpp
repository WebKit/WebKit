/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "RefCounter.h"

namespace WTF {

void RefCounter::Count::ref()
{
    bool valueWasZero = !m_value;
    ++m_value;
    
    if (valueWasZero && m_refCounter)
        m_refCounter->m_valueDidChange(true);
}

void RefCounter::Count::deref()
{
    ASSERT(m_value);
    --m_value;

    if (m_value)
        return;

    // The Count object is kept alive so long as either the RefCounter that created it remains
    // allocated, or so long as its reference count is non-zero.
    // If the RefCounter has already been deallocted then delete the Count when its reference
    // count reaches zero.
    if (m_refCounter)
        m_refCounter->m_valueDidChange(false);
    else
        delete this;
}

RefCounter::RefCounter(std::function<void(bool)> valueDidChange)
    : m_valueDidChange(valueDidChange)
    , m_count(new Count(*this))
{
}

RefCounter::~RefCounter()
{
    // The Count object is kept alive so long as either the RefCounter that created it remains
    // allocated, or so long as its reference count is non-zero.
    // If the reference count of the Count is already zero then delete it now, otherwise
    // clear its m_refCounter pointer.
    if (m_count->m_value)
        m_count->m_refCounter = nullptr;
    else
        delete m_count;
}

} // namespace WebCore
