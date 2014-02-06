/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef JSPluginElementFunctions_h
#define JSPluginElementFunctions_h

#include "JSDOMBinding.h"

namespace JSC {
namespace Bindings {
class Instance;
}
}

namespace WebCore {

    class HTMLElement;
    class JSHTMLElement;

    // JavaScript access to plug-in-exported properties for JSHTMLAppletElement, JSHTMLEmbedElement and JSHTMLObjectElement.

    JSC::Bindings::Instance* pluginInstance(HTMLElement&);
    JSC::JSObject* pluginScriptObject(JSC::ExecState*, JSHTMLElement*);

    JSC::EncodedJSValue pluginElementPropertyGetter(JSC::ExecState*, JSC::JSObject*, JSC::EncodedJSValue, JSC::PropertyName);
    bool pluginElementCustomGetOwnPropertySlot(JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&, JSHTMLElement*);
    bool pluginElementCustomPut(JSC::ExecState*, JSC::PropertyName, JSC::JSValue, JSHTMLElement*, JSC::PutPropertySlot&);
    JSC::CallType pluginElementGetCallData(JSHTMLElement*, JSC::CallData&);

    template <class Type, class Base> bool pluginElementCustomGetOwnPropertySlot(JSC::ExecState* exec, JSC::PropertyName propertyName, JSC::PropertySlot& slot, Type* element)
    {
        if (!element->globalObject()->world().isNormal()) {
            if (JSC::getStaticValueSlot<Type, Base>(exec, *Type::info()->staticPropHashTable, element, propertyName, slot))
                return true;

            JSC::JSValue proto = element->prototype();
            if (proto.isObject() && JSC::jsCast<JSC::JSObject*>(asObject(proto))->hasProperty(exec, propertyName))
                return false;
        }
        
        return pluginElementCustomGetOwnPropertySlot(exec, propertyName, slot, element);
    }

} // namespace WebCore

#endif // JSPluginElementFunctions_h
