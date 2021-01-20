/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DOMWrapperWorld.h"
#include "JSDOMWrapper.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/Weak.h>
#include <wtf/Variant.h>

namespace WebCore {

class JSValueInWrappedObject {
public:
    JSValueInWrappedObject(JSC::JSValue = { });
    JSValueInWrappedObject(const JSValueInWrappedObject&);
    operator JSC::JSValue() const;
    explicit operator bool() const;
    JSValueInWrappedObject& operator=(const JSValueInWrappedObject& other);
    void visit(JSC::SlotVisitor&) const;
    void clear();

private:
    // Use a weak pointer here so that if this code or client code has a visiting mistake,
    // we get null rather than a dangling pointer to a deleted object.
    using Weak = JSC::Weak<JSC::JSCell>;
    // FIXME: Would storing a separate JSValue alongside a Weak be better than using a Variant?
    using Value = Variant<JSC::JSValue, Weak>;
    static Value makeValue(JSC::JSValue);
    Value m_value;
};

JSC::JSValue cachedPropertyValue(JSC::JSGlobalObject&, const JSDOMObject& owner, JSValueInWrappedObject& cacheSlot, const WTF::Function<JSC::JSValue()>&);

inline auto JSValueInWrappedObject::makeValue(JSC::JSValue value) -> Value
{
    if (!value.isCell())
        return value;
    // FIXME: This is not quite right. It is possible that this value is being
    // stored in a wrapped object that does not yet have a wrapper. If garbage
    // collection occurs before the wrapped object gets a wrapper, it's possible
    // the value object could be collected, and this will become null. A future
    // version of this class should prevent the value from being collected in
    // that case. Unclear if this can actually happen in practice.
    return Weak { value.asCell() };
}

inline JSValueInWrappedObject::JSValueInWrappedObject(JSC::JSValue value)
    : m_value(makeValue(JSC::JSValue(value)))
{
}

inline JSValueInWrappedObject::JSValueInWrappedObject(const JSValueInWrappedObject& value)
    : m_value(makeValue(value))
{
}

inline JSValueInWrappedObject::operator JSC::JSValue() const
{
    return WTF::switchOn(m_value, [] (JSC::JSValue value) {
        return value;
    }, [] (const Weak& value) {
        return value.get();
    });
}

inline JSValueInWrappedObject::operator bool() const
{
    return JSC::JSValue { *this }.operator bool();
}

inline JSValueInWrappedObject& JSValueInWrappedObject::operator=(const JSValueInWrappedObject& other)
{
    m_value = makeValue(JSC::JSValue(other));
    return *this;
}

inline void JSValueInWrappedObject::visit(JSC::SlotVisitor& visitor) const
{
    return WTF::switchOn(m_value, [] (JSC::JSValue) {
        // Nothing to visit.
    }, [&visitor] (const Weak& value) {
        visitor.append(value);
    });
}

inline void JSValueInWrappedObject::clear()
{
    WTF::switchOn(m_value, [] (Weak& value) {
        value.clear();
    }, [] (auto&) { });
}

inline JSC::JSValue cachedPropertyValue(JSC::JSGlobalObject& lexicalGlobalObject, const JSDOMObject& owner, JSValueInWrappedObject& cachedValue, const WTF::Function<JSC::JSValue()>& function)
{
    if (cachedValue && isWorldCompatible(lexicalGlobalObject, cachedValue))
        return cachedValue;
    cachedValue = cloneAcrossWorlds(lexicalGlobalObject, owner, function());
    ASSERT(isWorldCompatible(lexicalGlobalObject, cachedValue));
    return cachedValue;
}

} // namespace WebCore
