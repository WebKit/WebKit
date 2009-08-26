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


#include "config.h"

#include "PropertyDescriptor.h"

#include "GetterSetter.h"
#include "JSObject.h"

namespace JSC {
bool PropertyDescriptor::writable() const
{
    ASSERT(!hasAccessors());
    return !(m_attributes & ReadOnly);
}

bool PropertyDescriptor::enumerable() const
{
    ASSERT(isValid());
    return !(m_attributes & DontEnum);
}

bool PropertyDescriptor::configurable() const
{
    ASSERT(isValid());
    return !(m_attributes & DontDelete);
}

bool PropertyDescriptor::hasAccessors() const
{
    return !!(m_attributes & (Getter | Setter));
}

void PropertyDescriptor::setUndefined()
{
    m_value = jsUndefined();
    m_attributes = ReadOnly | DontDelete | DontEnum;
}

JSValue PropertyDescriptor::getter() const
{
    ASSERT(hasAccessors());
    if (!m_getter)
        return jsUndefined();
    return m_getter;
}

JSValue PropertyDescriptor::setter() const
{
    ASSERT(hasAccessors());
    if (!m_setter)
        return jsUndefined();
    return m_setter;
}

void PropertyDescriptor::setDescriptor(JSValue value, unsigned attributes)
{
    ASSERT(value);
    if (attributes & (Getter | Setter)) {
        GetterSetter* accessor = asGetterSetter(value);
        m_getter = accessor->getter();
        m_setter = accessor->setter();
        ASSERT(m_getter || m_setter);
    } else {
        m_value = value;
    }
    m_attributes = attributes;
}

void PropertyDescriptor::setAccessorDescriptor(JSValue getter, JSValue setter, unsigned attributes)
{
    ASSERT(attributes & (Getter | Setter));
    ASSERT(getter || setter);
    m_attributes = attributes;
    m_getter = getter;
    m_setter = setter;
}

}
