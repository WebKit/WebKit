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

#ifndef SlotAccessor_h
#define SlotAccessor_h

#include "JSValue.h"

namespace JSC {

class JSCell;

typedef enum { } Unknown;
typedef JSValue* HandleSlot;

template <typename T> struct SlotTypes {
    typedef T* ExternalType;
    typedef JSCell* ExternalTypeBase;
    static ExternalType getFromBaseType(ExternalTypeBase base) { return reinterpret_cast<ExternalType>(base); }
    static ExternalTypeBase convertToBaseType(ExternalType value) { return reinterpret_cast<ExternalTypeBase>(value); }
    static ExternalType getFromSlot(HandleSlot slot) { return (slot && *slot) ? reinterpret_cast<ExternalType>(slot->asCell()) : 0; }
    static JSValue toJSValue(ExternalType cell) { return reinterpret_cast<ExternalTypeBase>(cell); }
    template <typename U> static void validateUpcast() { T* temp; temp = (U*)0; }
};

template <> struct SlotTypes<Unknown> {
    typedef JSValue ExternalType;
    typedef JSValue ExternalTypeBase;
    static ExternalType getFromBaseType(ExternalTypeBase base) { return base; }
    static ExternalTypeBase convertToBaseType(ExternalType value) { return value; }
    static ExternalType getFromSlot(HandleSlot slot) { return slot ? *slot : JSValue(); }
    static JSValue toJSValue(const JSValue& v) { return v; }
    template <typename U> static void validateUpcast() {}
};

template <typename Base, typename T> struct SlotAccessor {
    T* operator->() { return static_cast<Base*>(this)->get(); }
    const T* operator->() const { return static_cast<const Base*>(this)->get(); }
    T* operator*() { return static_cast<Base*>(this)->get(); }
    const T* operator*() const { return static_cast<const Base*>(this)->get(); }
};

template <typename Base> struct SlotAccessor<Base, Unknown> {
    JSValue* operator->() { return static_cast<Base*>(this)->slot(); }
    const JSValue* operator->() const { return static_cast<const Base*>(this)->slot(); }
};

// JSValue should not be used directly
template <typename Base> struct SlotAccessor<Base, JSValue>;
template <> struct SlotTypes<JSValue>;

}

#endif // SlotAccessor_h
