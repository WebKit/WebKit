/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "Document.h"
#include "EventNames.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"

using namespace JSC;

using namespace WebCore::EventNames;

ASSERT_CLASS_FITS_IN_CELL(WebCore::JSEventTargetNode)

static JSValue* jsEventTargetNodeOnAbort(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnAbort(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnBlur(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnBlur(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnChange(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnChange(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnClick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnClick(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnContextMenu(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnContextMenu(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDblClick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDblClick(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnBeforeCut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnBeforeCut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnCut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnCut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnBeforeCopy(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnBeforeCopy(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnCopy(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnCopy(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnBeforePaste(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnBeforePaste(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnPaste(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnPaste(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDrag(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDrag(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDragEnd(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDragEnd(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDragEnter(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDragEnter(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDragLeave(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDragLeave(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDragOver(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDragOver(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDragStart(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDragStart(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnDrop(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnDrop(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnError(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnError(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnFocus(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnFocus(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnInput(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnInput(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnKeyDown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnKeyDown(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnKeyPress(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnKeyPress(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnKeyUp(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnKeyUp(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnLoad(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnLoad(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseDown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseDown(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseMove(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseMove(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseOut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseOut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseOver(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseOver(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseUp(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseUp(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnMouseWheel(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnMouseWheel(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnReset(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnReset(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnResize(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnResize(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnScroll(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnScroll(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnSearch(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnSearch(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnSelect(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnSelect(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnSelectStart(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnSelectStart(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnSubmit(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnSubmit(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetNodeOnUnload(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetNodeOnUnload(ExecState*, JSObject*, JSValue*);

/* Source for JSEventTargetNodeTable
@begin JSEventTargetNodeTable
onabort       jsEventTargetNodeOnAbort        DontDelete|DontEnum
onblur        jsEventTargetNodeOnBlur         DontDelete|DontEnum
onchange      jsEventTargetNodeOnChange       DontDelete|DontEnum
onclick       jsEventTargetNodeOnClick        DontDelete|DontEnum
oncontextmenu jsEventTargetNodeOnContextMenu  DontDelete|DontEnum
ondblclick    jsEventTargetNodeOnDblClick     DontDelete|DontEnum
onbeforecut   jsEventTargetNodeOnBeforeCut    DontDelete|DontEnum
oncut         jsEventTargetNodeOnCut          DontDelete|DontEnum
onbeforecopy  jsEventTargetNodeOnBeforeCopy   DontDelete|DontEnum
oncopy        jsEventTargetNodeOnCopy         DontDelete|DontEnum
onbeforepaste jsEventTargetNodeOnBeforePaste  DontDelete|DontEnum
onpaste       jsEventTargetNodeOnPaste        DontDelete|DontEnum
ondrag        jsEventTargetNodeOnDrag         DontDelete|DontEnum
ondragend     jsEventTargetNodeOnDragEnd      DontDelete|DontEnum
ondragenter   jsEventTargetNodeOnDragEnter    DontDelete|DontEnum
ondragleave   jsEventTargetNodeOnDragLeave    DontDelete|DontEnum
ondragover    jsEventTargetNodeOnDragOver     DontDelete|DontEnum
ondragstart   jsEventTargetNodeOnDragStart    DontDelete|DontEnum
ondrop        jsEventTargetNodeOnDrop         DontDelete|DontEnum
onerror       jsEventTargetNodeOnError        DontDelete|DontEnum
onfocus       jsEventTargetNodeOnFocus        DontDelete|DontEnum
oninput       jsEventTargetNodeOnInput        DontDelete|DontEnum
onkeydown     jsEventTargetNodeOnKeyDown      DontDelete|DontEnum
onkeypress    jsEventTargetNodeOnKeyPress     DontDelete|DontEnum
onkeyup       jsEventTargetNodeOnKeyUp        DontDelete|DontEnum
onload        jsEventTargetNodeOnLoad         DontDelete|DontEnum
onmousedown   jsEventTargetNodeOnMouseDown    DontDelete|DontEnum
onmousemove   jsEventTargetNodeOnMouseMove    DontDelete|DontEnum
onmouseout    jsEventTargetNodeOnMouseOut     DontDelete|DontEnum
onmouseover   jsEventTargetNodeOnMouseOver    DontDelete|DontEnum
onmouseup     jsEventTargetNodeOnMouseUp      DontDelete|DontEnum
onmousewheel  jsEventTargetNodeOnMouseWheel   DontDelete|DontEnum
onreset       jsEventTargetNodeOnReset        DontDelete|DontEnum
onresize      jsEventTargetNodeOnResize       DontDelete|DontEnum
onscroll      jsEventTargetNodeOnScroll       DontDelete|DontEnum
onsearch      jsEventTargetNodeOnSearch       DontDelete|DontEnum
onselect      jsEventTargetNodeOnSelect       DontDelete|DontEnum
onselectstart jsEventTargetNodeOnSelectStart  DontDelete|DontEnum
onsubmit      jsEventTargetNodeOnSubmit       DontDelete|DontEnum
onunload      jsEventTargetNodeOnUnload       DontDelete|DontEnum
@end
*/

using namespace WebCore;
DECLARE_JS_EVENT_LISTENERS(EventTargetNode)

#include "JSEventTargetNode.lut.h"

namespace WebCore {

const ClassInfo JSEventTargetNode::s_info = { "EventTargetNode", &JSNode::s_info, &JSEventTargetNodeTable, 0 };

JSEventTargetNode::JSEventTargetNode(PassRefPtr<StructureID> structure, PassRefPtr<EventTargetNode> node)
    : JSNode(structure, node)
{
}

bool JSEventTargetNode::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSEventTargetNode, JSNode>(exec, &JSEventTargetNodeTable, this, propertyName, slot);
}

void JSEventTargetNode::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    lookupPut<JSEventTargetNode, JSNode>(exec, propertyName, value, &JSEventTargetNodeTable, this, slot);
}

JSObject* JSEventTargetNode::createPrototype(ExecState* exec)
{
    return new (exec) JSEventTargetNodePrototype(JSEventTargetNodePrototype::createStructureID(JSNodePrototype::self(exec)));
}

void JSEventTargetNode::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func) const
{
    Frame* frame = impl()->document()->frame();
    if (frame)
        EventTargetNodeCast(impl())->setEventListenerForType(eventType, toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, func, true));
}

JSValue* JSEventTargetNode::getListener(const AtomicString& eventType) const
{
    EventListener* listener = EventTargetNodeCast(impl())->eventListenerForType(eventType);
    JSEventListener* jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();

    return jsNull();
}

void JSEventTargetNode::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

EventTargetNode* toEventTargetNode(JSValue* val)
{
    if (!val || !val->isObject(&JSEventTargetNode::s_info))
        return 0;

    return static_cast<EventTargetNode*>(static_cast<JSEventTargetNode*>(val)->impl());
}

} // namespace WebCore
