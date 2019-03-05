/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GCAssertions.h"
#include "HandleTypes.h"
#include <type_traits>
#include <wtf/DumbPtrTraits.h>
#include <wtf/DumbValueTraits.h>

namespace JSC {

namespace DFG {
class DesiredWriteBarrier;
}

class JSCell;
class VM;
class JSGlobalObject;

template<class T>
using WriteBarrierTraitsSelect = typename std::conditional<std::is_same<T, Unknown>::value,
    DumbValueTraits<T>, DumbPtrTraits<T>
>::type;

template<class T, typename Traits = WriteBarrierTraitsSelect<T>> class WriteBarrierBase;
template<> class WriteBarrierBase<JSValue>;

JS_EXPORT_PRIVATE void slowValidateCell(JSCell*);
JS_EXPORT_PRIVATE void slowValidateCell(JSGlobalObject*);
    
#if ENABLE(GC_VALIDATION)
template<class T> inline void validateCell(T cell)
{
    ASSERT_GC_OBJECT_INHERITS(cell, std::remove_pointer<T>::type::info());
}

template<> inline void validateCell<JSCell*>(JSCell* cell)
{
    slowValidateCell(cell);
}

template<> inline void validateCell<JSGlobalObject*>(JSGlobalObject* globalObject)
{
    slowValidateCell(globalObject);
}
#else
template<class T> inline void validateCell(T)
{
}
#endif

// We have a separate base class with no constructors for use in Unions.
template <typename T, typename Traits> class WriteBarrierBase {
    using StorageType = typename Traits::StorageType;

public:
    void set(VM&, const JSCell* owner, T* value);
    
    // This is meant to be used like operator=, but is called copyFrom instead, in
    // order to kindly inform the C++ compiler that its advice is not appreciated.
    void copyFrom(const WriteBarrierBase& other)
    {
        // FIXME add version with different Traits once needed.
        Traits::exchange(m_cell, other.m_cell);
    }

    void setMayBeNull(VM&, const JSCell* owner, T* value);

    // Should only be used by JSCell during early initialisation
    // when some basic types aren't yet completely instantiated
    void setEarlyValue(VM&, const JSCell* owner, T* value);
    
    T* get() const
    {
        // Copy m_cell to a local to avoid multiple-read issues. (See <http://webkit.org/b/110854>)
        StorageType cell = m_cell;
        if (cell)
            validateCell(reinterpret_cast<JSCell*>(static_cast<void*>(Traits::unwrap(cell))));
        return Traits::unwrap(cell);
    }

    T* operator*() const
    {
        StorageType cell = m_cell;
        ASSERT(cell);
        auto unwrapped = Traits::unwrap(cell);
        validateCell<T>(unwrapped);
        return Traits::unwrap(unwrapped);
    }

    T* operator->() const
    {
        StorageType cell = m_cell;
        ASSERT(cell);
        auto unwrapped = Traits::unwrap(cell);
        validateCell(unwrapped);
        return unwrapped;
    }

    void clear() { Traits::exchange(m_cell, nullptr); }

    // Slot cannot be used when pointers aren't stored as-is.
    template<typename BarrierT, typename BarrierTraits, std::enable_if_t<std::is_same<BarrierTraits, DumbPtrTraits<BarrierT>>::value, void*> = nullptr>
    struct SlotHelper {
        static BarrierT** reinterpret(typename BarrierTraits::StorageType* cell) { return reinterpret_cast<T**>(cell); }
    };

    T** slot()
    {
        return SlotHelper<T, Traits>::reinterpret(&m_cell);
    }
    
    explicit operator bool() const { return !!m_cell; }
    
    bool operator!() const { return !m_cell; }

    void setWithoutWriteBarrier(T* value)
    {
#if ENABLE(WRITE_BARRIER_PROFILING)
        WriteBarrierCounters::usesWithoutBarrierFromCpp.count();
#endif
        Traits::exchange(this->m_cell, value);
    }

    T* unvalidatedGet() const { return Traits::unwrap(m_cell); }

private:
    StorageType m_cell;
};

template <> class WriteBarrierBase<Unknown, DumbValueTraits<Unknown>> {
public:
    void set(VM&, const JSCell* owner, JSValue);
    void setWithoutWriteBarrier(JSValue value)
    {
        m_value = JSValue::encode(value);
    }

    JSValue get() const
    {
        return JSValue::decode(m_value);
    }
    void clear() { m_value = JSValue::encode(JSValue()); }
    void setUndefined() { m_value = JSValue::encode(jsUndefined()); }
    void setStartingValue(JSValue value) { m_value = JSValue::encode(value); }
    bool isNumber() const { return get().isNumber(); }
    bool isInt32() const { return get().isInt32(); }
    bool isObject() const { return get().isObject(); }
    bool isNull() const { return get().isNull(); }
    bool isGetterSetter() const { return get().isGetterSetter(); }
    bool isCustomGetterSetter() const { return get().isCustomGetterSetter(); }
    
    JSValue* slot() const
    { 
        return bitwise_cast<JSValue*>(&m_value);
    }
    
    int32_t* tagPointer() { return &bitwise_cast<EncodedValueDescriptor*>(&m_value)->asBits.tag; }
    int32_t* payloadPointer() { return &bitwise_cast<EncodedValueDescriptor*>(&m_value)->asBits.payload; }
    
    explicit operator bool() const { return !!get(); }
    bool operator!() const { return !get(); } 
    
private:
    EncodedJSValue m_value;
};

template <typename T, typename Traits = WriteBarrierTraitsSelect<T>>
class WriteBarrier : public WriteBarrierBase<T, Traits> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WriteBarrier()
    {
        this->setWithoutWriteBarrier(0);
    }

    WriteBarrier(VM& vm, const JSCell* owner, T* value)
    {
        this->set(vm, owner, value);
    }

    WriteBarrier(DFG::DesiredWriteBarrier&, T* value)
    {
        ASSERT(isCompilationThread());
        this->setWithoutWriteBarrier(value);
    }

    enum MayBeNullTag { MayBeNull };
    WriteBarrier(VM& vm, const JSCell* owner, T* value, MayBeNullTag)
    {
        this->setMayBeNull(vm, owner, value);
    }
};

enum UndefinedWriteBarrierTagType { UndefinedWriteBarrierTag };
template <>
class WriteBarrier<Unknown, DumbValueTraits<Unknown>> : public WriteBarrierBase<Unknown, DumbValueTraits<Unknown>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WriteBarrier()
    {
        this->setWithoutWriteBarrier(JSValue());
    }
    WriteBarrier(UndefinedWriteBarrierTagType)
    {
        this->setWithoutWriteBarrier(jsUndefined());
    }

    WriteBarrier(VM& vm, const JSCell* owner, JSValue value)
    {
        this->set(vm, owner, value);
    }

    WriteBarrier(DFG::DesiredWriteBarrier&, JSValue value)
    {
        ASSERT(isCompilationThread());
        this->setWithoutWriteBarrier(value);
    }
};

template <typename U, typename V, typename TraitsU, typename TraitsV>
inline bool operator==(const WriteBarrierBase<U, TraitsU>& lhs, const WriteBarrierBase<V, TraitsV>& rhs)
{
    return lhs.get() == rhs.get();
}

} // namespace JSC
