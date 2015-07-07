/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

using namespace JSC;

namespace WebCore {

using namespace Bindings;
using namespace HTMLNames;

// JavaScript access to plug-in-exported properties for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

static inline bool isPluginElement(HTMLElement& element)
{
    return element.hasTagName(objectTag) || element.hasTagName(embedTag) || element.hasTagName(appletTag);
}

Instance* pluginInstance(HTMLElement& element)
{
    // The plugin element holds an owning reference, so we don't have to.
    if (!isPluginElement(element))
        return 0;
    Instance* instance = toHTMLPlugInElement(element).getInstance().get();
    if (!instance || !instance->rootObject())
        return 0;
    return instance;
}

static JSObject* pluginScriptObjectFromPluginViewBase(HTMLPlugInElement& pluginElement, JSGlobalObject* globalObject)
{
    Widget* pluginWidget = pluginElement.pluginWidget();
    if (!pluginWidget)
        return 0;
    
    if (!pluginWidget->isPluginViewBase())
        return 0;

    PluginViewBase* pluginViewBase = toPluginViewBase(pluginWidget);
    return pluginViewBase->scriptObject(globalObject);
}

static JSObject* pluginScriptObjectFromPluginViewBase(JSHTMLElement* jsHTMLElement)
{
    HTMLElement& element = jsHTMLElement->impl();
    if (!isPluginElement(element))
        return 0;

    HTMLPlugInElement& pluginElement = toHTMLPlugInElement(element);
    return pluginScriptObjectFromPluginViewBase(pluginElement, jsHTMLElement->globalObject());
}

JSObject* pluginScriptObject(ExecState* exec, JSHTMLElement* jsHTMLElement)
{
    HTMLElement& element = jsHTMLElement->impl();
    if (!isPluginElement(element))
        return 0;

    HTMLPlugInElement& pluginElement = toHTMLPlugInElement(element);

    // Choke point for script/plugin interaction; notify DOMTimer of the event.
    DOMTimer::scriptDidInteractWithPlugin(pluginElement);

    // First, see if the element has a plug-in replacement with a script.
    if (JSObject* scriptObject = pluginElement.scriptObjectForPluginReplacement())
        return scriptObject;
    
    // Next, see if we can ask the plug-in view for its script object.
    if (JSObject* scriptObject = pluginScriptObjectFromPluginViewBase(pluginElement, jsHTMLElement->globalObject()))
        return scriptObject;

    // Otherwise, fall back to getting the object from the instance.

    // The plugin element holds an owning reference, so we don't have to.
    Instance* instance = pluginElement.getInstance().get();
    if (!instance || !instance->rootObject())
        return 0;

    return instance->createRuntimeObject(exec);
}
    
EncodedJSValue pluginElementPropertyGetter(ExecState* exec, JSObject*, EncodedJSValue thisValue, PropertyName propertyName)
{

    JSHTMLElement* thisObject = jsDynamicCast<JSHTMLElement*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(exec);
    JSObject* scriptObject = pluginScriptObject(exec, thisObject);
    if (!scriptObject)
        return JSValue::encode(jsUndefined());
    
    return JSValue::encode(scriptObject->get(exec, propertyName));
}

bool pluginElementCustomGetOwnPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot, JSHTMLElement* element)
{
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return false;

    if (!scriptObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(element, DontDelete | DontEnum, pluginElementPropertyGetter);
    return true;
}

bool pluginElementCustomPut(ExecState* exec, PropertyName propertyName, JSValue value, JSHTMLElement* element, PutPropertySlot& slot)
{
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return 0;
    if (!scriptObject->hasProperty(exec, propertyName))
        return false;
    scriptObject->methodTable()->put(scriptObject, exec, propertyName, value, slot);
    return true;
}

static EncodedJSValue JSC_HOST_CALL callPlugin(ExecState* exec)
{
    JSHTMLElement* element = jsCast<JSHTMLElement*>(exec->callee());

    // Get the plug-in script object.
    JSObject* scriptObject = pluginScriptObject(exec, element);
    ASSERT(scriptObject);

    size_t argumentCount = exec->argumentCount();
    MarkedArgumentBuffer argumentList;
    for (size_t i = 0; i < argumentCount; i++)
        argumentList.append(exec->argument(i));

    CallData callData;
    CallType callType = getCallData(scriptObject, callData);
    ASSERT(callType == CallTypeHost);

    // Call the object.
    JSValue result = call(exec, scriptObject, callType, callData, exec->thisValue(), argumentList);
    return JSValue::encode(result);
}

CallType pluginElementGetCallData(JSHTMLElement* element, CallData& callData)
{
    // First, ask the plug-in view base for its runtime object.
    if (JSObject* scriptObject = pluginScriptObjectFromPluginViewBase(element)) {
        CallData scriptObjectCallData;
        
        if (scriptObject->methodTable()->getCallData(scriptObject, scriptObjectCallData) == CallTypeNone)
            return CallTypeNone;

        callData.native.function = callPlugin;
        return CallTypeHost;
    }
    
    Instance* instance = pluginInstance(element->impl());
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callPlugin;
    return CallTypeHost;
}

} // namespace WebCore
