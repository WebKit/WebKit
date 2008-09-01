// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "StructureID.h"

#include "identifier.h"
#include "JSCell.h"
#include <wtf/RefPtr.h>

namespace KJS {

StructureID::StructureID(JSValue* prototype)
    : m_isDictionary(false)
    , m_prototype(prototype)
    , m_cachedPrototypeChain(0)
    , m_previous(0)
    , m_nameInPrevious(0)
    , m_transitionCount(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype->isObject() || m_prototype->isNull());
}

PassRefPtr<StructureID> StructureID::addPropertyTransition(StructureID* structureID, const Identifier& name)
{
    ASSERT(!structureID->m_isDictionary);

    if (StructureID* existingTransition = structureID->m_transitionTable.get(name.ustring().rep()))
        return existingTransition;

    if (structureID->m_transitionCount > s_maxTransitionLength)
        return dictionaryTransition(structureID);

    RefPtr<StructureID> transition = create(structureID->m_prototype);
    transition->m_cachedPrototypeChain = structureID->m_cachedPrototypeChain;
    transition->m_previous = structureID;
    transition->m_nameInPrevious = name.ustring().rep();
    transition->m_transitionCount = structureID->m_transitionCount + 1;

    structureID->m_transitionTable.add(name.ustring().rep(), transition.get());
    return transition.release();
}

PassRefPtr<StructureID> StructureID::dictionaryTransition(StructureID* structureID)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype);
    transition->m_isDictionary = true;
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::changePrototypeTransition(StructureID* structureID, JSValue* prototype)
{
    RefPtr<StructureID> transition = create(prototype);
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::getterSetterTransition(StructureID* structureID)
{
    RefPtr<StructureID> transition = create(structureID->prototype());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    return transition.release();
}

StructureID::~StructureID()
{
    if (m_previous) {
        ASSERT(m_previous->m_transitionTable.contains(m_nameInPrevious));
        m_previous->m_transitionTable.remove(m_nameInPrevious);
    }
}

StructureIDChain::StructureIDChain(StructureID* structureID)
    : m_size(0)
{
    StructureID* tmp = structureID;
    while (tmp->prototype() != jsNull()) {
        ++m_size;
        tmp = static_cast<JSCell*>(tmp->prototype())->structureID();
    }

    m_vector.set(new RefPtr<StructureID>[m_size]);

    for (size_t i = 0; i < m_size; ++i) {
        m_vector[i] = structureID;
        structureID = static_cast<JSCell*>(structureID->prototype())->structureID();
    }
}

} // namespace KJS
