// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "JSHTMLElement.h"
#include "HTMLDocument.h"
#include "HTMLImageElement.h"
#include "kjs_proxy.h"

namespace WebCore {

using namespace KJS;

ImageConstructorImp::ImageConstructorImp(ExecState* exec, Document* doc)
    : DOMObject(exec->lexicalGlobalObject()->objectPrototype())
    , m_doc(doc)
{
}

JSObject* ImageConstructorImp::construct(ExecState*  exec, const List& list)
{
    bool widthSet = false;
    bool heightSet = false;
    int width = 0;
    int height = 0;

    if (list.size() > 0) {
        widthSet = true;
        JSValue* w = list.at(0);
        width = w->toInt32(exec);
    }

    if (list.size() > 1) {
        heightSet = true;
        JSValue* h = list.at(1);
        height = h->toInt32(exec);
    }

    // Calling toJS on the document causes the JS document wrapper to be
    // added to the window object. This is done to ensure that JSDocument::mark
    // will be called (which will cause the image element to be marked if necessary).
    // This is only a problem for elements created using the Image constructor since all
    // other elements are created through the document, using document.createElement for example.
    toJS(exec, m_doc.get());
    
    HTMLImageElement* image = new HTMLImageElement(m_doc.get());
    JSObject* result = static_cast<JSObject*>(toJS(exec, image));

    if (widthSet)
        image->setWidth(width);
    if (heightSet)
        image->setHeight(height);

    return result;
}

// -------------------------------------------------------------------------

// Runtime object support code for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

JSValue* runtimeObjectGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSHTMLElement* thisObj = static_cast<JSHTMLElement*>(slot.slotBase());
    HTMLElement* element = static_cast<HTMLElement*>(thisObj->impl());
    return getRuntimeObject(exec, element);
}

JSValue* runtimeObjectPropertyGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
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
    return runtimeObject->call(exec, thisObj, args);
}

} // namespace WebCore
