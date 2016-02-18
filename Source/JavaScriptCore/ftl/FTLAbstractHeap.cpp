/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
#include "FTLAbstractHeap.h"

#if ENABLE(FTL_JIT)

#include "DFGCommon.h"
#include "FTLAbbreviatedTypes.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLOutput.h"
#include "FTLTypedPointer.h"
#include "JSCInlines.h"
#include "Options.h"

namespace JSC { namespace FTL {

void AbstractHeap::decorateInstruction(LValue instruction, const AbstractHeapRepository& repository) const
{
    UNUSED_PARAM(instruction);
    UNUSED_PARAM(repository);
}

void AbstractHeap::dump(PrintStream& out) const
{
    out.print(heapName());
    if (m_parent)
        out.print("->", *m_parent);
}

void AbstractField::dump(PrintStream& out) const
{
    out.print(heapName(), "(", m_offset, ")");
    if (parent())
        out.print("->", *parent());
}

IndexedAbstractHeap::IndexedAbstractHeap(AbstractHeap* parent, const char* heapName, ptrdiff_t offset, size_t elementSize)
    : m_heapForAnyIndex(parent, heapName)
    , m_heapNameLength(strlen(heapName))
    , m_offset(offset)
    , m_elementSize(elementSize)
{
}

IndexedAbstractHeap::~IndexedAbstractHeap()
{
}

TypedPointer IndexedAbstractHeap::baseIndex(Output& out, LValue base, LValue index, JSValue indexAsConstant, ptrdiff_t offset)
{
    if (indexAsConstant.isInt32())
        return out.address(base, at(indexAsConstant.asInt32()), offset);

    LValue result = out.add(base, out.mul(index, out.constIntPtr(m_elementSize)));
    
    return TypedPointer(atAnyIndex(), out.addPtr(result, m_offset + offset));
}

const AbstractField& IndexedAbstractHeap::atSlow(ptrdiff_t index)
{
    ASSERT(static_cast<size_t>(index) >= m_smallIndices.size());
    
    if (UNLIKELY(!m_largeIndices))
        m_largeIndices = std::make_unique<MapType>();

    std::unique_ptr<AbstractField>& field = m_largeIndices->add(index, nullptr).iterator->value;
    if (!field) {
        field = std::make_unique<AbstractField>();
        initialize(*field, index);
    }

    return *field;
}

void IndexedAbstractHeap::initialize(AbstractField& field, ptrdiff_t signedIndex)
{
    // Build up a name of the form:
    //
    //    heapName_hexIndex
    //
    // or:
    //
    //    heapName_neg_hexIndex
    //
    // For example if you access an indexed heap called FooBar at index 5, you'll
    // get:
    //
    //    FooBar_5
    //
    // Or if you access an indexed heap called Blah at index -10, you'll get:
    //
    //    Blah_neg_A
    //
    // This is important because LLVM uses the string to distinguish the types.
    
    static const char* negSplit = "_neg_";
    static const char* posSplit = "_";
    
    bool negative;
    size_t index;
    if (signedIndex < 0) {
        negative = true;
        index = -signedIndex;
    } else {
        negative = false;
        index = signedIndex;
    }
    
    for (unsigned power = 4; power <= sizeof(void*) * 8; power += 4) {
        if (isGreaterThanNonZeroPowerOfTwo(index, power))
            continue;
        
        unsigned numHexlets = power >> 2;
        
        size_t stringLength = m_heapNameLength + (negative ? strlen(negSplit) : strlen(posSplit)) + numHexlets;
        char* characters;
        m_largeIndexNames.append(CString::newUninitialized(stringLength, characters));
        
        memcpy(characters, m_heapForAnyIndex.heapName(), m_heapNameLength);
        if (negative)
            memcpy(characters + m_heapNameLength, negSplit, strlen(negSplit));
        else
            memcpy(characters + m_heapNameLength, posSplit, strlen(posSplit));
        
        size_t accumulator = index;
        for (unsigned i = 0; i < numHexlets; ++i) {
            characters[stringLength - i - 1] = lowerNibbleToASCIIHexDigit(accumulator);
            accumulator >>= 4;
        }
        
        field.initialize(&m_heapForAnyIndex, characters, m_offset + signedIndex * m_elementSize);
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void IndexedAbstractHeap::dump(PrintStream& out) const
{
    out.print("Indexed:", atAnyIndex());
}

NumberedAbstractHeap::NumberedAbstractHeap(AbstractHeap* heap, const char* heapName)
    : m_indexedHeap(heap, heapName, 0, 1)
{
}

NumberedAbstractHeap::~NumberedAbstractHeap()
{
}

void NumberedAbstractHeap::dump(PrintStream& out) const
{
    out.print("Numbered: ", atAnyNumber());
}

AbsoluteAbstractHeap::AbsoluteAbstractHeap(AbstractHeap* heap, const char* heapName)
    : m_indexedHeap(heap, heapName, 0, 1)
{
}

AbsoluteAbstractHeap::~AbsoluteAbstractHeap()
{
}

void AbsoluteAbstractHeap::dump(PrintStream& out) const
{
    out.print("Absolute:", atAnyAddress());
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

