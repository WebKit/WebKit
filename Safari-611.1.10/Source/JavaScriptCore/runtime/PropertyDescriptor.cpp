/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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
#include "PropertyDescriptor.h"

#include "GetterSetter.h"
#include "JSCJSValueInlines.h"

namespace JSC {

bool PropertyDescriptor::writable() const
{
    ASSERT(!isAccessorDescriptor());
    return !(m_attributes & PropertyAttribute::ReadOnly);
}

bool PropertyDescriptor::enumerable() const
{
    return !(m_attributes & PropertyAttribute::DontEnum);
}

bool PropertyDescriptor::configurable() const
{
    return !(m_attributes & PropertyAttribute::DontDelete);
}

bool PropertyDescriptor::isDataDescriptor() const
{
    return m_value || (m_seenAttributes & WritablePresent);
}

bool PropertyDescriptor::isGenericDescriptor() const
{
    return !isAccessorDescriptor() && !isDataDescriptor();
}

bool PropertyDescriptor::isAccessorDescriptor() const
{
    return m_getter || m_setter;
}

void PropertyDescriptor::setUndefined()
{
    m_value = jsUndefined();
    m_attributes = PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete | PropertyAttribute::DontEnum;
}

GetterSetter* PropertyDescriptor::slowGetterSetter(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSValue getter = m_getter && !m_getter.isUndefined() ? jsCast<JSObject*>(m_getter) : jsUndefined();
    JSValue setter = m_setter && !m_setter.isUndefined() ? jsCast<JSObject*>(m_setter) : jsUndefined();
    return GetterSetter::create(vm, globalObject, getter, setter);
}

JSValue PropertyDescriptor::getter() const
{
    ASSERT(isAccessorDescriptor());
    return m_getter;
}

JSValue PropertyDescriptor::setter() const
{
    ASSERT(isAccessorDescriptor());
    return m_setter;
}

JSObject* PropertyDescriptor::getterObject() const
{
    ASSERT(isAccessorDescriptor() && getterPresent());
    return m_getter.isObject() ? asObject(m_getter) : nullptr;
}

JSObject* PropertyDescriptor::setterObject() const
{
    ASSERT(isAccessorDescriptor() && setterPresent());
    return m_setter.isObject() ? asObject(m_setter) : nullptr;
}

void PropertyDescriptor::setDescriptor(JSValue value, unsigned attributes)
{
    ASSERT(value);

    // We need to mask off the PropertyAttribute::CustomValue bit because
    // PropertyDescriptor::attributesEqual() does an equivalent test on
    // m_attributes, and a property that has a CustomValue should be indistinguishable
    // from a property that has a normal value as far as JS code is concerned.
    // PropertyAttribute does not need knowledge of the underlying implementation
    // actually being a CustomValue. So, we'll just mask it off up front here.
    m_attributes = attributes & ~PropertyAttribute::CustomValue;
    if (value.isGetterSetter()) {
        m_attributes &= ~PropertyAttribute::ReadOnly; // FIXME: we should be able to ASSERT this!

        GetterSetter* accessor = jsCast<GetterSetter*>(value);
        m_getter = !accessor->isGetterNull() ? accessor->getter() : jsUndefined();
        m_setter = !accessor->isSetterNull() ? accessor->setter() : jsUndefined();
        m_seenAttributes = EnumerablePresent | ConfigurablePresent;
    } else {
        m_value = value;
        m_seenAttributes = EnumerablePresent | ConfigurablePresent | WritablePresent;
    }
}

void PropertyDescriptor::setCustomDescriptor(unsigned attributes)
{
    ASSERT(!(attributes & PropertyAttribute::CustomValue));
    m_attributes = attributes | PropertyAttribute::Accessor | PropertyAttribute::CustomAccessor;
    m_attributes &= ~PropertyAttribute::ReadOnly;
    m_seenAttributes = EnumerablePresent | ConfigurablePresent;
    setGetter(jsUndefined());
    setSetter(jsUndefined());
    m_value = JSValue();
}

void PropertyDescriptor::setAccessorDescriptor(GetterSetter* accessor, unsigned attributes)
{
    ASSERT(attributes & PropertyAttribute::Accessor);
    ASSERT(!(attributes & PropertyAttribute::CustomValue));
    attributes &= ~PropertyAttribute::ReadOnly; // FIXME: we should be able to ASSERT this!

    m_attributes = attributes;
    m_getter = !accessor->isGetterNull() ? accessor->getter() : jsUndefined();
    m_setter = !accessor->isSetterNull() ? accessor->setter() : jsUndefined();
    m_seenAttributes = EnumerablePresent | ConfigurablePresent;
}

void PropertyDescriptor::setWritable(bool writable)
{
    if (writable)
        m_attributes &= ~PropertyAttribute::ReadOnly;
    else
        m_attributes |= PropertyAttribute::ReadOnly;
    m_seenAttributes |= WritablePresent;
}

void PropertyDescriptor::setEnumerable(bool enumerable)
{
    if (enumerable)
        m_attributes &= ~PropertyAttribute::DontEnum;
    else
        m_attributes |= PropertyAttribute::DontEnum;
    m_seenAttributes |= EnumerablePresent;
}

void PropertyDescriptor::setConfigurable(bool configurable)
{
    if (configurable)
        m_attributes &= ~PropertyAttribute::DontDelete;
    else
        m_attributes |= PropertyAttribute::DontDelete;
    m_seenAttributes |= ConfigurablePresent;
}

void PropertyDescriptor::setSetter(JSValue setter)
{
    m_setter = setter;
    m_attributes |= PropertyAttribute::Accessor;
    m_attributes &= ~PropertyAttribute::ReadOnly;
}

void PropertyDescriptor::setGetter(JSValue getter)
{
    m_getter = getter;
    m_attributes |= PropertyAttribute::Accessor;
    m_attributes &= ~PropertyAttribute::ReadOnly;
}

bool PropertyDescriptor::equalTo(JSGlobalObject* globalObject, const PropertyDescriptor& other) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (other.m_value.isEmpty() != m_value.isEmpty()
        || other.m_getter.isEmpty() != m_getter.isEmpty()
        || other.m_setter.isEmpty() != m_setter.isEmpty())
        return false;
    if (m_value) {
        bool isSame = sameValue(globalObject, other.m_value, m_value);
        RETURN_IF_EXCEPTION(scope, false);
        if (!isSame)
            return false;
    }
    return (!m_getter || JSValue::strictEqual(globalObject, other.m_getter, m_getter))
        && (!m_setter || JSValue::strictEqual(globalObject, other.m_setter, m_setter))
        && attributesEqual(other);
}

bool PropertyDescriptor::attributesEqual(const PropertyDescriptor& other) const
{
    unsigned mismatch = other.m_attributes ^ m_attributes;
    unsigned sharedSeen = other.m_seenAttributes & m_seenAttributes;
    if (sharedSeen & WritablePresent && mismatch & PropertyAttribute::ReadOnly)
        return false;
    if (sharedSeen & ConfigurablePresent && mismatch & PropertyAttribute::DontDelete)
        return false;
    if (sharedSeen & EnumerablePresent && mismatch & PropertyAttribute::DontEnum)
        return false;
    return true;
}

unsigned PropertyDescriptor::attributesOverridingCurrent(const PropertyDescriptor& current) const
{
    unsigned currentAttributes = current.m_attributes;
    if (isDataDescriptor() && current.isAccessorDescriptor())
        currentAttributes |= PropertyAttribute::ReadOnly;
    unsigned overrideMask = 0;
    if (writablePresent())
        overrideMask |= PropertyAttribute::ReadOnly;
    if (enumerablePresent())
        overrideMask |= PropertyAttribute::DontEnum;
    if (configurablePresent())
        overrideMask |= PropertyAttribute::DontDelete;
    if (isAccessorDescriptor())
        overrideMask |= PropertyAttribute::Accessor;
    return (m_attributes & overrideMask) | (currentAttributes & ~overrideMask & ~PropertyAttribute::CustomAccessor);
}

}
