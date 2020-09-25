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
#include "PluginViewBase.h"


namespace WebCore {
using namespace JSC;

using namespace Bindings;
using namespace HTMLNames;

// JavaScript access to plug-in-exported properties for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

static JSC_DECLARE_HOST_FUNCTION(callPlugin);

Instance* pluginInstance(HTMLElement& element)
{
    // The plugin element holds an owning reference, so we don't have to.
    if (!is<HTMLPlugInElement>(element))
        return nullptr;
    auto* instance = downcast<HTMLPlugInElement>(element).bindingsInstance();
    if (!instance || !instance->rootObject())
        return nullptr;
    return instance;
}

static JSObject* pluginScriptObjectFromPluginViewBase(HTMLPlugInElement& pluginElement, JSGlobalObject* globalObject)
{
    Widget* pluginWidget = pluginElement.pluginWidget();
    if (!is<PluginViewBase>(pluginWidget))
        return nullptr;

    return downcast<PluginViewBase>(*pluginWidget).scriptObject(globalObject);
}

static JSObject* pluginScriptObjectFromPluginViewBase(JSHTMLElement* jsHTMLElement)
{
    HTMLElement& element = jsHTMLElement->wrapped();
    if (!is<HTMLPlugInElement>(element))
        return nullptr;

    HTMLPlugInElement& pluginElement = downcast<HTMLPlugInElement>(element);
    return pluginScriptObjectFromPluginViewBase(pluginElement, jsHTMLElement->globalObject());
}

JSObject* pluginScriptObject(JSGlobalObject* lexicalGlobalObject, JSHTMLElement* jsHTMLElement)
{
    HTMLElement& element = jsHTMLElement->wrapped();
    if (!is<HTMLPlugInElement>(element))
        return nullptr;

    auto& pluginElement = downcast<HTMLPlugInElement>(element);

    // Choke point for script/plugin interaction; notify DOMTimer of the event.
    DOMTimer::scriptDidInteractWithPlugin(pluginElement);

    // First, see if the element has a plug-in replacement with a script.
    if (auto* scriptObject = pluginElement.scriptObjectForPluginReplacement())
        return scriptObject;
    
    // Next, see if we can ask the plug-in view for its script object.
    if (auto* scriptObject = pluginScriptObjectFromPluginViewBase(pluginElement, jsHTMLElement->globalObject()))
        return scriptObject;

    // Otherwise, fall back to getting the object from the instance.

    // The plugin element holds an owning reference, so we don't have to.
    auto* instance = pluginElement.bindingsInstance();
    if (!instance || !instance->rootObject())
        return nullptr;

    return instance->createRuntimeObject(lexicalGlobalObject);
}
    
static EncodedJSValue pluginElementPropertyGetter(JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSHTMLElement* thisObject = jsDynamicCast<JSHTMLElement*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(lexicalGlobalObject, scope);
    JSObject* scriptObject = pluginScriptObject(lexicalGlobalObject, thisObject);
    if (!scriptObject)
        return JSValue::encode(jsUndefined());
    
    return JSValue::encode(scriptObject->get(lexicalGlobalObject, propertyName));
}

bool pluginElementCustomGetOwnPropertySlot(JSHTMLElement* element, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    slot.setIsTaintedByOpaqueObject();

    if (!element->globalObject()->world().isNormal()) {
        JSC::JSValue proto = element->getPrototypeDirect(lexicalGlobalObject->vm());
        if (proto.isObject() && JSC::jsCast<JSC::JSObject*>(asObject(proto))->hasProperty(lexicalGlobalObject, propertyName))
            return false;
    }

    if (slot.isVMInquiry()) {
        slot.setValue(element, static_cast<unsigned>(JSC::PropertyAttribute::None), jsUndefined());
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
    putResult = scriptObject->methodTable(lexicalGlobalObject->vm())->put(scriptObject, lexicalGlobalObject, propertyName, value, slot);
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
    for (size_t i = 0; i < argumentCount; i++)
        argumentList.append(callFrame->argument(i));
    ASSERT(!argumentList.hasOverflowed());

    auto callData = getCallData(lexicalGlobalObject->vm(), scriptObject);
    ASSERT(callData.type == CallData::Type::Native);

    // Call the object.
    JSValue result = call(lexicalGlobalObject, scriptObject, callData, callFrame->thisValue(), argumentList);
    return JSValue::encode(result);
}

CallData pluginElementCustomGetCallData(JSHTMLElement* element)
{
    CallData callData;

    // First, ask the plug-in view base for its runtime object.
    if (JSObject* scriptObject = pluginScriptObjectFromPluginViewBase(element)) {
        VM& vm = scriptObject->vm();
        auto scriptObjectCallData = getCallData(vm, scriptObject);
        if (scriptObjectCallData.type != CallData::Type::None) {
            callData.type = CallData::Type::Native;
            callData.native.function = callPlugin;
        }
    } else {
        Instance* instance = pluginInstance(element->wrapped());
        if (instance && instance->supportsInvokeDefaultMethod()) {
            callData.type = CallData::Type::Native;
            callData.native.function = callPlugin;
        }
    }

    return callData;
}

} // namespace WebCore
