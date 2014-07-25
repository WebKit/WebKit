/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef ConstantStructureCheck_h
#define ConstantStructureCheck_h

#include "DumpContext.h"
#include "JSCell.h"
#include "Structure.h"
#include <wtf/PrintStream.h>
#include <wtf/Vector.h>

namespace JSC {

class ConstantStructureCheck {
public:
    ConstantStructureCheck()
        : m_constant(nullptr)
        , m_structure(nullptr)
    {
    }
    
    ConstantStructureCheck(JSCell* constant, Structure* structure)
        : m_constant(constant)
        , m_structure(structure)
    {
        ASSERT(!!m_constant == !!m_structure);
    }
    
    bool operator!() const { return !m_constant; }
    
    JSCell* constant() const { return m_constant; }
    Structure* structure() const { return m_structure; }
    
    void dumpInContext(PrintStream&, DumpContext*) const;
    void dump(PrintStream&) const;
    
private:
    JSCell* m_constant;
    Structure* m_structure;
};

typedef Vector<ConstantStructureCheck, 2> ConstantStructureCheckVector;

Structure* structureFor(const ConstantStructureCheckVector& vector, JSCell* constant);
bool areCompatible(const ConstantStructureCheckVector&, const ConstantStructureCheckVector&);
void mergeInto(const ConstantStructureCheckVector& source, ConstantStructureCheckVector& target);

} // namespace JSC

#endif // ConstantStructureCheck_h

