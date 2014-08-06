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
#include "IntendedStructureChain.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include "StructureChain.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

IntendedStructureChain::IntendedStructureChain(JSGlobalObject* globalObject, JSValue prototype)
    : m_globalObject(globalObject)
    , m_prototype(prototype)
{
    ASSERT(m_prototype.isNull() || m_prototype.isObject());
    if (prototype.isNull())
        return;
    for (Structure* current = asObject(prototype)->structure(); current; current = current->storedPrototypeStructure())
        m_vector.append(current);
}

IntendedStructureChain::IntendedStructureChain(JSGlobalObject* globalObject, Structure* head)
    : m_globalObject(globalObject)
    , m_prototype(head->prototypeForLookup(m_globalObject))
{
    if (m_prototype.isNull())
        return;
    for (Structure* current = asObject(m_prototype)->structure(); current; current = current->storedPrototypeStructure())
        m_vector.append(current);
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, Structure* prototypeStructure)
    : m_globalObject(codeBlock->globalObject())
    , m_prototype(head->prototypeForLookup(m_globalObject))
{
    m_vector.append(prototypeStructure);
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, StructureChain* chain)
    : m_globalObject(codeBlock->globalObject())
    , m_prototype(head->prototypeForLookup(m_globalObject))
{
    for (unsigned i = 0; chain->head()[i]; ++i)
        m_vector.append(chain->head()[i].get());
}

IntendedStructureChain::IntendedStructureChain(CodeBlock* codeBlock, Structure* head, StructureChain* chain, unsigned count)
    : m_globalObject(codeBlock->globalObject())
    , m_prototype(head->prototypeForLookup(m_globalObject))
{
    for (unsigned i = 0; i < count; ++i)
        m_vector.append(chain->head()[i].get());
}

IntendedStructureChain::~IntendedStructureChain()
{
}

bool IntendedStructureChain::isStillValid() const
{
    JSValue currentPrototype = m_prototype;
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

bool IntendedStructureChain::mayInterceptStoreTo(VM& vm, StringImpl* uid)
{
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        unsigned attributes;
        PropertyOffset offset = m_vector[i]->getConcurrently(vm, uid, attributes);
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
    for (unsigned i = 0; i < m_vector.size(); ++i) {
        Structure* structure = m_vector[i];
        if (structure->isProxy())
            return false;
        if (structure->isDictionary())
            return false;
    }
    return true;
}

bool IntendedStructureChain::takesSlowPathInDFGForImpureProperty()
{
    for (size_t i = 0; i < size(); ++i) {
        if (at(i)->takesSlowPathInDFGForImpureProperty())
            return true;
    }
    return false;
}

JSObject* IntendedStructureChain::terminalPrototype() const
{
    ASSERT(!m_vector.isEmpty());
    if (m_vector.size() == 1)
        return asObject(m_prototype);
    return asObject(m_vector[m_vector.size() - 2]->storedPrototype());
}

bool IntendedStructureChain::operator==(const IntendedStructureChain& other) const
{
    return m_globalObject == other.m_globalObject
        && m_prototype == other.m_prototype
        && m_vector == other.m_vector;
}

void IntendedStructureChain::gatherChecks(ConstantStructureCheckVector& vector) const
{
    JSValue currentPrototype = m_prototype;
    for (unsigned i = 0; i < size(); ++i) {
        JSObject* currentObject = asObject(currentPrototype);
        Structure* currentStructure = at(i);
        vector.append(ConstantStructureCheck(currentObject, currentStructure));
        currentPrototype = currentStructure->prototypeForLookup(m_globalObject);
    }
}

void IntendedStructureChain::visitChildren(SlotVisitor& visitor)
{
    visitor.appendUnbarrieredPointer(&m_globalObject);
    visitor.appendUnbarrieredValue(&m_prototype);
    for (unsigned i = m_vector.size(); i--;)
        visitor.appendUnbarrieredPointer(&m_vector[i]);
}

void IntendedStructureChain::dump(PrintStream& out) const
{
    dumpInContext(out, 0);
}

void IntendedStructureChain::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print(
        "(global = ", RawPointer(m_globalObject), ", head = ",
        inContext(m_prototype, context), ", vector = [");
    CommaPrinter comma;
    for (unsigned i = 0; i < m_vector.size(); ++i)
        out.print(comma, pointerDumpInContext(m_vector[i], context));
    out.print("])");
}

} // namespace JSC

