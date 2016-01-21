/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "B3SwitchValue.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include <wtf/ListDump.h>

namespace JSC { namespace B3 {

SwitchValue::~SwitchValue()
{
}

SwitchCase SwitchValue::removeCase(unsigned index)
{
    FrequentedBlock resultBlock = m_successors[index];
    int64_t resultValue = m_values[index];
    m_successors[index] = m_successors.last();
    m_successors.removeLast();
    m_values[index] = m_values.last();
    m_values.removeLast();
    return SwitchCase(resultValue, resultBlock);
}

void SwitchValue::appendCase(const SwitchCase& switchCase)
{
    m_successors.append(m_successors.last());
    m_successors[m_successors.size() - 2] = switchCase.target();
    m_values.append(switchCase.caseValue());
}

void SwitchValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    // This destructively overrides ControlValue's dumpMeta().
    for (SwitchCase switchCase : *this)
        out.print(comma, switchCase);
    out.print(comma, "fallThrough = ", fallThrough());
}

Value* SwitchValue::cloneImpl() const
{
    return new SwitchValue(*this);
}

SwitchValue::SwitchValue(
    unsigned index, Origin origin, Value* child, const FrequentedBlock& fallThrough)
    : ControlValue(index, Switch, Void, origin, child)
{
    m_successors.append(fallThrough);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
