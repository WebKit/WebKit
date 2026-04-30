/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
 *
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

#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueStructure.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/WeakInlines.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/JSDOMWrapper.h>
#include <WebCore/JSValueInWrappedObject.h>

namespace WebCore {

inline JSValueInWrappedObject::JSValueInWrappedObject(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    setWeakly(globalObject, value);
}

inline JSValueInWrappedObject::JSValueInWrappedObject(RefPtr<DOMWrapperWorld>&& world, JSC::JSValue value)
{
    setWeakly(WTF::move(world), value);
}

inline JSC::JSValue JSValueInWrappedObject::getValue(JSC::JSValue nullValue) const
{
    if (m_nonCell)
        return m_nonCell;
    return m_cell ? m_cell.get() : nullValue;
}

inline JSValueInWrappedObject::operator bool() const
{
    return m_nonCell || m_cell;
}

template<typename Visitor>
inline void JSValueInWrappedObject::visitInGCThread(Visitor& visitor) const
{
    visitor.append(m_cell);
}

template void JSValueInWrappedObject::visitInGCThread(JSC::AbstractSlotVisitor&) const;
template void JSValueInWrappedObject::visitInGCThread(JSC::SlotVisitor&) const;

inline void JSValueInWrappedObject::setValueInternal(JSC::JSValue value)
{
    m_world = nullptr;
    if (!value.isCell()) {
        m_nonCell = value;
        m_cell.clear();
        return;
    }
    m_nonCell = { };
    JSC::Weak weak { value.asCell() };
    WTF::storeStoreFence();
    m_cell = WTF::move(weak);
}

inline void JSValueInWrappedObject::setWorld(JSC::JSGlobalObject& globalObject)
{
    m_world = &currentWorld(globalObject);
}

inline void JSValueInWrappedObject::setWorld(RefPtr<DOMWrapperWorld>&& world)
{
    m_world = WTF::move(world);
}

inline void JSValueInWrappedObject::setWeakly(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    setValueInternal(value);
    setWorld(globalObject);
}

inline void JSValueInWrappedObject::setWeakly(RefPtr<DOMWrapperWorld>&& world, JSC::JSValue value)
{
    setValueInternal(value);
    setWorld(WTF::move(world));
}

inline void JSValueInWrappedObject::set(JSC::JSGlobalObject& globalObject, const JSC::JSCell* owner, JSC::JSValue value)
{
    setValueInternal(value);
    if (owner)
        globalObject.vm().writeBarrier(owner, value);
    setWorld(globalObject);
}

inline void JSValueInWrappedObject::set(RefPtr<DOMWrapperWorld>&& world, JSC::VM& vm, const JSC::JSCell* owner, JSC::JSValue value)
{
    setValueInternal(value);
    if (owner)
        vm.writeBarrier(owner, value);
    setWorld(WTF::move(world));
}

inline void JSValueInWrappedObject::clear()
{
    m_nonCell = { };
    m_cell.clear();
    m_world = nullptr;
}

inline RefPtr<DOMWrapperWorld> JSValueInWrappedObject::world() const
{
    return m_world.get();
}

inline bool JSValueInWrappedObject::isWorldCompatible(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    JSC::JSValue value = getValue();
    if (!value.isObject())
        return true;

    if (RefPtr world = m_world.get())
        return world == &currentWorld(lexicalGlobalObject);
    return false;
}

inline JSC::JSValue cachedPropertyValue(JSC::ThrowScope& throwScope, JSC::JSGlobalObject& lexicalGlobalObject, const JSDOMObject& owner, JSValueInWrappedObject& cachedValue, const auto& function)
{
    if (cachedValue && cachedValue.isWorldCompatible(lexicalGlobalObject))
        return cachedValue.getValue();

    auto value = function(throwScope);
    RETURN_IF_EXCEPTION(throwScope, { });

    cachedValue.set(lexicalGlobalObject, &owner, cloneAcrossWorlds(lexicalGlobalObject, owner, value));
    ASSERT(cachedValue.isWorldCompatible(lexicalGlobalObject));
    return cachedValue.getValue();
}

} // namespace WebCore
