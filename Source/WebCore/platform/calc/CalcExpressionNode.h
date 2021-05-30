/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

enum class CalcExpressionNodeType : uint8_t {
    Undefined,
    Number,
    Length,
    Operation,
    Negation,
    Inversion,
    BlendLength,
};

class CalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CalcExpressionNode(CalcExpressionNodeType = CalcExpressionNodeType::Undefined);
    virtual ~CalcExpressionNode() = default;

    CalcExpressionNodeType type() const { return m_type; }

    virtual float evaluate(float maxValue) const = 0;
    virtual bool operator==(const CalcExpressionNode&) const = 0;
    virtual void dump(TextStream&) const = 0;

private:
    CalcExpressionNodeType m_type;
};

inline CalcExpressionNode::CalcExpressionNode(CalcExpressionNodeType type)
    : m_type(type)
{
}

TextStream& operator<<(TextStream&, const CalcExpressionNode&);

}

#define SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CalcExpressionNode& node) { return node.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
