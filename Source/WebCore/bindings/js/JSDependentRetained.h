/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSDependentRetained_h
#define JSDependentRetained_h

#include "JSDOMGlobalObject.h"
#include <heap/Weak.h>
#include <runtime/JSObject.h>
#include <runtime/PrivateName.h>
#include <wtf/Forward.h>

namespace WebCore {

class JSDependentRetained {
public:
    JSDependentRetained(JSC::JSObject* owner, JSC::JSObject* value, JSDOMGlobalObject* globalObject)
        : m_value(value)
        , m_globalObject(globalObject)
    {
        ASSERT(value);
        if (owner)
            retain(owner);
    }

    ~JSDependentRetained()
    {
        release();
    }

    JSC::JSObject* get() const
    {
        return m_value.get();
    }

    bool isEmpty() const
    {
        return !m_owner || !m_value;
    }

    void retain(JSC::JSObject* owner)
    {
        ASSERT(!m_owner && owner);
        ASSERT(m_value);
        m_owner = JSC::PassWeak<JSC::JSObject>(owner);
        m_owner->putDirect(m_globalObject->globalData(), m_propertyName, get());
    }

private:

    void release()
    {
        if (m_owner)
            m_owner->removeDirect(m_globalObject->globalData(), m_propertyName);
        m_value.clear();
        m_owner.clear();
    }

    JSC::Weak<JSC::JSObject> m_owner;
    JSC::Weak<JSC::JSObject> m_value;
    JSC::PrivateName m_propertyName;
    JSC::Weak<JSDOMGlobalObject> m_globalObject;
};

} // namespace WebCore

#endif // JSDependentRetained_h
