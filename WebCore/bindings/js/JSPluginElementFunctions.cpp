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
#include "runtime_object.h"

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

static RuntimeObject* getRuntimeObject(ExecState* exec, Node* node)
{
    Instance* instance = pluginInstance(node);
    if (!instance)
        return 0;
    return instance->createRuntimeObject(exec);
}

JSValue runtimeObjectPropertyGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(asObject(slotBase));
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    RuntimeObject* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return jsUndefined();
    return runtimeObject->get(exec, propertyName);
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* element)
{
    RuntimeObject* runtimeObject = getRuntimeObject(exec, element->impl());
    if (!runtimeObject)
        return false;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    return true;
}

bool runtimeObjectCustomGetOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor, JSHTMLElement* element)
{
    RuntimeObject* runtimeObject = getRuntimeObject(exec, element->impl());
    if (!runtimeObject)
        return false;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    PropertySlot slot;
    slot.setCustom(element, runtimeObjectPropertyGetter);
    // While we don't know what the plugin allows, we do know that we prevent
    // enumeration or deletion of properties, so we mark plugin properties
    // as DontEnum | DontDelete
    descriptor.setDescriptor(slot.getValue(exec, propertyName), DontEnum | DontDelete);
    return true;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValue value, HTMLElement* element, PutPropertySlot& slot)
{
    RuntimeObject* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return 0;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    runtimeObject->put(exec, propertyName, value, slot);
    return true;
}

static JSValue JSC_HOST_CALL callPlugin(ExecState* exec, JSObject* function, JSValue, const ArgList& args)
{
    Instance* instance = pluginInstance(static_cast<JSHTMLElement*>(function)->impl());
    instance->begin();
    JSValue result = instance->invokeDefaultMethod(exec, args);
    instance->end();
    return result;
}

CallType runtimeObjectGetCallData(HTMLElement* element, CallData& callData)
{
    Instance* instance = pluginInstance(element);
    if (!instance || !instance->supportsInvokeDefaultMethod())
        return CallTypeNone;
    callData.native.function = callPlugin;
    return CallTypeHost;
}

} // namespace WebCore
