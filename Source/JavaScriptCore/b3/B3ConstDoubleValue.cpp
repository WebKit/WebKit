/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "B3ConstDoubleValue.h"

#if ENABLE(B3_JIT)

#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

ConstDoubleValue::~ConstDoubleValue()
{
}

Value* ConstDoubleValue::negConstant(Procedure& proc) const
{
    return proc.add<ConstDoubleValue>(origin(), -m_value);
}

Value* ConstDoubleValue::addConstant(Procedure& proc, int32_t other) const
{
    return proc.add<ConstDoubleValue>(origin(), m_value + static_cast<double>(other));
}

Value* ConstDoubleValue::addConstant(Procedure& proc, Value* other) const
{
    if (!other->hasDouble())
        return nullptr;
    return proc.add<ConstDoubleValue>(origin(), m_value + other->asDouble());
}

Value* ConstDoubleValue::subConstant(Procedure& proc, Value* other) const
{
    if (!other->hasDouble())
        return nullptr;
    return proc.add<ConstDoubleValue>(origin(), m_value - other->asDouble());
}

TriState ConstDoubleValue::equalConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value == other->asDouble());
}

TriState ConstDoubleValue::notEqualConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value != other->asDouble());
}

TriState ConstDoubleValue::lessThanConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value < other->asDouble());
}

TriState ConstDoubleValue::greaterThanConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value > other->asDouble());
}

TriState ConstDoubleValue::lessEqualConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value <= other->asDouble());
}

TriState ConstDoubleValue::greaterEqualConstant(Value* other) const
{
    if (!other->hasDouble())
        return MixedTriState;
    return triState(m_value >= other->asDouble());
}

void ConstDoubleValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma);
    out.printf("%le", m_value);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
