/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DFGDesiredWeakReferences.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGCommonData.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

DesiredWeakReferences::DesiredWeakReferences()
    : m_codeBlock(nullptr)
{
}

DesiredWeakReferences::DesiredWeakReferences(CodeBlock* codeBlock)
    : m_codeBlock(codeBlock)
{
}

DesiredWeakReferences::~DesiredWeakReferences()
{
}

void DesiredWeakReferences::addLazily(JSCell* cell)
{
    m_references.append(cell);
}

void DesiredWeakReferences::reallyAdd(VM& vm, CommonData* common)
{
    for (unsigned i = 0; i < m_references.size(); i++) {
        JSCell* target = m_references[i];
        common->weakReferences.append(WriteBarrier<JSCell>(vm, m_codeBlock->ownerExecutable(), target));
    }
}

void DesiredWeakReferences::visitChildren(SlotVisitor& visitor)
{
    for (unsigned i = m_references.size(); i--;)
        visitor.appendUnbarrieredPointer(&m_references[i]);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
