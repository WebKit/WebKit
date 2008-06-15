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
#include "kjs_html.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "JSHTMLElement.h"
#include "ScriptController.h"
#include "HTMLPlugInElement.h"

#if USE(JAVASCRIPTCORE_BINDINGS)
#include "runtime.h"
#endif

using namespace KJS;

namespace WebCore {

using namespace HTMLNames;

// Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

static JSObject* getRuntimeObject(ExecState* exec, Node* node)
{
    if (!node)
        return 0;

#if USE(JAVASCRIPTCORE_BINDINGS)
    if (node->hasTagName(objectTag) || node->hasTagName(embedTag) || node->hasTagName(appletTag)) {
        HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(node);
        if (plugInElement->getInstance() && plugInElement->getInstance()->rootObject())
            // The instance is owned by the PlugIn element.
            return KJS::Bindings::Instance::createRuntimeObject(plugInElement->getInstance());
    }
#endif

    // If we don't have a runtime object return 0.
    return 0;
}

JSValue* runtimeObjectGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    return getRuntimeObject(exec, element);
}

JSValue* runtimeObjectPropertyGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    JSObject* runtimeObject = getRuntimeObject(exec, element);
    if (!runtimeObject)
        return jsUndefined();
    return runtimeObject->get(exec, propertyName);
}

bool runtimeObjectCustomGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, JSHTMLElement* originalObj, HTMLElement* thisImp)
{
    JSObject* runtimeObject = getRuntimeObject(exec, thisImp);
    if (!runtimeObject)
        return false;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    slot.setCustom(originalObj, runtimeObjectPropertyGetter);
    return true;
}

bool runtimeObjectCustomPut(ExecState* exec, const Identifier& propertyName, JSValue* value, HTMLElement* thisImp)
{
    JSObject* runtimeObject = getRuntimeObject(exec, thisImp);
    if (!runtimeObject)
        return 0;
    if (!runtimeObject->hasProperty(exec, propertyName))
        return false;
    runtimeObject->put(exec, propertyName, value);
    return true;
}

bool runtimeObjectImplementsCall(HTMLElement* thisImp)
{
    Frame* frame = thisImp->document()->frame();
    if (!frame)
        return false;
    ExecState* exec = frame->scriptProxy()->globalObject()->globalExec();
    JSObject* runtimeObject = getRuntimeObject(exec, thisImp);
    if (!runtimeObject)
        return false;
    return runtimeObject->implementsCall();
}

JSValue* runtimeObjectCallAsFunction(ExecState* exec, JSObject* thisObj, const List& args, HTMLElement* thisImp)
{
    JSObject* runtimeObject = getRuntimeObject(exec, thisImp);
    if (!runtimeObject)
        return jsUndefined();
    return runtimeObject->callAsFunction(exec, thisObj, args);
}

} // namespace WebCore
