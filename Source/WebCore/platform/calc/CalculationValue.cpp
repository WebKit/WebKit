/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CalculationValue.h"

#include "CalcExpressionNode.h"
#include <limits>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<CalculationValue> CalculationValue::create(std::unique_ptr<CalcExpressionNode> value, ValueRange range)
{
    return adoptRef(*new CalculationValue(WTFMove(value), range));
}

CalculationValue::CalculationValue(std::unique_ptr<CalcExpressionNode> expression, ValueRange range)
    : m_expression(WTFMove(expression))
    , m_shouldClampToNonNegative(range == ValueRange::NonNegative)
{
}

CalculationValue::~CalculationValue() = default;

float CalculationValue::evaluate(float maxValue) const
{
    float result = m_expression->evaluate(maxValue);
    // FIXME: This test was originally needed when we did not detect division by zero at parse time.
    // It's possible that this is now unneeded code and can be removed.
    if (std::isnan(result))
        return 0;
    return m_shouldClampToNonNegative && result < 0 ? 0 : result;
}

bool operator==(const CalculationValue& a, const CalculationValue& b)
{
    return a.expression() == b.expression();
}

TextStream& operator<<(TextStream& ts, const CalculationValue& value)
{
    return ts << "calc(" << value.expression() << ")";
}

} // namespace WebCore
