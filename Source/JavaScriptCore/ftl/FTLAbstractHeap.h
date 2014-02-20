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

#ifndef FTLAbstractHeap_h
#define FTLAbstractHeap_h

#if ENABLE(FTL_JIT)

#include "FTLAbbreviations.h"
#include "JSCJSValue.h"
#include <array>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace JSC { namespace FTL {

// The FTL JIT tries to aid LLVM's TBAA. The FTL's notion of how this
// happens is the AbstractHeap. AbstractHeaps are a simple type system
// with sub-typing.

class AbstractHeapRepository;
class Output;
class TypedPointer;

class AbstractHeap {
    WTF_MAKE_NONCOPYABLE(AbstractHeap); WTF_MAKE_FAST_ALLOCATED;
public:
    AbstractHeap()
        : m_parent(0)
        , m_heapName(0)
        , m_tbaaMetadata(0)
    {
    }
    
    AbstractHeap(AbstractHeap* parent, const char* heapName)
        : m_parent(parent)
        , m_heapName(heapName)
        , m_tbaaMetadata(0)
    {
    }
    
    bool isInitialized() const { return !!m_heapName; }
    
    void initialize(AbstractHeap* parent, const char* heapName)
    {
        m_parent = parent;
        m_heapName = heapName;
    }

    AbstractHeap* parent() const
    {
        ASSERT(isInitialized());
        return m_parent;
    }
    
    const char* heapName() const
    {
        ASSERT(isInitialized());
        return m_heapName;
    }
    
    LValue tbaaMetadata(const AbstractHeapRepository& repository) const
    {
        ASSERT(isInitialized());
        if (LIKELY(!!m_tbaaMetadata))
            return m_tbaaMetadata;
        return tbaaMetadataSlow(repository);
    }
    
    void decorateInstruction(LValue instruction, const AbstractHeapRepository&) const;

private:
    friend class AbstractHeapRepository;
    
    LValue tbaaMetadataSlow(const AbstractHeapRepository&) const;
    
    AbstractHeap* m_parent;
    const char* m_heapName;
    mutable LValue m_tbaaMetadata;
};

// Think of "AbstractField" as being an "AbstractHeapWithOffset". I would have named
// it the latter except that I don't like typing that much.
class AbstractField : public AbstractHeap {
public:
    AbstractField()
    {
    }
    
    AbstractField(AbstractHeap* parent, const char* heapName, ptrdiff_t offset)
        : AbstractHeap(parent, heapName)
        , m_offset(offset)
    {
    }
    
    void initialize(AbstractHeap* parent, const char* heapName, ptrdiff_t offset)
    {
        AbstractHeap::initialize(parent, heapName);
        m_offset = offset;
    }
    
    ptrdiff_t offset() const
    {
        ASSERT(isInitialized());
        return m_offset;
    }
    
private:
    ptrdiff_t m_offset;
};

class IndexedAbstractHeap {
public:
    IndexedAbstractHeap(LContext, AbstractHeap* parent, const char* heapName, ptrdiff_t offset, size_t elementSize);
    ~IndexedAbstractHeap();
    
    const AbstractHeap& atAnyIndex() const { return m_heapForAnyIndex; }
    
    const AbstractField& at(ptrdiff_t index)
    {
        if (static_cast<size_t>(index) < m_smallIndices.size())
            return returnInitialized(m_smallIndices[index], index);
        return atSlow(index);
    }
    
    const AbstractField& operator[](ptrdiff_t index) { return at(index); }
    
    TypedPointer baseIndex(Output& out, LValue base, LValue index, JSValue indexAsConstant = JSValue(), ptrdiff_t offset = 0);
    
private:
    const AbstractField& returnInitialized(AbstractField& field, ptrdiff_t index)
    {
        if (UNLIKELY(!field.isInitialized()))
            initialize(field, index);
        return field;
    }

    const AbstractField& atSlow(ptrdiff_t index);
    void initialize(AbstractField& field, ptrdiff_t index);

    AbstractHeap m_heapForAnyIndex;
    size_t m_heapNameLength;
    ptrdiff_t m_offset;
    size_t m_elementSize;
    LValue m_scaleTerm;
    bool m_canShift;
    std::array<AbstractField, 16> m_smallIndices;
    
    struct WithoutZeroOrOneHashTraits : WTF::GenericHashTraits<ptrdiff_t> {
        static void constructDeletedValue(ptrdiff_t& slot) { slot = 1; }
        static bool isDeletedValue(ptrdiff_t value) { return value == 1; }
    };
    typedef HashMap<ptrdiff_t, std::unique_ptr<AbstractField>, WTF::IntHash<ptrdiff_t>, WithoutZeroOrOneHashTraits> MapType;
    
    OwnPtr<MapType> m_largeIndices;
    Vector<CString, 16> m_largeIndexNames;
};

// A numbered abstract heap is like an indexed abstract heap, except that you
// can't rely on there being a relationship between the number you use to
// retrieve the sub-heap, and the offset that this heap has. (In particular,
// the sub-heaps don't have indices.)

class NumberedAbstractHeap {
public:
    NumberedAbstractHeap(LContext, AbstractHeap* parent, const char* heapName);
    ~NumberedAbstractHeap();
    
    const AbstractHeap& atAnyNumber() const { return m_indexedHeap.atAnyIndex(); }
    
    const AbstractHeap& at(unsigned number) { return m_indexedHeap.at(number); }
    const AbstractHeap& operator[](unsigned number) { return at(number); }

private:
    
    // We use the fact that the indexed heap already has a superset of the
    // functionality we need.
    IndexedAbstractHeap m_indexedHeap;
};

class AbsoluteAbstractHeap {
public:
    AbsoluteAbstractHeap(LContext, AbstractHeap* parent, const char* heapName);
    ~AbsoluteAbstractHeap();
    
    const AbstractHeap& atAnyAddress() const { return m_indexedHeap.atAnyIndex(); }
    
    const AbstractHeap& at(void* address)
    {
        return m_indexedHeap.at(bitwise_cast<ptrdiff_t>(address));
    }
    
    const AbstractHeap& operator[](void* address) { return at(address); }

private:
    // The trick here is that the indexed heap is "indexed" by a pointer-width
    // integer. Pointers are themselves pointer-width integers. So we can reuse
    // all of the functionality.
    IndexedAbstractHeap m_indexedHeap;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLAbstractHeap_h

