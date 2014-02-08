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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "IntendedStructureChain.h"

#include "CodeBlock.h"
#include "Operations.h"
#include "StructureChain.h"

namespace JSC {

IntendedStructureChain::IntendedStructureChain(JSGlobalObject* globalObject, Structure* head)
    : m_globalObject(globalObject)
    , m_head(head)
{
    JSValue prototype = head->prototypeForLookup(globalObject);
    if (prototype.isNull())
        return;
    for (Structure* current = asObject(prototype)->structure(); current; current = current->storedPrototypeStructure())
        m_vector.append(current);
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, Structure* prototypeStructure)
    : m_globalObject(codeBlock->globalObject())
    , m_head(head)
{
    m_vector.append(prototypeStructure);
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, StructureChain* chain)
    : m_globalObject(codeBlock->globalObject())
    , m_head(head)
{
    for (unsigned i = 0; chain->head()[i]; ++i)
        m_vector.append(chain->head()[i].get());
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, StructureChain* chain, unsigned count)
    : m_globalObject(codeBlock->globalObject())
    , m_head(head)
{
    for (unsigned i = 0; i < count; ++i)
        m_vector.append(chain->head()[i].get());
}

IntendedStructureChain::~IntendedStructureChain()
{
}

bool IntendedStructureChain::isStillValid() const
{
    JSValue currentPrototype = m_head->prototypeForLookup(m_globalObject);
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        if (asObject(currentPrototype)->structure() != m_vector[i])
            return false;
        currentPrototype = m_vector[i]->storedPrototype();
    }
    return true;
}

bool IntendedStructureChain::matches(StructureChain* chain) const
{
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        if (m_vector[i] != chain->head()[i].get())
            return false;
    }
    if (chain->head()[m_vector.size()])
        return false;
    return true;
}

StructureChain* IntendedStructureChain::chain(VM& vm) const
{
    ASSERT(isStillValid());
    StructureChain* result = StructureChain::create(vm, m_head);
    ASSERT(matches(result));
    return result;
}

bool IntendedStructureChain::mayInterceptStoreTo(VM& vm, StringImpl* uid)
{
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        unsigned attributes;
        JSCell* specificValue;
        PropertyOffset offset = m_vector[i]->getConcurrently(vm, uid, attributes, specificValue);
        if (!isValidOffset(offset))
            continue;
        if (attributes & (ReadOnly | Accessor))
            return true;
        return false;
    }
    return false;
}

bool IntendedStructureChain::isNormalized()
{
    if (m_head->typeInfo().type() == ProxyType)
        return false;
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        Structure* structure = m_vector[i];
        if (structure->typeInfo().type() == ProxyType)
            return false;
        if (structure->isDictionary())
            return false;
    }
    return true;
}

JSObject* IntendedStructureChain::terminalPrototype() const
{
    ASSERT(!m_vector.isEmpty());
    if (m_vector.size() == 1)
        return asObject(m_head->prototypeForLookup(m_globalObject));
    return asObject(m_vector[m_vector.size() - 2]->storedPrototype());
}

void IntendedStructureChain::visitChildren(SlotVisitor& visitor)
{
    visitor.appendUnbarrieredPointer(&m_globalObject);
    visitor.appendUnbarrieredPointer(&m_head);
    for (unsigned i = m_vector.size(); i--;)
        visitor.appendUnbarrieredPointer(&m_vector[i]);
}

} // namespace JSC

