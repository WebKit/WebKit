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
#include "B3Const64Value.h"

#if ENABLE(B3_JIT)

#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

Const64Value::~Const64Value()
{
}

Value* Const64Value::negConstant(Procedure& proc) const
{
    return proc.add<Const64Value>(origin(), -m_value);
}

Value* Const64Value::addConstant(Procedure& proc, int32_t other) const
{
    return proc.add<Const64Value>(origin(), m_value + static_cast<int64_t>(other));
}

Value* Const64Value::addConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt64())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value + other->asInt64());
}

Value* Const64Value::subConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt64())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value - other->asInt64());
}

Value* Const64Value::bitAndConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt64())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value & other->asInt64());
}

Value* Const64Value::bitOrConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt64())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value | other->asInt64());
}

Value* Const64Value::bitXorConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt64())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value ^ other->asInt64());
}

Value* Const64Value::shlConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt32())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value << (other->asInt32() & 63));
}

Value* Const64Value::sShrConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt32())
        return nullptr;
    return proc.add<Const64Value>(origin(), m_value >> (other->asInt32() & 63));
}

Value* Const64Value::zShrConstant(Procedure& proc, Value* other) const
{
    if (!other->hasInt32())
        return nullptr;
    return proc.add<Const64Value>(origin(), static_cast<int64_t>(static_cast<uint64_t>(m_value) >> (other->asInt32() & 63)));
}

TriState Const64Value::equalConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value == other->asInt64());
}

TriState Const64Value::notEqualConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value != other->asInt64());
}

TriState Const64Value::lessThanConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value < other->asInt64());
}

TriState Const64Value::greaterThanConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value > other->asInt64());
}

TriState Const64Value::lessEqualConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value <= other->asInt64());
}

TriState Const64Value::greaterEqualConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(m_value >= other->asInt64());
}

TriState Const64Value::aboveConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(static_cast<uint64_t>(m_value) > static_cast<uint64_t>(other->asInt64()));
}

TriState Const64Value::belowConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(static_cast<uint64_t>(m_value) < static_cast<uint64_t>(other->asInt64()));
}

TriState Const64Value::aboveEqualConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(static_cast<uint64_t>(m_value) >= static_cast<uint64_t>(other->asInt64()));
}

TriState Const64Value::belowEqualConstant(Value* other) const
{
    if (!other->hasInt64())
        return MixedTriState;
    return triState(static_cast<uint64_t>(m_value) <= static_cast<uint64_t>(other->asInt64()));
}

void Const64Value::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma, m_value);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
