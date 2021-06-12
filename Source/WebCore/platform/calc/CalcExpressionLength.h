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

#include "CalcExpressionNode.h"
#include "Length.h"

namespace WebCore {

class CalcExpressionLength final : public CalcExpressionNode {
public:
    explicit CalcExpressionLength(Length);

    const Length& length() const { return m_length; }

private:
    float evaluate(float maxValue) const final;
    bool operator==(const CalcExpressionNode&) const final;
    void dump(TextStream&) const final;

    Length m_length;
};

inline CalcExpressionLength::CalcExpressionLength(Length length)
    : CalcExpressionNode(CalcExpressionNodeType::Length)
    , m_length(length)
{
}

inline bool operator==(const CalcExpressionLength& a, const CalcExpressionLength& b)
{
    return a.length() == b.length();
}

}

SPECIALIZE_TYPE_TRAITS_CALCEXPRESSION_NODE(CalcExpressionLength, type() == WebCore::CalcExpressionNodeType::Length)
