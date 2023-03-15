/*
 *  Copyright (C) 2008-2021 Apple Inc. All rights reseved.
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

#pragma once

#include "JSLocalDOMWindow.h"

namespace WebCore {

class DOMWindow;
class Frame;

JSLocalDOMWindow* asJSLocalDOMWindow(JSC::JSGlobalObject*);
const JSLocalDOMWindow* asJSLocalDOMWindow(const JSC::JSGlobalObject*);

enum class DOMWindowType : bool { Local, Remote };
template<DOMWindowType> bool jsLocalDOMWindowGetOwnPropertySlotRestrictedAccess(JSDOMGlobalObject*, DOMWindow&, JSC::JSGlobalObject&, JSC::PropertyName, JSC::PropertySlot&, const String&);

enum class CrossOriginObject : bool { Window, Location };
template<CrossOriginObject> void addCrossOriginOwnPropertyNames(JSC::JSGlobalObject&, JSC::PropertyNameArray&);

bool handleCommonCrossOriginProperties(JSC::JSObject* thisObject, JSC::VM&, JSC::PropertyName, JSC::PropertySlot&);

JSLocalDOMWindow& mainWorldGlobalObject(LocalFrame&);
JSLocalDOMWindow* mainWorldGlobalObject(LocalFrame*);

inline JSLocalDOMWindow* asJSLocalDOMWindow(JSC::JSGlobalObject* globalObject)
{
    return JSC::jsCast<JSLocalDOMWindow*>(globalObject);
}

inline const JSLocalDOMWindow* asJSLocalDOMWindow(const JSC::JSGlobalObject* globalObject)
{
    return static_cast<const JSLocalDOMWindow*>(globalObject);
}

inline JSLocalDOMWindow* mainWorldGlobalObject(LocalFrame* frame)
{
    return frame ? &mainWorldGlobalObject(*frame) : nullptr;
}

JSC_DECLARE_CUSTOM_GETTER(showModalDialogGetter);

} // namespace WebCore
