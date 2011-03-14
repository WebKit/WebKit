/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WriteBarrier_h
#define WriteBarrier_h

#include "JSValue.h"

namespace JSC {
class JSCell;
class JSGlobalData;

typedef enum { } Unknown;
typedef JSValue* HandleSlot;

// FIXME: Remove all uses of this class.
template <class T> class DeprecatedPtr {
public:
    DeprecatedPtr() : m_cell(0) { }
    DeprecatedPtr(T* cell) : m_cell(reinterpret_cast<JSCell*>(cell)) { }
    T* get() const { return reinterpret_cast<T*>(m_cell); }
    T* operator*() const { return static_cast<T*>(m_cell); }
    T* operator->() const { return static_cast<T*>(m_cell); }
    
    JSCell** slot() { return &m_cell; }
    
    typedef T* (DeprecatedPtr::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_cell ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }

    bool operator!() const { return !m_cell; }

protected:
    JSCell* m_cell;
};

// FIXME: Remove all uses of this class.
template <> class DeprecatedPtr<Unknown> {
public:
    DeprecatedPtr() { }
    DeprecatedPtr(JSValue value) : m_value(value) { }
    DeprecatedPtr(JSCell* value) : m_value(value) { }
    const JSValue& get() const { return m_value; }
    const JSValue* operator*() const { return &m_value; }
    const JSValue* operator->() const { return &m_value; }
    
    JSValue* slot() { return &m_value; }
    
    typedef JSValue (DeprecatedPtr::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_value ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    bool operator!() const { return !m_value; }
    
private:
    JSValue m_value;
};

template <typename U, typename V> inline bool operator==(const DeprecatedPtr<U>& lhs, const DeprecatedPtr<V>& rhs)
{
    return lhs.get() == rhs.get();
}

template <typename T> struct JSValueChecker {
    static const bool IsJSValue = false;
};

template <> struct JSValueChecker<JSValue> {
    static const bool IsJSValue = true;
};

// We have a separate base class with no constructors for use in Unions.
template <typename T> class WriteBarrierBase {
public:
    COMPILE_ASSERT(!JSValueChecker<T>::IsJSValue, WriteBarrier_JSValue_is_invalid__use_unknown);
    void set(JSGlobalData&, const JSCell*, T* value) { this->m_cell = reinterpret_cast<JSCell*>(value); }
    
    T* get() const { return reinterpret_cast<T*>(m_cell); }
    T* operator*() const { return static_cast<T*>(m_cell); }
    T* operator->() const { return static_cast<T*>(m_cell); }
    void clear() { m_cell = 0; }
    
    JSCell** slot() { return &m_cell; }
    
    typedef T* (WriteBarrierBase::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_cell ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    
    bool operator!() const { return !m_cell; }

    void setWithoutWriteBarrier(T* value) { this->m_cell = reinterpret_cast<JSCell*>(value); }

private:
    JSCell* m_cell;
};

template <> class WriteBarrierBase<Unknown> {
public:
    void set(JSGlobalData&, const JSCell*, JSValue value) { m_value = JSValue::encode(value); }
    void setWithoutWriteBarrier(JSValue value) { m_value = JSValue::encode(value); }
    JSValue get() const { return JSValue::decode(m_value); }
    void clear() { m_value = JSValue::encode(JSValue()); }
    void setUndefined() { m_value = JSValue::encode(jsUndefined()); }
    bool isNumber() const { return get().isNumber(); }
    bool isGetterSetter() const { return get().isGetterSetter(); }
    
    JSValue* slot()
    { 
        union {
            EncodedJSValue* v;
            JSValue* slot;
        } u;
        u.v = &m_value;
        return u.slot;
    }
    
    typedef JSValue (WriteBarrierBase::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return get() ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    bool operator!() const { return !get(); } 
    
private:
    EncodedJSValue m_value;
};

template <typename T> class WriteBarrier : public WriteBarrierBase<T> {
public:
    WriteBarrier()
    {
        this->setWithoutWriteBarrier(0);
    }

    WriteBarrier(JSGlobalData& globalData, const JSCell* owner, T* value)
    {
        this->set(globalData, owner, value);
    }
};

template <> class WriteBarrier<Unknown> : public WriteBarrierBase<Unknown> {
public:
    WriteBarrier()
    {
        this->setWithoutWriteBarrier(JSValue());
    }

    WriteBarrier(JSGlobalData& globalData, const JSCell* owner, JSValue value)
    {
        this->set(globalData, owner, value);
    }
};

template <typename U, typename V> inline bool operator==(const WriteBarrierBase<U>& lhs, const WriteBarrierBase<V>& rhs)
{
    return lhs.get() == rhs.get();
}

// For use in data members that are owned by the Heap.
template <typename T> class HeapRoot : public WriteBarrier<T> {
private:
    friend class Heap;
    friend class JSGlobalData; // FIXME: Move Heap roots from JSGlobalData to Heap.
    friend class SmallStrings; // FIXME: Convert SmallStrings to use weak handles.

    HeapRoot() { }
    HeapRoot(T* value)
    {
        this->setWithoutWriteBarrier(value);
    }

public:
    HeapRoot& operator=(T* value)
    {
        this->setWithoutWriteBarrier(value);
        return *this;
    }
};

// For use in data members that are owned by the Heap.
template <> class HeapRoot<Unknown> : public WriteBarrier<Unknown> {
private:
    friend class Heap;
    friend class JSGlobalData; // FIXME: Move Heap roots from JSGlobalData to Heap.
    friend class SmallStrings; // FIXME: Convert SmallStrings to use weak handles.

    HeapRoot() { }
    HeapRoot(JSValue value)
    {
        this->setWithoutWriteBarrier(value);
    }

public:
    HeapRoot& operator=(JSValue value)
    {
        this->setWithoutWriteBarrier(value);
        return *this;
    }
};

} // namespace JSC

#endif // WriteBarrier_h
