/*
  * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)
#include "B3Const128Value.h"

#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace B3 {

Const128Value::~Const128Value() = default;

Value* Const128Value::vectorAndConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasV128())
        return nullptr;
    v128_t result = vectorAnd(m_value, other->asV128());
    return proc.add<Const128Value>(origin(), result);
}

Value* Const128Value::vectorOrConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasV128())
        return nullptr;
    v128_t result = vectorOr(m_value, other->asV128());
    return proc.add<Const128Value>(origin(), result);
}

Value* Const128Value::vectorXorConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasV128())
        return nullptr;
    v128_t result = vectorXor(m_value, other->asV128());
    return proc.add<Const128Value>(origin(), result);
}

void Const128Value::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma, m_value.u64x2[0], comma, m_value.u64x2[1]);
}

} } // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
