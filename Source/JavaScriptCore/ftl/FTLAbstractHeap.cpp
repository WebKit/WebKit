/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "FTLAbbreviations.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLOutput.h"
#include "FTLTypedPointer.h"
#include "JSCInlines.h"
#include "Options.h"

namespace JSC { namespace FTL {

LValue AbstractHeap::tbaaMetadataSlow(const AbstractHeapRepository& repository) const
{
    m_tbaaMetadata = mdNode(
        repository.m_context,
        mdString(repository.m_context, m_heapName),
        m_parent->tbaaMetadata(repository));
    return m_tbaaMetadata;
}

void AbstractHeap::decorateInstruction(LValue instruction, const AbstractHeapRepository& repository) const
{
    if (!Options::useFTLTBAA())
        return;
    setMetadata(instruction, repository.m_tbaaKind, tbaaMetadata(repository));
}

IndexedAbstractHeap::IndexedAbstractHeap(LContext context, AbstractHeap* parent, const char* heapName, ptrdiff_t offset, size_t elementSize)
    : m_heapForAnyIndex(parent, heapName)
    , m_heapNameLength(strlen(heapName))
    , m_offset(offset)
    , m_elementSize(elementSize)
    , m_scaleTerm(0)
    , m_canShift(false)
{
    // See if there is a common shift amount we could use instead of multiplying. Don't
    // try too hard. This is just a speculative optimization to reduce load on LLVM.
    for (unsigned i = 0; i < 4; ++i) {
        if ((1 << i) == m_elementSize) {
            if (i)
                m_scaleTerm = constInt(intPtrType(context), i, ZeroExtend);
            m_canShift = true;
            break;
        }
    }
    
    if (!m_canShift)
        m_scaleTerm = constInt(intPtrType(context), m_elementSize, ZeroExtend);
}

IndexedAbstractHeap::~IndexedAbstractHeap()
{
}

TypedPointer IndexedAbstractHeap::baseIndex(Output& out, LValue base, LValue index, JSValue indexAsConstant, ptrdiff_t offset)
{
    if (indexAsConstant.isInt32())
        return out.address(base, at(indexAsConstant.asInt32()), offset);
    
    LValue result;
    if (m_canShift) {
        if (!m_scaleTerm)
            result = out.add(base, index);
        else
            result = out.add(base, out.shl(index, m_scaleTerm));
    } else
        result = out.add(base, out.mul(index, m_scaleTerm));
    
    return TypedPointer(atAnyIndex(), out.addPtr(result, m_offset + offset));
}

const AbstractField& IndexedAbstractHeap::atSlow(ptrdiff_t index)
{
    ASSERT(static_cast<size_t>(index) >= m_smallIndices.size());
    
    if (UNLIKELY(!m_largeIndices))
        m_largeIndices = adoptPtr(new MapType());

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

NumberedAbstractHeap::NumberedAbstractHeap(LContext context, AbstractHeap* heap, const char* heapName)
    : m_indexedHeap(context, heap, heapName, 0, 1)
{
}

NumberedAbstractHeap::~NumberedAbstractHeap()
{
}

AbsoluteAbstractHeap::AbsoluteAbstractHeap(LContext context, AbstractHeap* heap, const char* heapName)
    : m_indexedHeap(context, heap, heapName, 0, 1)
{
}

AbsoluteAbstractHeap::~AbsoluteAbstractHeap()
{
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

