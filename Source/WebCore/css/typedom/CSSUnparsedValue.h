/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CSSStyleValue.h"
#include <variant>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<typename> class ExceptionOr;
class CSSOMVariableReferenceValue;
class CSSParserTokenRange;
using CSSUnparsedSegment = std::variant<String, RefPtr<CSSOMVariableReferenceValue>>;

class CSSUnparsedValue final : public CSSStyleValue {
    WTF_MAKE_ISO_ALLOCATED(CSSUnparsedValue);
public:
    static Ref<CSSUnparsedValue> create(Vector<CSSUnparsedSegment>&&);
    static Ref<CSSUnparsedValue> create(CSSParserTokenRange);

    void serialize(StringBuilder&, OptionSet<SerializationArguments>) const final;
    size_t length() const { return m_segments.size(); }
    
    ExceptionOr<CSSUnparsedSegment> item(size_t);
    ExceptionOr<CSSUnparsedSegment> setItem(size_t, CSSUnparsedSegment&&);

    CSSStyleValueType getType() const final { return CSSStyleValueType::CSSUnparsedValue; }

private:
    explicit CSSUnparsedValue(Vector<CSSUnparsedSegment>&& segments);
    
    Vector<CSSUnparsedSegment> m_segments;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSUnparsedValue)
    static bool isType(const WebCore::CSSStyleValue& styleValue) { return styleValue.getType() == WebCore::CSSStyleValueType::CSSUnparsedValue; }
SPECIALIZE_TYPE_TRAITS_END()
