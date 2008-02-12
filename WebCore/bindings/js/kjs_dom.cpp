/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
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
#include "kjs_dom.h"

#include "Document.h"
#include "EventTarget.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSAttr.h"
#include "JSNode.h"
#include "XMLHttpRequest.h"
#include "kjs_events.h"
#include "kjs_window.h"

#if ENABLE(SVG)
#include "JSSVGElementInstance.h"
#endif

#if USE(JAVASCRIPTCORE_BINDINGS)
#include <bindings/runtime_object.h>
#endif

namespace WebCore {

using namespace KJS;
using namespace HTMLNames;

Attr* toAttr(JSValue* val, bool& ok)
{
    if (!val || !val->isObject(&JSAttr::info)) {
        ok = false;
        return 0;
    }

    ok = true;
    return static_cast<Attr*>(static_cast<JSNode*>(val)->impl());
}

bool checkNodeSecurity(ExecState* exec, Node* node)
{
    return node && allowsAccessFromFrame(exec, node->document()->frame());
}

JSValue* toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG)
    // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return toJS(exec, instance);
#endif
    
    Node* node = target->toNode();
    if (node)
        return toJS(exec, node);

    if (XMLHttpRequest* xhr = target->toXMLHttpRequest())
        // XMLHttpRequest is always created via JS, so we don't need to use cacheDOMObject() here.
        return ScriptInterpreter::getDOMObject(xhr);

    // There are two kinds of EventTargets: EventTargetNode and XMLHttpRequest.
    // If SVG support is enabled, there is also SVGElementInstance.
    ASSERT(0);
    return jsNull();
}

JSValue* getRuntimeObject(ExecState* exec, Node* n)
{
    if (!n)
        return 0;

#if USE(JAVASCRIPTCORE_BINDINGS)
    if (n->hasTagName(objectTag) || n->hasTagName(embedTag) || n->hasTagName(appletTag)) {
        HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(n);
        if (plugInElement->getInstance() && plugInElement->getInstance()->rootObject())
            // The instance is owned by the PlugIn element.
            return KJS::Bindings::Instance::createRuntimeObject(plugInElement->getInstance());
    }
#endif

    // If we don't have a runtime object return 0.
    return 0;
}

} // namespace WebCore
