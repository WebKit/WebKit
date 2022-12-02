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

#include "CSSOMVariableReferenceValue.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSTokenizer.h"
#include "CSSVariableReferenceValue.h"
#include "ExceptionOr.h"
#include <variant>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSUnparsedValue);

Ref<CSSUnparsedValue> CSSUnparsedValue::create(Vector<CSSUnparsedSegment>&& segments)
{
    return adoptRef(*new CSSUnparsedValue(WTFMove(segments)));
}

Ref<CSSUnparsedValue> CSSUnparsedValue::create(CSSParserTokenRange tokens)
{
    // This function assumes that tokens have the correct syntax. Otherwise asserts would be triggered.
    StringBuilder builder;
    Vector<Vector<CSSUnparsedSegment>> segmentStack;
    segmentStack.append({ });
    
    Vector<std::optional<StringView>> identifiers;
    
    while (!tokens.atEnd()) {
        auto currentToken = tokens.consume();
        
        if (currentToken.type() == FunctionToken || currentToken.type() == LeftParenthesisToken) {
            if (currentToken.functionId() == CSSValueVar) {
                if (!builder.isEmpty()) {
                    segmentStack.last().append(builder.toString());
                    builder.clear();
                }
                tokens.consumeWhitespace();
                auto identToken = tokens.consumeIncludingWhitespace();
                // Token after whitespace consumption must be variable reference identifier
                ASSERT(identToken.type() == IdentToken);
                if (tokens.peek().type() == CommaToken) {
                    // Fallback present
                    identifiers.append(StringView(identToken.value()));
                    segmentStack.append({ });
                    tokens.consume();
                } else if (tokens.peek().type() == RightParenthesisToken) {
                    // No fallback
                    auto variableReference = CSSOMVariableReferenceValue::create(identToken.value().toString());
                    ASSERT(!variableReference.hasException());
                    segmentStack.last().append(CSSUnparsedSegment { RefPtr<CSSOMVariableReferenceValue> { variableReference.releaseReturnValue() } });
                    tokens.consume();
                } else
                    ASSERT_NOT_REACHED();
                
            } else {
                currentToken.serialize(builder);
                identifiers.append(std::nullopt);
            }
        } else if (currentToken.type() == RightParenthesisToken) {
            ASSERT(segmentStack.size());
            if (!builder.isEmpty())
                segmentStack.last().append(builder.toString());
            builder.clear();
            ASSERT(!identifiers.isEmpty());
            
            if (auto topIdentifier = identifiers.takeLast()) {
                auto variableReference = CSSOMVariableReferenceValue::create(topIdentifier->toString(), CSSUnparsedValue::create(segmentStack.takeLast()));
                ASSERT(!variableReference.hasException());
                segmentStack.last().append(variableReference.releaseReturnValue());
            } else
                currentToken.serialize(builder);
        } else
            currentToken.serialize(builder);
    }
    ASSERT(segmentStack.size() == 1);
    if (!builder.isEmpty())
        segmentStack.last().append(builder.toString());
    
    return CSSUnparsedValue::create(WTFMove(segmentStack.last()));
}

CSSUnparsedValue::CSSUnparsedValue(Vector<CSSUnparsedSegment>&& segments)
    : m_segments(WTFMove(segments))
{
}

void CSSUnparsedValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    for (auto& segment : m_segments) {
        std::visit(WTF::makeVisitor([&] (const String& value) {
            builder.append(value);
        }, [&] (const RefPtr<CSSOMVariableReferenceValue>& value) {
            value->serialize(builder, arguments);
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
    if (index > m_segments.size())
        return Exception { RangeError, makeString("Index ", index, " exceeds index range for unparsed segments.") };
    if (index == m_segments.size())
        m_segments.append(WTFMove(val));
    else
        m_segments[index] = WTFMove(val);
    return CSSUnparsedSegment { m_segments[index] };
}

RefPtr<CSSValue> CSSUnparsedValue::toCSSValue() const
{
    CSSTokenizer tokenizer(toString());
    return CSSVariableReferenceValue::create(tokenizer.tokenRange(), strictCSSParserContext());
}

} // namespace WebCore
