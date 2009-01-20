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
#include "StructureChain.h"

#include "JSObject.h"
#include "Structure.h"
#include <wtf/RefPtr.h>

namespace JSC {

StructureChain::StructureChain(Structure* structure)
{
    size_t size = 1;

    Structure* tmp = structure;
    while (!tmp->storedPrototype().isNull()) {
        ++size;
        tmp = asCell(tmp->storedPrototype())->structure();
    }
    
    m_vector.set(new RefPtr<Structure>[size + 1]);

    size_t i;
    for (i = 0; i < size - 1; ++i) {
        m_vector[i] = structure;
        structure = asObject(structure->storedPrototype())->structure();
    }
    m_vector[i] = structure;
    m_vector[i + 1] = 0;
}

bool structureChainsAreEqual(StructureChain* chainA, StructureChain* chainB)
{
    if (!chainA || !chainB)
        return false;

    RefPtr<Structure>* a = chainA->head();
    RefPtr<Structure>* b = chainB->head();
    while (1) {
        if (*a != *b)
            return false;
        if (!*a)
            return true;
        a++;
        b++;
    }
}

} // namespace JSC
