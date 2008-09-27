/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSEventTargetBase.h"

#include "Document.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSEventTargetNode.h"
#include "JSMessagePort.h"
#include "JSXMLHttpRequestUpload.h"
#include <kjs/Error.h>

#if ENABLE(SVG)
#include "EventTargetSVGElementInstance.h"
#include "JSEventTargetSVGElementInstance.h"
#endif

using namespace JSC;

namespace WebCore {

static JSValue* jsEventTargetAddEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetRemoveEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetDispatchEvent(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "JSEventTargetBase.lut.h"

namespace WebCore {

/*
@begin JSEventTargetPrototypeTable
addEventListener    jsEventTargetAddEventListener       DontDelete|Function 3
removeEventListener jsEventTargetRemoveEventListener    DontDelete|Function 3
dispatchEvent       jsEventTargetDispatchEvent          DontDelete|Function 1
@end
*/

static inline bool retrieveEventTargetAndCorrespondingNode(ExecState*, JSValue* thisValue, Node*& eventNode, EventTarget*& eventTarget)
{
    if (thisValue->isObject(&JSNode::s_info)) {
        JSEventTargetNode* jsNode = static_cast<JSEventTargetNode*>(thisValue);
        EventTargetNode* node = static_cast<EventTargetNode*>(jsNode->impl());
        ASSERT(node);

        eventNode = node;
        eventTarget = node;
        return true;
    }

#if ENABLE(SVG)
    if (thisValue->isObject(&JSSVGElementInstance::s_info)) {
        JSEventTargetSVGElementInstance* jsNode = static_cast<JSEventTargetSVGElementInstance*>(thisValue);
        EventTargetSVGElementInstance* node = static_cast<EventTargetSVGElementInstance*>(jsNode->impl());
        ASSERT(node);

        eventNode = node->correspondingElement();
        eventTarget = node;
        return true;
    }
#endif

    return false;
}

JSValue* jsEventTargetAddEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, args.at(exec, 1)))
        eventTarget->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetRemoveEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = toJSDOMWindow(frame)->findJSEventListener(args.at(exec, 1)))
        eventTarget->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetDispatchEvent(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    Node* eventNode = 0;
    EventTarget* eventTarget = 0;
    if (!retrieveEventTargetAndCorrespondingNode(exec, thisValue, eventNode, eventTarget))
        return throwError(exec, TypeError);

    ExceptionCode ec = 0;
    JSValue* result = jsBoolean(eventTarget->dispatchEvent(toEvent(args.at(exec, 0)), ec));
    setDOMException(exec, ec);
    return result;
}

JSValue* toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG)
    // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
    if (SVGElementInstance* instance = target->toSVGElementInstance())
        return toJS(exec, instance);
#endif
    
    if (Node* node = target->toNode())
        return toJS(exec, node);

    if (XMLHttpRequest* xhr = target->toXMLHttpRequest())
        // XMLHttpRequest is always created via JS, so we don't need to use cacheDOMObject() here.
        return getCachedDOMObjectWrapper(xhr);

    if (XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload())
        return toJS(exec, upload);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (DOMApplicationCache* cache = target->toDOMApplicationCache())
        // DOMApplicationCache is always created via JS, so we don't need to use cacheDOMObject() here.
        return getCachedDOMObjectWrapper(cache);
#endif

    if (MessagePort* messagePort = target->toMessagePort())
        return toJS(exec, messagePort);
    
    ASSERT_NOT_REACHED();
    return jsNull();
}

} // namespace WebCore
