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
#include "DFGDesiredIdentifiers.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

DesiredIdentifiers::DesiredIdentifiers()
    : m_codeBlock(nullptr)
{
}

DesiredIdentifiers::DesiredIdentifiers(CodeBlock* codeBlock)
    : m_codeBlock(codeBlock)
{
}

DesiredIdentifiers::~DesiredIdentifiers()
{
}

unsigned DesiredIdentifiers::numberOfIdentifiers()
{
    return m_codeBlock->numberOfIdentifiers() + m_addedIdentifiers.size();
}

void DesiredIdentifiers::addLazily(StringImpl* rep)
{
    m_addedIdentifiers.append(rep);
}

StringImpl* DesiredIdentifiers::at(unsigned index) const
{
    StringImpl* result;
    if (index < m_codeBlock->numberOfIdentifiers())
        result = m_codeBlock->identifier(index).impl();
    else
        result = m_addedIdentifiers[index - m_codeBlock->numberOfIdentifiers()];
    ASSERT(result->hasAtLeastOneRef());
    return result;
}

void DesiredIdentifiers::reallyAdd(VM& vm, CommonData* commonData)
{
    for (unsigned i = 0; i < m_addedIdentifiers.size(); ++i) {
        StringImpl* rep = m_addedIdentifiers[i];
        ASSERT(rep->hasAtLeastOneRef());
        commonData->dfgIdentifiers.append(Identifier(&vm, rep));
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

