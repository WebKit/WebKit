/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "Counter.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class DeprecatedCSSOMCounter final : public RefCounted<DeprecatedCSSOMCounter> {
public:
    static Ref<DeprecatedCSSOMCounter> create(const Counter& counter) { return adoptRef(*new DeprecatedCSSOMCounter(counter)); }

    String identifier() const { return m_identifier->stringValue(); }
    String listStyle() const { return m_listStyle->stringValue(); }
    String separator() const { return m_separator->stringValue(); }
    
private:
    DeprecatedCSSOMCounter(const Counter& counter)
        : m_identifier(counter.identifierValue().createDeprecatedCSSOMPrimitiveWrapper())
        , m_listStyle(counter.listStyleValue().createDeprecatedCSSOMPrimitiveWrapper())
        , m_separator(counter.separatorValue().createDeprecatedCSSOMPrimitiveWrapper())
    {
    }
    
    Ref<DeprecatedCSSOMPrimitiveValue> m_identifier;
    Ref<DeprecatedCSSOMPrimitiveValue> m_listStyle;
    Ref<DeprecatedCSSOMPrimitiveValue> m_separator;
};

} // namespace WebCore
