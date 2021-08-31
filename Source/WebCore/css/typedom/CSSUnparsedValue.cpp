/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSUnparsedValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSOMVariableReferenceValue.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Variant.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSUnparsedValue);

Ref<CSSUnparsedValue> CSSUnparsedValue::create(Vector<CSSUnparsedSegment>&& segments)
{
    return adoptRef(*new CSSUnparsedValue(WTFMove(segments)));
}

CSSUnparsedValue::CSSUnparsedValue(Vector<CSSUnparsedSegment>&& segments)
    : m_segments(WTFMove(segments))
{
}

String CSSUnparsedValue::toString() const
{
    StringBuilder builder;
    serialize(builder);
    
    return builder.toString();
}

void CSSUnparsedValue::serialize(StringBuilder& builder) const
{
    for (auto& segment : m_segments) {
        WTF::visit(WTF::makeVisitor([&] (const String& value) {
            builder.append(value);
        }, [&] (const RefPtr<CSSOMVariableReferenceValue>& value) {
            builder.append(value->toString());
        }), segment);
    }
}

ExceptionOr<CSSUnparsedSegment> CSSUnparsedValue::item(size_t index)
{
    if (index >= m_segments.size())
        return Exception { RangeError, makeString("Index ", index, " exceeds index range for unparsed segments.") };
    return CSSUnparsedSegment { m_segments[index] };
}

ExceptionOr<CSSUnparsedSegment> CSSUnparsedValue::setItem(size_t index, CSSUnparsedSegment&& val)
{
    if (index >= m_segments.size())
        return Exception { RangeError, makeString("Index ", index, " exceeds index range for unparsed segments.") };
    m_segments[index] = WTFMove(val);
    return CSSUnparsedSegment { m_segments[index] };
}

} // namespace WebCore

#endif
