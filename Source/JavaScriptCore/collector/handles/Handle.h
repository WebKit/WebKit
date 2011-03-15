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

#ifndef Handle_h
#define Handle_h

#include "WriteBarrier.h"

#include <wtf/Assertions.h>

namespace JSC {

/*
    A Handle is a smart pointer that updates automatically when the garbage
    collector moves the object to which it points.

    The base Handle class represents a temporary reference to a pointer whose
    lifetime is guaranteed by something else.
*/

template <class T> class Handle;

// Creating a JSValue Handle is invalid
template <> class Handle<JSValue>;

class HandleBase {
    friend class HandleHeap;

public:
    bool operator!() const { return isEmpty(); }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef JSValue (HandleBase::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return (m_slot && *m_slot) ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }

    bool isEmpty() const { return !m_slot || !*m_slot; }

protected:
    HandleBase(HandleSlot slot)
        : m_slot(slot)
    {
        ASSERT(slot);
    }

    enum DontNullCheckSlotTag { DontNullCheckSlot };
    HandleBase(HandleSlot slot, DontNullCheckSlotTag)
        : m_slot(slot)
    {
    }
    
    HandleSlot slot() const { return m_slot; }
    void invalidate()
    {
        // It is unsafe to use a handle after invalidating it.
        m_slot = 0;
    }

    // needed by Global<>::operator= and Global<>::set if it's an empty handle
    void setSlot(HandleSlot slot)
    {
        ASSERT(!m_slot);
        ASSERT(slot);
        m_slot = slot;
    }

private:
    HandleSlot m_slot;
};

template <typename T> struct HandleTypes {
    typedef T* ExternalType;
    static ExternalType getFromSlot(HandleSlot slot) { return (slot && *slot) ? reinterpret_cast<ExternalType>(slot->asCell()) : 0; }
    static JSValue toJSValue(T* cell) { return reinterpret_cast<JSCell*>(cell); }
    template <typename U> static void validateUpcast() { T* temp; temp = (U*)0; }
};

template <> struct HandleTypes<Unknown> {
    typedef JSValue ExternalType;
    static ExternalType getFromSlot(HandleSlot slot) { return slot ? *slot : JSValue(); }
    static JSValue toJSValue(const JSValue& v) { return v; }
    template <typename U> static void validateUpcast() {}
};

template <typename Base, typename T> struct HandleConverter {
    T* operator->() { return static_cast<Base*>(this)->get(); }
    const T* operator->() const { return static_cast<const Base*>(this)->get(); }
    T* operator*() { return static_cast<Base*>(this)->get(); }
    const T* operator*() const { return static_cast<const Base*>(this)->get(); }
};

template <typename Base> struct HandleConverter<Base, Unknown> {
    Handle<JSObject> asObject() const;
    bool isObject() const { return jsValue().isObject(); }
    bool getNumber(double number) const { return jsValue().getNumber(number); }
    UString getString(ExecState*) const;
    bool isUndefinedOrNull() const { return jsValue().isUndefinedOrNull(); }

private:
    JSValue jsValue() const { return static_cast<const Base*>(this)->get(); }
};

template <typename T> class Handle : public HandleBase, public HandleConverter<Handle<T>, T> {
public:
    template <typename A, typename B> friend class HandleConverter;
    typedef typename HandleTypes<T>::ExternalType ExternalType;
    template <typename U> Handle(Handle<U> o)
    {
        typename HandleTypes<T>::template validateUpcast<U>();
        m_slot = o.slot();
    }

    ExternalType get() const { return HandleTypes<T>::getFromSlot(this->slot()); }

protected:

    Handle(HandleSlot slot)
        : HandleBase(slot)
    {
    }
    Handle(HandleSlot slot, HandleBase::DontNullCheckSlotTag)
        : HandleBase(slot, HandleBase::DontNullCheckSlot)
    {
    }
    
private:
    friend class HandleHeap;

    static Handle<T> wrapSlot(HandleSlot slot)
    {
        return Handle<T>(slot);
    }
};

template <typename Base> Handle<JSObject> HandleConverter<Base, Unknown>::asObject() const
{
    return Handle<JSObject>::wrapSlot(static_cast<const Base*>(this)->slot());
}

template <typename T, typename U> inline bool operator==(const Handle<T>& a, const Handle<U>& b)
{ 
    return a.get() == b.get(); 
}

template <typename T, typename U> inline bool operator==(const Handle<T>& a, U* b)
{ 
    return a.get() == b; 
}

template <typename T, typename U> inline bool operator==(T* a, const Handle<U>& b) 
{
    return a == b.get(); 
}

template <typename T, typename U> inline bool operator!=(const Handle<T>& a, const Handle<U>& b)
{ 
    return a.get() != b.get(); 
}

template <typename T, typename U> inline bool operator!=(const Handle<T>& a, U* b)
{
    return a.get() != b; 
}

template <typename T, typename U> inline bool operator!=(T* a, const Handle<U>& b)
{ 
    return a != b.get(); 
}

template <typename T, typename U> inline bool operator!=(const Handle<T>& a, JSValue b)
{
    return a.get() != b; 
}

template <typename T, typename U> inline bool operator!=(JSValue a, const Handle<U>& b)
{ 
    return a != b.get(); 
}

}

#endif
