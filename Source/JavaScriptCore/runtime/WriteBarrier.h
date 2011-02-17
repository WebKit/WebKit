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

#include "SlotAccessor.h"

namespace JSC {
class JSCell;
class JSGlobalData;

template <class T> class DeprecatedPtr : public SlotAccessor<DeprecatedPtr<T>, T> {
public:
    typedef typename SlotTypes<T>::ExternalType ExternalType;
    typedef typename SlotTypes<T>::ExternalTypeBase ExternalTypeBase;
    
    DeprecatedPtr() : m_value() { }
    DeprecatedPtr(ExternalType value) : m_value(SlotTypes<T>::convertToBaseType(value)) { }
    ExternalType get() const { return SlotTypes<T>::getFromBaseType(m_value); }
    
    ExternalTypeBase* slot() { return &m_value; }
    
    typedef ExternalType (DeprecatedPtr::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_value ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    
    bool operator!() const { return !m_value; }
    const DeprecatedPtr& operator=(ExternalType value)
    {
        m_value = SlotTypes<T>::convertToBaseType(value);
        return *this;
    }
    
protected:
    ExternalTypeBase m_value;
};

template <typename T> class WriteBarrierTranslator {
public:
    typedef JSCell* WriteBarrierStorageType;
    static WriteBarrierStorageType convertToStorage(T* cell) { return reinterpret_cast<WriteBarrierStorageType>(cell); }
    static T* convertFromStorage(WriteBarrierStorageType storage) { return reinterpret_cast<T*>(storage); }
};

template <> class WriteBarrierTranslator<Unknown>;

template <typename T> class WriteBarrierBase : public SlotAccessor<DeprecatedPtr<T>, T>, public WriteBarrierTranslator<T> {
public:
    typedef typename SlotTypes<T>::ExternalType ExternalType;
    typedef typename SlotTypes<T>::ExternalTypeBase ExternalTypeBase;
    typedef typename WriteBarrierTranslator<T>::WriteBarrierStorageType StorageType;

    void set(JSGlobalData&, const JSCell*, ExternalType value) { this->m_value = WriteBarrierTranslator<T>::convertToStorage(value); }
    
    ExternalType get() const { return WriteBarrierTranslator<T>::convertFromStorage(m_value); }
    void clear() { m_value = 0; }
    
    ExternalTypeBase* slot()
    {
        union {
            StorageType* intype;
            ExternalTypeBase* outtype;
        } u;
        u.intype = &m_value;
        return u.outtype;
    }
    
    typedef ExternalType (WriteBarrierBase::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_value ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    
    bool operator!() const { return !m_value; }

    void setWithoutWriteBarrier(ExternalType value) { this->m_value = WriteBarrierTranslator<T>::convertToStorage(value); }

protected:
    StorageType m_value;
};

template <> class WriteBarrierTranslator<Unknown> {
public:
    void setUndefined() { static_cast<WriteBarrierBase<Unknown>*>(this)->setWithoutWriteBarrier(jsUndefined()); }
    typedef EncodedJSValue WriteBarrierStorageType;
    static WriteBarrierStorageType convertToStorage(JSValue value) { return JSValue::encode(value); }
    static JSValue convertFromStorage(WriteBarrierStorageType storage) { return JSValue::decode(storage); }
};

template <typename T> class WriteBarrier : public WriteBarrierBase<T> {
public:
    WriteBarrier() { this->clear(); }
    WriteBarrier(JSGlobalData& globalData, const JSCell* owner, typename WriteBarrierBase<T>::ExternalType value)
    {
        this->set(globalData, owner, value);
    }

};

template <typename U, typename V> inline bool operator==(const DeprecatedPtr<U>& lhs, const DeprecatedPtr<V>& rhs)
{
    return lhs.get() == rhs.get();
}

template <typename U, typename V> inline bool operator==(const WriteBarrierBase<U>& lhs, const WriteBarrierBase<V>& rhs)
{
    return lhs.get() == rhs.get();
}

COMPILE_ASSERT(sizeof(WriteBarrier<Unknown>) == sizeof(JSValue), WriteBarrier_Unknown_should_be_sizeof_JSValue);
COMPILE_ASSERT(sizeof(WriteBarrier<JSCell>) == sizeof(JSCell*), WriteBarrier_Unknown_should_be_sizeof_JSCell_pointer);
}

#endif // WriteBarrier_h
