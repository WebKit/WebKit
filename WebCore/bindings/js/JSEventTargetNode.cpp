/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSEventTargetNode.h"

#include "AtomicString.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "EventTargetNode.h"
#include "Frame.h"
#include "JSEvent.h"
#include "Node.h"
#include "kjs_events.h"
#include "kjs_window.h"

#include "JSEventTargetNode.lut.h"

namespace WebCore {

using namespace KJS;
using namespace EventNames;

/* Source for JSEventTargetNodeTable
@begin JSEventTargetNodeTable 50
onabort       WebCore::JSEventTargetNode::OnAbort                DontDelete
onblur        WebCore::JSEventTargetNode::OnBlur                 DontDelete
onchange      WebCore::JSEventTargetNode::OnChange               DontDelete
onclick       WebCore::JSEventTargetNode::OnClick                DontDelete
oncontextmenu WebCore::JSEventTargetNode::OnContextMenu          DontDelete
ondblclick    WebCore::JSEventTargetNode::OnDblClick             DontDelete
onbeforecut   WebCore::JSEventTargetNode::OnBeforeCut            DontDelete
oncut         WebCore::JSEventTargetNode::OnCut                  DontDelete
onbeforecopy  WebCore::JSEventTargetNode::OnBeforeCopy           DontDelete
oncopy        WebCore::JSEventTargetNode::OnCopy                 DontDelete
onbeforepaste WebCore::JSEventTargetNode::OnBeforePaste          DontDelete
onpaste       WebCore::JSEventTargetNode::OnPaste                DontDelete
ondrag        WebCore::JSEventTargetNode::OnDrag                 DontDelete
ondragend     WebCore::JSEventTargetNode::OnDragEnd              DontDelete
ondragenter   WebCore::JSEventTargetNode::OnDragEnter            DontDelete
ondragleave   WebCore::JSEventTargetNode::OnDragLeave            DontDelete
ondragover    WebCore::JSEventTargetNode::OnDragOver             DontDelete
ondragstart   WebCore::JSEventTargetNode::OnDragStart            DontDelete
ondrop        WebCore::JSEventTargetNode::OnDrop                 DontDelete
onerror       WebCore::JSEventTargetNode::OnError                DontDelete
onfocus       WebCore::JSEventTargetNode::OnFocus                DontDelete
oninput       WebCore::JSEventTargetNode::OnInput                DontDelete
onkeydown     WebCore::JSEventTargetNode::OnKeyDown              DontDelete
onkeypress    WebCore::JSEventTargetNode::OnKeyPress             DontDelete
onkeyup       WebCore::JSEventTargetNode::OnKeyUp                DontDelete
onload        WebCore::JSEventTargetNode::OnLoad                 DontDelete
onmousedown   WebCore::JSEventTargetNode::OnMouseDown            DontDelete
onmousemove   WebCore::JSEventTargetNode::OnMouseMove            DontDelete
onmouseout    WebCore::JSEventTargetNode::OnMouseOut             DontDelete
onmouseover   WebCore::JSEventTargetNode::OnMouseOver            DontDelete
onmouseup     WebCore::JSEventTargetNode::OnMouseUp              DontDelete
onmousewheel  WebCore::JSEventTargetNode::OnMouseWheel           DontDelete
onreset       WebCore::JSEventTargetNode::OnReset                DontDelete
onresize      WebCore::JSEventTargetNode::OnResize               DontDelete
onscroll      WebCore::JSEventTargetNode::OnScroll               DontDelete
onsearch      WebCore::JSEventTargetNode::OnSearch               DontDelete
onselect      WebCore::JSEventTargetNode::OnSelect               DontDelete
onselectstart WebCore::JSEventTargetNode::OnSelectStart          DontDelete
onsubmit      WebCore::JSEventTargetNode::OnSubmit               DontDelete
onunload      WebCore::JSEventTargetNode::OnUnload               DontDelete
@end
*/

JSEventTargetNode::JSEventTargetNode(ExecState* exec, Node* n)
    : JSNode(exec, n)
{
    setPrototype(JSEventTargetNodePrototype::self(exec));
}

bool JSEventTargetNode::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSEventTargetNode, JSNode>(exec, &JSEventTargetNodeTable, this, propertyName, slot);
}

JSValue* JSEventTargetNode::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
        case OnAbort:
            return getListener(abortEvent);
        case OnBlur:
            return getListener(blurEvent);
        case OnChange:
            return getListener(changeEvent);
        case OnClick:
            return getListener(clickEvent);
        case OnContextMenu:
            return getListener(contextmenuEvent);
        case OnDblClick:
            return getListener(dblclickEvent);
        case OnError:
            return getListener(errorEvent);
        case OnFocus:
            return getListener(focusEvent);
        case OnInput:
            return getListener(inputEvent);
        case OnKeyDown:
            return getListener(keydownEvent);
        case OnKeyPress:
            return getListener(keypressEvent);
        case OnKeyUp:
            return getListener(keyupEvent);
        case OnLoad:
            return getListener(loadEvent);
        case OnMouseDown:
            return getListener(mousedownEvent);
        case OnMouseMove:
            return getListener(mousemoveEvent);
        case OnMouseOut:
            return getListener(mouseoutEvent);
        case OnMouseOver:
            return getListener(mouseoverEvent);
        case OnMouseUp:
            return getListener(mouseupEvent);      
        case OnMouseWheel:
            return getListener(mousewheelEvent);      
        case OnBeforeCut:
            return getListener(beforecutEvent);
        case OnCut:
            return getListener(cutEvent);
        case OnBeforeCopy:
            return getListener(beforecopyEvent);
        case OnCopy:
            return getListener(copyEvent);
        case OnBeforePaste:
            return getListener(beforepasteEvent);
        case OnPaste:
            return getListener(pasteEvent);
        case OnDragEnter:
            return getListener(dragenterEvent);
        case OnDragOver:
            return getListener(dragoverEvent);
        case OnDragLeave:
            return getListener(dragleaveEvent);
        case OnDrop:
            return getListener(dropEvent);
        case OnDragStart:
            return getListener(dragstartEvent);
        case OnDrag:
            return getListener(dragEvent);
        case OnDragEnd:
            return getListener(dragendEvent);
        case OnReset:
            return getListener(resetEvent);
        case OnResize:
            return getListener(resizeEvent);
        case OnScroll:
            return getListener(scrollEvent);
        case OnSearch:
            return getListener(searchEvent);
        case OnSelect:
            return getListener(selectEvent);
        case OnSelectStart:
            return getListener(selectstartEvent);
        case OnSubmit:
            return getListener(submitEvent);
        case OnUnload:
            return getListener(unloadEvent);
    }

    return jsUndefined();
}

void JSEventTargetNode::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<JSEventTargetNode, JSNode>(exec, propertyName, value, attr, &JSEventTargetNodeTable, this);
}

void JSEventTargetNode::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
{
    switch (token) {
        case OnAbort:
            setListener(exec, abortEvent, value);
            break;
        case OnBlur:
            setListener(exec, blurEvent, value);
            break;
        case OnChange:
            setListener(exec, changeEvent, value);
            break;
        case OnClick:
            setListener(exec, clickEvent, value);
            break;
        case OnContextMenu:
            setListener(exec, contextmenuEvent, value);
            break;
        case OnDblClick:
            setListener(exec, dblclickEvent, value);
            break;
        case OnError:
            setListener(exec, errorEvent, value);
            break;
        case OnFocus:
            setListener(exec, focusEvent, value);
            break;
        case OnInput:
            setListener(exec, inputEvent, value);
            break;
        case OnKeyDown:
            setListener(exec, keydownEvent, value);
            break;
        case OnKeyPress:
            setListener(exec, keypressEvent, value);
            break;
        case OnKeyUp:
            setListener(exec, keyupEvent, value);
            break;
        case OnLoad:
            setListener(exec, loadEvent, value);
            break;
        case OnMouseDown:
            setListener(exec, mousedownEvent, value);
            break;
        case OnMouseMove:
            setListener(exec, mousemoveEvent, value);
            break;
        case OnMouseOut:
            setListener(exec, mouseoutEvent, value);
            break;
        case OnMouseOver:
            setListener(exec, mouseoverEvent, value);
            break;
        case OnMouseUp:
            setListener(exec, mouseupEvent, value);
            break;
        case OnMouseWheel:
            setListener(exec, mousewheelEvent, value);
            break;
        case OnBeforeCut:
            setListener(exec, beforecutEvent, value);
            break;
        case OnCut:
            setListener(exec, cutEvent, value);
            break;
        case OnBeforeCopy:
            setListener(exec, beforecopyEvent, value);
            break;
        case OnCopy:
            setListener(exec, copyEvent, value);
            break;
        case OnBeforePaste:
            setListener(exec, beforepasteEvent, value);
            break;
        case OnPaste:
            setListener(exec, pasteEvent, value);
            break;
        case OnDragEnter:
            setListener(exec, dragenterEvent, value);
            break;
        case OnDragOver:
            setListener(exec, dragoverEvent, value);
            break;
        case OnDragLeave:
            setListener(exec, dragleaveEvent, value);
            break;
        case OnDrop:
            setListener(exec, dropEvent, value);
            break;
        case OnDragStart:
            setListener(exec, dragstartEvent, value);
            break;
        case OnDrag:
            setListener(exec, dragEvent, value);
            break;
        case OnDragEnd:
            setListener(exec, dragendEvent, value);
            break;
        case OnReset:
            setListener(exec, resetEvent, value);
            break;
        case OnResize:
            setListener(exec, resizeEvent, value);
            break;
        case OnScroll:
            setListener(exec, scrollEvent, value);
            break;
        case OnSearch:
            setListener(exec, searchEvent, value);
            break;
        case OnSelect:
            setListener(exec, selectEvent, value);
            break;
        case OnSelectStart:
            setListener(exec, selectstartEvent, value);
            break;
        case OnSubmit:
            setListener(exec, submitEvent, value);
            break;
        case OnUnload:
            setListener(exec, unloadEvent, value);
            break;
    }
}

void JSEventTargetNode::setListener(ExecState* exec, const AtomicString &eventType, JSValue* func) const
{
    Frame* frame = impl()->document()->frame();
    if (frame)
        EventTargetNodeCast(impl())->setHTMLEventListener(eventType, KJS::Window::retrieveWindow(frame)->findOrCreateJSEventListener(func, true));
}

JSValue* JSEventTargetNode::getListener(const AtomicString& eventType) const
{
    EventListener* listener = EventTargetNodeCast(impl())->getHTMLEventListener(eventType);
    JSEventListener* jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();

    return jsNull();
}

void JSEventTargetNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

/*
@begin JSEventTargetNodePrototypeTable 5
# from the EventTarget interface
addEventListener        WebCore::JSEventTargetNode::AddEventListener   DontDelete|Function 3
removeEventListener     WebCore::JSEventTargetNode::RemoveEventListener    DontDelete|Function 3
dispatchEvent           WebCore::JSEventTargetNode::DispatchEvent  DontDelete|Function 1
@end
*/

KJS_IMPLEMENT_PROTOTYPE_FUNCTION(JSEventTargetNodePrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("EventTargetNode", JSEventTargetNodePrototype, JSEventTargetNodePrototypeFunction)

JSValue* JSEventTargetNodePrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSEventTargetNode::info))
        return throwError(exec, TypeError);
    DOMExceptionTranslator exception(exec);
    JSEventTargetNode* jsNode = static_cast<JSEventTargetNode*>(thisObj);
    EventTargetNode* node = static_cast<EventTargetNode*>(jsNode->impl());
    switch (id) {
        case JSEventTargetNode::AddEventListener: {
            Frame* frame = node->document()->frame();
            if (!frame)
                return jsUndefined();
            JSEventListener* listener = KJS::Window::retrieveWindow(frame)->findOrCreateJSEventListener(args[1]);
            if (listener)
                node->addEventListener(args[0]->toString(exec), listener,args[2]->toBoolean(exec));
            return jsUndefined();
        }
        case JSEventTargetNode::RemoveEventListener: {
            Frame* frame = node->document()->frame();
            if (!frame)
                return jsUndefined();
            JSEventListener* listener = KJS::Window::retrieveWindow(frame)->findJSEventListener(args[1]);
            if (listener) 
                node->removeEventListener(args[0]->toString(exec), listener,args[2]->toBoolean(exec));
            return jsUndefined();
        }
        case JSEventTargetNode::DispatchEvent:
            return jsBoolean(node->dispatchEvent(toEvent(args[0]), exception));
    }

    return jsUndefined();
}


EventTargetNode* toEventTargetNode(JSValue* val)
{
    if (!val || !val->isObject(&JSEventTargetNode::info))
        return 0;
    return static_cast<EventTargetNode*>(static_cast<JSEventTargetNode*>(val)->impl());
}

} // namespace WebCore
