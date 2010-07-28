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

#include "Bridge.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSHTMLElement.h"
#include "PluginViewBase.h"

using namespace JSC;

namespace WebCore {

using namespace Bindings;
using namespace HTMLNames;

// Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

Instance* pluginInstance(Node* node)
{
    if (!node)
        return 0;
    if (!(node->hasTagName(objectTag) || node->hasTagName(embedTag) || node->hasTagName(appletTag)))
        return 0;
    HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(node);
    // The plugin element holds an owning reference, so we don't have to.
    Instance* instance = plugInElement->getInstance().get();
    if (!instance || !instance->rootObject())
        return 0;
    return instance;
}

JSObject* pluginScriptObject(ExecState* exec, JSHTMLElement* jsHTMLElement)
{
    HTMLElement* element = jsHTMLElement->impl();
    if (!(element->hasTagName(objectTag) || element->hasTagName(embedTag) || element->hasTagName(appletTag)))
        return 0;

    HTMLPlugInElement* pluginElement = static_cast<HTMLPlugInElement*>(element);

    // First, see if we can ask the plug-in view for its script object.
    if (Widget* pluginWidget = pluginElement->pluginWidget()) {
        if (pluginWidget->isPluginViewBase()) {
            PluginViewBase* pluginViewBase = static_cast<PluginViewBase*>(pluginWidget);
            if (JSObject* scriptObject = pluginViewBase->scriptObject(exec, jsHTMLElement->globalObject()))
                return scriptObject;
        }
    }

    // Otherwise, fall back to getting the object from the instance.

    // The plugin element holds an owning reference, so we don't have to.
    Instance* instance = pluginElement->getInstance().get();
    if (!instance || !instance->rootObject())
        return 0;

    return instance->createRuntimeObject(exec);
}
    
JSValue runtimeObjectPropertyGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSHTMLElement* element = static_cast<JSHTMLElement*>(asObject(slotBase));
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return jsUndefined();
    
    return scriptObject->get(exec, propertyName);
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* element)
{
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return false;

    if (!scriptObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    return true;
}

bool runtimeObjectCustomGetOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor, JSHTMLElement* element)
{
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return false;
    if (!scriptObject->hasProperty(exec, propertyName))
        return false;
    PropertySlot slot;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    // While we don't know what the plugin allows, we do know that we prevent
    // enumeration or deletion of properties, so we mark plugin properties
    // as DontEnum | DontDelete
    descriptor.setDescriptor(slot.getValue(exec, propertyName), DontEnum | DontDelete);
    return true;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValue value, JSHTMLElement* element, PutPropertySlot& slot)
{
    JSObject* scriptObject = pluginScriptObject(exec, element);
    if (!scriptObject)
        return 0;
    if (!scriptObject->hasProperty(exec, propertyName))
        return false;
    scriptObject->put(exec, propertyName, value, slot);
    return true;
}

static EncodedJSValue JSC_HOST_CALL callPlugin(ExecState* exec)
{
    Instance* instance = pluginInstance(static_cast<JSHTMLElement*>(exec->callee())->impl());
    instance->begin();
    JSValue result = instance->invokeDefaultMethod(exec);
    instance->end();
    return JSValue::encode(result);
}

CallType runtimeObjectGetCallData(JSHTMLElement* element, CallData& callData)
{
    Instance* instance = pluginInstance(element->impl());
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callPlugin;
    return CallTypeHost;
}

} // namespace WebCore
