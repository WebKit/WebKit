/*
 * Copyright (C) 2013, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2018 Igalia S.L.
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
#include "JSWeakValue.h"

#include "JSCInlines.h"

namespace JSC {

JSWeakValue::~JSWeakValue()
{
    clear();
}

void JSWeakValue::clear()
{
    switch (m_tag) {
    case WeakTypeTag::NotSet:
        return;
    case WeakTypeTag::Primitive:
        m_value.primitive = JSValue();
        return;
    case WeakTypeTag::Object:
        m_value.object.clear();
        return;
    case WeakTypeTag::String:
        m_value.string.clear();
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool JSWeakValue::isClear() const
{
    switch (m_tag) {
    case WeakTypeTag::NotSet:
        return true;
    case WeakTypeTag::Primitive:
        return !m_value.primitive;
    case WeakTypeTag::Object:
        return !m_value.object;
    case WeakTypeTag::String:
        return !m_value.string;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void JSWeakValue::setPrimitive(JSValue primitive)
{
    ASSERT(!isSet());
    ASSERT(!m_value.primitive);
    ASSERT(primitive.isPrimitive());
    m_tag = WeakTypeTag::Primitive;
    m_value.primitive = primitive;
}

void JSWeakValue::setObject(JSObject* object, WeakHandleOwner& owner, void* context)
{
    ASSERT(!isSet());
    ASSERT(!m_value.object);
    m_tag = WeakTypeTag::Object;
    Weak<JSObject> weak(object, &owner, context);
    m_value.object.swap(weak);
}

void JSWeakValue::setString(JSString* string, WeakHandleOwner& owner, void* context)
{
    ASSERT(!isSet());
    ASSERT(!m_value.string);
    m_tag = WeakTypeTag::String;
    Weak<JSString> weak(string, &owner, context);
    m_value.string.swap(weak);
}

} // namespace JSC
