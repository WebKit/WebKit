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

/* Source for JSEventTargetPropertiesTable
@begin JSEventTargetPropertiesTable
onabort       WebCore::JSEventTargetProperties::OnAbort                DontDelete
onblur        WebCore::JSEventTargetProperties::OnBlur                 DontDelete
onchange      WebCore::JSEventTargetProperties::OnChange               DontDelete
onclick       WebCore::JSEventTargetProperties::OnClick                DontDelete
oncontextmenu WebCore::JSEventTargetProperties::OnContextMenu          DontDelete
ondblclick    WebCore::JSEventTargetProperties::OnDblClick             DontDelete
onbeforecut   WebCore::JSEventTargetProperties::OnBeforeCut            DontDelete
oncut         WebCore::JSEventTargetProperties::OnCut                  DontDelete
onbeforecopy  WebCore::JSEventTargetProperties::OnBeforeCopy           DontDelete
oncopy        WebCore::JSEventTargetProperties::OnCopy                 DontDelete
onbeforepaste WebCore::JSEventTargetProperties::OnBeforePaste          DontDelete
onpaste       WebCore::JSEventTargetProperties::OnPaste                DontDelete
ondrag        WebCore::JSEventTargetProperties::OnDrag                 DontDelete
ondragend     WebCore::JSEventTargetProperties::OnDragEnd              DontDelete
ondragenter   WebCore::JSEventTargetProperties::OnDragEnter            DontDelete
ondragleave   WebCore::JSEventTargetProperties::OnDragLeave            DontDelete
ondragover    WebCore::JSEventTargetProperties::OnDragOver             DontDelete
ondragstart   WebCore::JSEventTargetProperties::OnDragStart            DontDelete
ondrop        WebCore::JSEventTargetProperties::OnDrop                 DontDelete
onerror       WebCore::JSEventTargetProperties::OnError                DontDelete
onfocus       WebCore::JSEventTargetProperties::OnFocus                DontDelete
oninput       WebCore::JSEventTargetProperties::OnInput                DontDelete
onkeydown     WebCore::JSEventTargetProperties::OnKeyDown              DontDelete
onkeypress    WebCore::JSEventTargetProperties::OnKeyPress             DontDelete
onkeyup       WebCore::JSEventTargetProperties::OnKeyUp                DontDelete
onload        WebCore::JSEventTargetProperties::OnLoad                 DontDelete
onmousedown   WebCore::JSEventTargetProperties::OnMouseDown            DontDelete
onmousemove   WebCore::JSEventTargetProperties::OnMouseMove            DontDelete
onmouseout    WebCore::JSEventTargetProperties::OnMouseOut             DontDelete
onmouseover   WebCore::JSEventTargetProperties::OnMouseOver            DontDelete
onmouseup     WebCore::JSEventTargetProperties::OnMouseUp              DontDelete
onmousewheel  WebCore::JSEventTargetProperties::OnMouseWheel           DontDelete
onreset       WebCore::JSEventTargetProperties::OnReset                DontDelete
onresize      WebCore::JSEventTargetProperties::OnResize               DontDelete
onscroll      WebCore::JSEventTargetProperties::OnScroll               DontDelete
onsearch      WebCore::JSEventTargetProperties::OnSearch               DontDelete
onselect      WebCore::JSEventTargetProperties::OnSelect               DontDelete
onselectstart WebCore::JSEventTargetProperties::OnSelectStart          DontDelete
onsubmit      WebCore::JSEventTargetProperties::OnSubmit               DontDelete
onunload      WebCore::JSEventTargetProperties::OnUnload               DontDelete
@end
*/

/*
@begin JSEventTargetPrototypeTable
addEventListener        WebCore::jsEventTargetAddEventListener    DontDelete|Function 3
removeEventListener     WebCore::jsEventTargetRemoveEventListener DontDelete|Function 3
dispatchEvent           WebCore::jsEventTargetDispatchEvent       DontDelete|Function 1
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

const AtomicString& eventNameForPropertyToken(int token)
{    
    switch (token) {
    case JSEventTargetProperties::OnAbort:
        return abortEvent;
    case JSEventTargetProperties::OnBlur:
        return blurEvent;
    case JSEventTargetProperties::OnChange:
        return changeEvent;
    case JSEventTargetProperties::OnClick:
        return clickEvent;
    case JSEventTargetProperties::OnContextMenu:
        return contextmenuEvent;
    case JSEventTargetProperties::OnDblClick:
        return dblclickEvent;
    case JSEventTargetProperties::OnError:
        return errorEvent;
    case JSEventTargetProperties::OnFocus:
        return focusEvent;
    case JSEventTargetProperties::OnInput:
        return inputEvent;
    case JSEventTargetProperties::OnKeyDown:
        return keydownEvent;
    case JSEventTargetProperties::OnKeyPress:
        return keypressEvent;
    case JSEventTargetProperties::OnKeyUp:
        return keyupEvent;
    case JSEventTargetProperties::OnLoad:
        return loadEvent;
    case JSEventTargetProperties::OnMouseDown:
        return mousedownEvent;
    case JSEventTargetProperties::OnMouseMove:
        return mousemoveEvent;
    case JSEventTargetProperties::OnMouseOut:
        return mouseoutEvent;
    case JSEventTargetProperties::OnMouseOver:
        return mouseoverEvent;
    case JSEventTargetProperties::OnMouseUp:
        return mouseupEvent;      
    case JSEventTargetProperties::OnMouseWheel:
        return mousewheelEvent;      
    case JSEventTargetProperties::OnBeforeCut:
        return beforecutEvent;
    case JSEventTargetProperties::OnCut:
        return cutEvent;
    case JSEventTargetProperties::OnBeforeCopy:
        return beforecopyEvent;
    case JSEventTargetProperties::OnCopy:
        return copyEvent;
    case JSEventTargetProperties::OnBeforePaste:
        return beforepasteEvent;
    case JSEventTargetProperties::OnPaste:
        return pasteEvent;
    case JSEventTargetProperties::OnDragEnter:
        return dragenterEvent;
    case JSEventTargetProperties::OnDragOver:
        return dragoverEvent;
    case JSEventTargetProperties::OnDragLeave:
        return dragleaveEvent;
    case JSEventTargetProperties::OnDrop:
        return dropEvent;
    case JSEventTargetProperties::OnDragStart:
        return dragstartEvent;
    case JSEventTargetProperties::OnDrag:
        return dragEvent;
    case JSEventTargetProperties::OnDragEnd:
        return dragendEvent;
    case JSEventTargetProperties::OnReset:
        return resetEvent;
    case JSEventTargetProperties::OnResize:
        return resizeEvent;
    case JSEventTargetProperties::OnScroll:
        return scrollEvent;
    case JSEventTargetProperties::OnSearch:
        return searchEvent;
    case JSEventTargetProperties::OnSelect:
        return selectEvent;
    case JSEventTargetProperties::OnSelectStart:
        return selectstartEvent;
    case JSEventTargetProperties::OnSubmit:
        return submitEvent;
    case JSEventTargetProperties::OnUnload:
        return unloadEvent;
    }

    return nullAtom;
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
