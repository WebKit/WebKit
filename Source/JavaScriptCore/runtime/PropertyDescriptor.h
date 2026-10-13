/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/DefinePropertyAttributes.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/PropertySlot.h>

namespace JSC {

class GetterSetter;

class PropertyDescriptor {
public:
    PropertyDescriptor()
        : m_attributes(defaultAttributes)
        , m_seenAttributes(0)
    {
    }
    PropertyDescriptor(JSValue value, unsigned attributes)
        : m_value(value)
        , m_attributes(attributes)
        , m_seenAttributes(EnumerablePresent | ConfigurablePresent | WritablePresent)
    {
        ASSERT(m_value);
        ASSERT(!m_value.isGetterSetterSlow());
        ASSERT(!m_value.isCustomGetterSetterSlow());
    }
    JS_EXPORT_PRIVATE bool LEGACY_NODELETE writable() const;
    JS_EXPORT_PRIVATE bool LEGACY_NODELETE enumerable() const;
    JS_EXPORT_PRIVATE bool LEGACY_NODELETE configurable() const;
    JS_EXPORT_PRIVATE bool LEGACY_NODELETE isDataDescriptor() const;
    bool LEGACY_NODELETE isGenericDescriptor() const;
    JS_EXPORT_PRIVATE bool LEGACY_NODELETE isAccessorDescriptor() const;
    unsigned attributes() const { return m_attributes; }
    JSValue value() const { return m_value; }
    GetterSetter* slowGetterSetter(JSGlobalObject*) const; // Be aware that this will lazily allocate a GetterSetter object. It's much better to use LEGACY_NODELETE getter() and setter() individually if possible.
    JS_EXPORT_PRIVATE JSValue getter() const;
    JS_EXPORT_PRIVATE JSValue setter() const;
    JSObject* LEGACY_NODELETE getterObject() const;
    JSObject* LEGACY_NODELETE setterObject() const;
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setUndefined();
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setDescriptor(JSValue, unsigned attributes);
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setAccessorDescriptor(unsigned attributes);
    JS_EXPORT_PRIVATE void setAccessorDescriptor(GetterSetter* accessor, unsigned attributes);
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setWritable(bool);
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setEnumerable(bool);
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setConfigurable(bool);
    void setValue(JSValue value) { m_value = value; }
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setSetter(JSValue);
    JS_EXPORT_PRIVATE void LEGACY_NODELETE setGetter(JSValue);
    bool isEmpty() const { return !(m_value || m_getter || m_setter || m_seenAttributes); }
    bool writablePresent() const { return m_seenAttributes & WritablePresent; }
    bool enumerablePresent() const { return m_seenAttributes & EnumerablePresent; }
    bool configurablePresent() const { return m_seenAttributes & ConfigurablePresent; }
    bool setterPresent() const { return !!m_setter; }
    bool getterPresent() const { return !!m_getter; }
    bool equalTo(JSGlobalObject*, const PropertyDescriptor& other) const;
    bool LEGACY_NODELETE attributesEqual(const PropertyDescriptor& other) const;
    unsigned LEGACY_NODELETE attributesOverridingCurrent(const PropertyDescriptor& current) const;

    JS_EXPORT_PRIVATE bool setPropertySlot(JSGlobalObject*, PropertyName, const PropertySlot&);

private:
    static constexpr unsigned defaultAttributes = PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly;

    bool operator==(const PropertyDescriptor&) { return false; }
    enum { WritablePresent = 1, EnumerablePresent = 2, ConfigurablePresent = 4};
    // May be a getter/setter
    JSValue m_value;
    JSValue m_getter;
    JSValue m_setter;
    unsigned m_attributes;
    unsigned m_seenAttributes;
};

inline PropertyDescriptor toPropertyDescriptor(JSValue value, JSValue getter, JSValue setter, DefinePropertyAttributes attributes)
{
    // We assume that validation is already done.
    PropertyDescriptor desc;

    if (std::optional<bool> enumerable = attributes.enumerable())
        desc.setEnumerable(enumerable.value());

    if (std::optional<bool> configurable = attributes.configurable())
        desc.setConfigurable(configurable.value());

    if (attributes.hasValue())
        desc.setValue(value);

    if (std::optional<bool> writable = attributes.writable())
        desc.setWritable(writable.value());

    if (attributes.hasGet())
        desc.setGetter(getter);

    if (attributes.hasSet())
        desc.setSetter(setter);

    return desc;
}

}
