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

#include "config.h"
#include "ConstantStructureCheck.h"

#include "JSCInlines.h"

namespace JSC {

void ConstantStructureCheck::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print(
        "(Check if ", inContext(JSValue(m_constant), context), " has structure ",
        pointerDumpInContext(m_structure, context), ")");
}

void ConstantStructureCheck::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

Structure* structureFor(const ConstantStructureCheckVector& vector, JSCell* constant)
{
    for (unsigned i = vector.size(); i--;) {
        if (vector[i].constant() == constant)
            return vector[i].structure();
    }
    return nullptr;
}

bool areCompatible(const ConstantStructureCheckVector& a, const ConstantStructureCheckVector& b)
{
    for (unsigned i = a.size(); i--;) {
        Structure* otherStructure = structureFor(b, a[i].constant());
        if (!otherStructure)
            continue;
        if (a[i].structure() != otherStructure)
            return false;
    }
    return true;
}

void mergeInto(const ConstantStructureCheckVector& source, ConstantStructureCheckVector& target)
{
    for (unsigned i = source.size(); i--;) {
        if (structureFor(target, source[i].constant()))
            continue;
        target.append(source[i]);
    }
}

} // namespace JSC

