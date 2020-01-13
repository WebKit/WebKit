/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "InstanceOfAccessCase.h"

#if ENABLE(JIT)

#include "JSCInlines.h"

namespace JSC {

std::unique_ptr<AccessCase> InstanceOfAccessCase::create(
    VM& vm, JSCell* owner, AccessType accessType, Structure* structure,
    const ObjectPropertyConditionSet& conditionSet, JSObject* prototype)
{
    return std::unique_ptr<AccessCase>(new InstanceOfAccessCase(vm, owner, accessType, structure, conditionSet, prototype));
}

void InstanceOfAccessCase::dumpImpl(PrintStream& out, CommaPrinter& comma) const
{
    Base::dumpImpl(out, comma);
    out.print(comma, "prototype = ", JSValue(prototype()));
}

std::unique_ptr<AccessCase> InstanceOfAccessCase::clone() const
{
    std::unique_ptr<InstanceOfAccessCase> result(new InstanceOfAccessCase(*this));
    result->resetState();
    return result;
}

InstanceOfAccessCase::~InstanceOfAccessCase()
{
}

InstanceOfAccessCase::InstanceOfAccessCase(
    VM& vm, JSCell* owner, AccessType accessType, Structure* structure,
    const ObjectPropertyConditionSet& conditionSet, JSObject* prototype)
    : Base(vm, owner, accessType, nullptr, invalidOffset, structure, conditionSet, nullptr)
    , m_prototype(vm, owner, prototype)
{
}

} // namespace JSC

#endif // ENABLE(JIT)

