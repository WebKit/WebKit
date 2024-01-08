/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSPluginElementFunctions.h"

#include "BridgeJSC.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSHTMLElement.h"

namespace WebCore {
using namespace JSC;

using namespace Bindings;
using namespace HTMLNames;

// JavaScript access to plug-in-exported properties for JSHTMLEmbedElement and JSHTMLObjectElement.

static JSC_DECLARE_HOST_FUNCTION(callPlugin);
static JSC_DECLARE_CUSTOM_GETTER(pluginElementPropertyGetter);

Instance* pluginInstance(HTMLElement& element)
{
    // The plugin element holds an owning reference, so we don't have to.
    auto* pluginElement = dynamicDowncast<HTMLPlugInElement>(element);
    if (!pluginElement)
        return nullptr;
    auto* instance = pluginElement->bindingsInstance();
    if (!instance || !instance->rootObject())
        return nullptr;
    return instance;
}

JSObject* pluginScriptObject(JSGlobalObject* lexicalGlobalObject, JSHTMLElement* jsHTMLElement)
{
    auto* element = dynamicDowncast<HTMLPlugInElement>(jsHTMLElement->wrapped());
    if (!element)
        return nullptr;

    // Choke point for script/plugin interaction; notify DOMTimer of the event.
    DOMTimer::scriptDidInteractWithPlugin();

    // The plugin element holds an owning reference, so we don't have to.
    auto* instance = element->bindingsInstance();
    if (!instance || !instance->rootObject())
        return nullptr;

    return instance->createRuntimeObject(lexicalGlobalObject);
}

JSC_DEFINE_CUSTOM_GETTER(pluginElementPropertyGetter, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSHTMLElement* thisObject = jsDynamicCast<JSHTMLElement*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(lexicalGlobalObject, scope);
    JSObject* scriptObject = pluginScriptObject(lexicalGlobalObject, thisObject);
    if (!scriptObject)
        return JSValue::encode(jsUndefined());
    
    return JSValue::encode(scriptObject->get(lexicalGlobalObject, propertyName));
}

bool pluginElementCustomGetOwnPropertySlot(JSHTMLElement* element, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    slot.setIsTaintedByOpaqueObject();

    if (propertyName.uid() == vm.propertyNames->toPrimitiveSymbol.impl())
        return false;

    if (!element->globalObject()->world().isNormal()) {
        JSValue proto = element->getPrototypeDirect();
        if (proto.isObject() && JSC::jsCast<JSC::JSObject*>(asObject(proto))->hasProperty(lexicalGlobalObject, propertyName))
            return false;
    }

    if (slot.isVMInquiry()) {
        slot.setValue(element, enumToUnderlyingType(JSC::PropertyAttribute::None), jsUndefined());
        return false; // Can't execute stuff below because they can call back into JS.
    }

    JSObject* scriptObject = pluginScriptObject(lexicalGlobalObject, element);
    if (!scriptObject)
        return false;

    if (!scriptObject->hasProperty(lexicalGlobalObject, propertyName))
        return false;

    slot.setCustom(element, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::DontEnum, pluginElementPropertyGetter);
    return true;
}

bool pluginElementCustomPut(JSHTMLElement* element, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot, bool& putResult)
{
    JSObject* scriptObject = pluginScriptObject(lexicalGlobalObject, element);
    if (!scriptObject)
        return false;
    if (!scriptObject->hasProperty(lexicalGlobalObject, propertyName))
        return false;
    putResult = scriptObject->methodTable()->put(scriptObject, lexicalGlobalObject, propertyName, value, slot);
    return true;
}

JSC_DEFINE_HOST_FUNCTION(callPlugin, (JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame))
{
    JSHTMLElement* element = jsCast<JSHTMLElement*>(callFrame->jsCallee());

    // Get the plug-in script object.
    JSObject* scriptObject = pluginScriptObject(lexicalGlobalObject, element);
    ASSERT(scriptObject);

    size_t argumentCount = callFrame->argumentCount();
    MarkedArgumentBuffer argumentList;
    argumentList.ensureCapacity(argumentCount);
    for (size_t i = 0; i < argumentCount; i++)
        argumentList.append(callFrame->argument(i));
    ASSERT(!argumentList.hasOverflowed());

    auto callData = JSC::getCallData(scriptObject);
    ASSERT(callData.type == CallData::Type::Native);

    // Call the object.
    JSValue result = call(lexicalGlobalObject, scriptObject, callData, callFrame->thisValue(), argumentList);
    return JSValue::encode(result);
}

CallData pluginElementCustomGetCallData(JSHTMLElement* element)
{
    CallData callData;

    Instance* instance = pluginInstance(element->wrapped());
    if (instance && instance->supportsInvokeDefaultMethod()) {
        callData.type = CallData::Type::Native;
        callData.native.function = callPlugin;
        callData.native.isBoundFunction = false;
    }

    return callData;
}

} // namespace WebCore
