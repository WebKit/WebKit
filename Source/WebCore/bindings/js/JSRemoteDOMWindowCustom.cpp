/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSRemoteDOMWindow.h"

#include "JSDOMExceptionHandling.h"
#include "JSLocalDOMWindowCustom.h"
#include "WebCoreJSClientData.h"

namespace WebCore {
using namespace JSC;

bool JSRemoteDOMWindow::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    if (std::optional<unsigned> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, lexicalGlobalObject, index.value(), slot);

    // FIXME (rdar://115751655): This should be replaced with a same-origin check between the active and target document.
    if (propertyName == "$vm"_s)
        return true;

    auto* thisObject = jsCast<JSRemoteDOMWindow*>(object);
    return jsLocalDOMWindowGetOwnPropertySlotRestrictedAccess<DOMWindowType::Remote>(thisObject, thisObject->wrapped(), *lexicalGlobalObject, propertyName, slot, String());
}

bool JSRemoteDOMWindow::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto* thisObject = jsCast<JSRemoteDOMWindow*>(object);
    auto& window = thisObject->wrapped();
    auto* frame = window.frame();

    // Indexed getters take precendence over regular properties, so caching would be invalid.
    slot.disableCaching();

    // These are also allowed cross-origin, so come before the access check.
    if (frame && index < frame->tree().childCount()) {
        // FIXME: <rdar://118263337> This should work also if it's a RemoteFrame, it should just return a RemoteDOMWindow.
        // JSLocalDOMWindow::getOwnPropertySlotByIndex uses scopedChild. Investigate the difference.
        if (auto* child = dynamicDowncast<LocalFrame>(frame->tree().child(index))) {
            slot.setValue(thisObject, enumToUnderlyingType(JSC::PropertyAttribute::ReadOnly), toJS(lexicalGlobalObject, child->document()->domWindow()));
            return true;
        }
    }

    // FIXME: <rdar://118263337> Make this more like JSLocalDOMWindow::getOwnPropertySlotByIndex and share code when possible.
    // There is some missing functionality here, and it is likely important.

    return jsLocalDOMWindowGetOwnPropertySlotRestrictedAccess<DOMWindowType::Remote>(thisObject, thisObject->wrapped(), *lexicalGlobalObject, Identifier::from(vm, index), slot, String());
}

bool JSRemoteDOMWindow::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsCast<JSRemoteDOMWindow*>(cell);
    if (!thisObject->wrapped().frame())
        return false;

    String errorMessage;

    // We only allow setting "location" attribute cross-origin.
    if (propertyName == builtinNames(vm).locationPublicName()) {
        auto setter = s_info.staticPropHashTable->entry(propertyName)->propertyPutter();
        scope.release();
        setter(lexicalGlobalObject, JSValue::encode(slot.thisValue()), JSValue::encode(value), propertyName);
        return true;
    }

    throwSecurityError(*lexicalGlobalObject, scope, errorMessage);
    return false;
}

bool JSRemoteDOMWindow::putByIndex(JSCell*, JSGlobalObject*, unsigned, JSValue, bool)
{
    return false;
}

bool JSRemoteDOMWindow::deleteProperty(JSCell*, JSGlobalObject* lexicalGlobalObject, PropertyName, DeletePropertySlot&)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*lexicalGlobalObject, scope, String());
    return false;
}

bool JSRemoteDOMWindow::deletePropertyByIndex(JSCell*, JSGlobalObject* lexicalGlobalObject, unsigned)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*lexicalGlobalObject, scope, String());
    return false;
}

void JSRemoteDOMWindow::getOwnPropertyNames(JSObject*, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    // FIXME: Add scoped children indexes.

    if (mode == DontEnumPropertiesMode::Include)
        addCrossOriginOwnPropertyNames<CrossOriginObject::Window>(*lexicalGlobalObject, propertyNames);
}

bool JSRemoteDOMWindow::defineOwnProperty(JSC::JSObject*, JSC::JSGlobalObject* lexicalGlobalObject, JSC::PropertyName, const JSC::PropertyDescriptor&, bool)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    throwSecurityError(*lexicalGlobalObject, scope, String());
    return false;
}

JSValue JSRemoteDOMWindow::self(JSC::JSGlobalObject&) const
{
    return globalThis();
}

JSValue JSRemoteDOMWindow::window(JSC::JSGlobalObject&) const
{
    return globalThis();
}

JSValue JSRemoteDOMWindow::frames(JSC::JSGlobalObject&) const
{
    return globalThis();
}

JSValue JSRemoteDOMWindow::getPrototype(JSObject*, JSGlobalObject*)
{
    return jsNull();
}

bool JSRemoteDOMWindow::preventExtensions(JSObject*, JSGlobalObject*)
{
    return false;
}

void JSRemoteDOMWindow::setOpener(JSC::JSGlobalObject&, JSC::JSValue)
{
    // FIXME: <rdar://118263373> Implement, probably like JSLocalDOMWindow::setOpener.
}

} // namepace WebCore
