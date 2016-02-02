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
#include "B3ControlValue.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"

namespace JSC { namespace B3 {

ControlValue::~ControlValue()
{
}

bool ControlValue::replaceSuccessor(BasicBlock* from, BasicBlock* to)
{
    bool result = false;
    for (FrequentedBlock& successor : m_successors) {
        if (successor.block() == from) {
            successor.block() = to;
            result = true;

            // Keep looping because it's valid for a successor to be mentioned multiple times,
            // like if multiple switch cases have the same target.
        }
    }
    return result;
}

void ControlValue::convertToJump(BasicBlock* destination)
{
    unsigned index = this->index();
    Origin origin = this->origin();
    BasicBlock* owner = this->owner;

    this->ControlValue::~ControlValue();

    new (this) ControlValue(Jump, origin, FrequentedBlock(destination));

    this->owner = owner;
    this->m_index = index;
}

void ControlValue::convertToOops()
{
    unsigned index = this->index();
    Origin origin = this->origin();
    BasicBlock* owner = this->owner;

    this->ControlValue::~ControlValue();

    new (this) ControlValue(Oops, origin);

    this->owner = owner;
    this->m_index = index;
}

void ControlValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    for (FrequentedBlock successor : m_successors)
        out.print(comma, successor);
}

Value* ControlValue::cloneImpl() const
{
    return new ControlValue(*this);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
