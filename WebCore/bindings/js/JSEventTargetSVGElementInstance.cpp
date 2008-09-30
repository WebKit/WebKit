/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#include "JSEventTargetSVGElementInstance.h"

#include "Document.h"
#include "EventNames.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"

using namespace JSC;

using namespace WebCore::EventNames;

ASSERT_CLASS_FITS_IN_CELL(WebCore::JSEventTargetSVGElementInstance)

static JSValue* jsEventTargetAddEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetRemoveEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* jsEventTargetDispatchEvent(ExecState*, JSObject*, JSValue*, const ArgList&);

static JSValue* jsEventTargetSVGElementInstanceOnAbort(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnAbort(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnBlur(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnBlur(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnChange(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnChange(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnClick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnClick(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnContextMenu(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnContextMenu(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDblClick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDblClick(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnBeforeCut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnBeforeCut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnCut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnCut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnBeforeCopy(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnBeforeCopy(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnCopy(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnCopy(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnBeforePaste(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnBeforePaste(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnPaste(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnPaste(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDrag(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDrag(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDragEnd(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDragEnd(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDragEnter(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDragEnter(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDragLeave(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDragLeave(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDragOver(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDragOver(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDragStart(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDragStart(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnDrop(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnDrop(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnError(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnError(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnFocus(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnFocus(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnInput(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnInput(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnKeyDown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnKeyDown(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnKeyPress(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnKeyPress(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnKeyUp(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnKeyUp(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnLoad(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnLoad(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseDown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseDown(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseMove(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseMove(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseOut(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseOut(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseOver(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseOver(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseUp(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseUp(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnMouseWheel(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnMouseWheel(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnReset(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnReset(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnResize(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnResize(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnScroll(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnScroll(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnSearch(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnSearch(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnSelect(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnSelect(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnSelectStart(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnSelectStart(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnSubmit(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnSubmit(ExecState*, JSObject*, JSValue*);
static JSValue* jsEventTargetSVGElementInstanceOnUnload(ExecState*, const Identifier&, const PropertySlot&);
static void setJSEventTargetSVGElementInstanceOnUnload(ExecState*, JSObject*, JSValue*);

/* Source for JSEventTargetSVGElementInstanceTable
@begin JSEventTargetSVGElementInstanceTable
onabort       jsEventTargetSVGElementInstanceOnAbort        DontDelete|DontEnum
onblur        jsEventTargetSVGElementInstanceOnBlur         DontDelete|DontEnum
onchange      jsEventTargetSVGElementInstanceOnChange       DontDelete|DontEnum
onclick       jsEventTargetSVGElementInstanceOnClick        DontDelete|DontEnum
oncontextmenu jsEventTargetSVGElementInstanceOnContextMenu  DontDelete|DontEnum
ondblclick    jsEventTargetSVGElementInstanceOnDblClick     DontDelete|DontEnum
onbeforecut   jsEventTargetSVGElementInstanceOnBeforeCut    DontDelete|DontEnum
oncut         jsEventTargetSVGElementInstanceOnCut          DontDelete|DontEnum
onbeforecopy  jsEventTargetSVGElementInstanceOnBeforeCopy   DontDelete|DontEnum
oncopy        jsEventTargetSVGElementInstanceOnCopy         DontDelete|DontEnum
onbeforepaste jsEventTargetSVGElementInstanceOnBeforePaste  DontDelete|DontEnum
onpaste       jsEventTargetSVGElementInstanceOnPaste        DontDelete|DontEnum
ondrag        jsEventTargetSVGElementInstanceOnDrag         DontDelete|DontEnum
ondragend     jsEventTargetSVGElementInstanceOnDragEnd      DontDelete|DontEnum
ondragenter   jsEventTargetSVGElementInstanceOnDragEnter    DontDelete|DontEnum
ondragleave   jsEventTargetSVGElementInstanceOnDragLeave    DontDelete|DontEnum
ondragover    jsEventTargetSVGElementInstanceOnDragOver     DontDelete|DontEnum
ondragstart   jsEventTargetSVGElementInstanceOnDragStart    DontDelete|DontEnum
ondrop        jsEventTargetSVGElementInstanceOnDrop         DontDelete|DontEnum
onerror       jsEventTargetSVGElementInstanceOnError        DontDelete|DontEnum
onfocus       jsEventTargetSVGElementInstanceOnFocus        DontDelete|DontEnum
oninput       jsEventTargetSVGElementInstanceOnInput        DontDelete|DontEnum
onkeydown     jsEventTargetSVGElementInstanceOnKeyDown      DontDelete|DontEnum
onkeypress    jsEventTargetSVGElementInstanceOnKeyPress     DontDelete|DontEnum
onkeyup       jsEventTargetSVGElementInstanceOnKeyUp        DontDelete|DontEnum
onload        jsEventTargetSVGElementInstanceOnLoad         DontDelete|DontEnum
onmousedown   jsEventTargetSVGElementInstanceOnMouseDown    DontDelete|DontEnum
onmousemove   jsEventTargetSVGElementInstanceOnMouseMove    DontDelete|DontEnum
onmouseout    jsEventTargetSVGElementInstanceOnMouseOut     DontDelete|DontEnum
onmouseover   jsEventTargetSVGElementInstanceOnMouseOver    DontDelete|DontEnum
onmouseup     jsEventTargetSVGElementInstanceOnMouseUp      DontDelete|DontEnum
onmousewheel  jsEventTargetSVGElementInstanceOnMouseWheel   DontDelete|DontEnum
onreset       jsEventTargetSVGElementInstanceOnReset        DontDelete|DontEnum
onresize      jsEventTargetSVGElementInstanceOnResize       DontDelete|DontEnum
onscroll      jsEventTargetSVGElementInstanceOnScroll       DontDelete|DontEnum
onsearch      jsEventTargetSVGElementInstanceOnSearch       DontDelete|DontEnum
onselect      jsEventTargetSVGElementInstanceOnSelect       DontDelete|DontEnum
onselectstart jsEventTargetSVGElementInstanceOnSelectStart  DontDelete|DontEnum
onsubmit      jsEventTargetSVGElementInstanceOnSubmit       DontDelete|DontEnum
onunload      jsEventTargetSVGElementInstanceOnUnload       DontDelete|DontEnum
@end
*/

/*
@begin JSEventTargetSVGElementInstancePrototypeTable
addEventListener    jsEventTargetAddEventListener       DontDelete|Function 3
removeEventListener jsEventTargetRemoveEventListener    DontDelete|Function 3
dispatchEvent       jsEventTargetDispatchEvent          DontDelete|Function 1
@end
*/

using namespace WebCore;

DECLARE_JS_EVENT_LISTENERS(EventTargetSVGElementInstance)

#include "JSEventTargetSVGElementInstance.lut.h"

namespace WebCore {

const ClassInfo JSEventTargetSVGElementInstancePrototype::s_info = { "EventTargetSVGElementInstancePrototype", 0, &JSEventTargetSVGElementInstancePrototypeTable, 0 };

JSObject* JSEventTargetSVGElementInstancePrototype::self(ExecState* exec)
{
    return getDOMPrototype<JSEventTargetSVGElementInstance>(exec);
}

bool JSEventTargetSVGElementInstancePrototype::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, &JSEventTargetSVGElementInstancePrototypeTable, this, propertyName, slot);
}

const ClassInfo JSEventTargetSVGElementInstance::s_info = { "EventTargetSVGElementInstance", &JSSVGElementInstance::s_info, &JSEventTargetSVGElementInstanceTable, 0 };

JSEventTargetSVGElementInstance::JSEventTargetSVGElementInstance(PassRefPtr<StructureID> structure, PassRefPtr<EventTargetSVGElementInstance> node)
    : JSSVGElementInstance(structure, node)
{
}

bool JSEventTargetSVGElementInstance::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSEventTargetSVGElementInstance, JSSVGElementInstance>(exec, &JSEventTargetSVGElementInstanceTable, this, propertyName, slot);
}

void JSEventTargetSVGElementInstance::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    lookupPut<JSEventTargetSVGElementInstance, JSSVGElementInstance>(exec, propertyName, value, &JSEventTargetSVGElementInstanceTable, this, slot);
}

JSObject* JSEventTargetSVGElementInstance::createPrototype(ExecState* exec)
{
    return new (exec) JSEventTargetSVGElementInstancePrototype(JSEventTargetSVGElementInstancePrototype::createStructureID(JSSVGElementInstancePrototype::self(exec)));
}

void JSEventTargetSVGElementInstance::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func) const
{
    SVGElement* element = impl()->correspondingElement();
    if (Frame* frame = element->document()->frame())
        element->setEventListenerForType(eventType, toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, func, true));
}

JSValue* JSEventTargetSVGElementInstance::getListener(const AtomicString& eventType) const
{
    SVGElement* element = impl()->correspondingElement();
    EventListener* listener = element->eventListenerForType(eventType);

    JSEventListener* jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();

    return jsNull();
}

EventTargetSVGElementInstance* toEventTargetSVGElementInstance(JSValue* val)
{
    if (!val || !val->isObject(&JSSVGElementInstance::s_info))
        return 0;

    return static_cast<EventTargetSVGElementInstance*>(static_cast<JSEventTargetSVGElementInstance*>(val)->impl());
}

} // namespace WebCore

using namespace WebCore;

JSValue* jsEventTargetAddEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSSVGElementInstance::s_info))
        return throwError(exec, TypeError);

    JSEventTargetSVGElementInstance* jsElementInstance = static_cast<JSEventTargetSVGElementInstance*>(thisValue);
    EventTargetSVGElementInstance* elementInstance = static_cast<EventTargetSVGElementInstance*>(jsElementInstance->impl());
    WebCore::Node* eventNode = elementInstance->correspondingElement();

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, args.at(exec, 1)))
        elementInstance->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetRemoveEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSSVGElementInstance::s_info))
        return throwError(exec, TypeError);

    JSEventTargetSVGElementInstance* jsElementInstance = static_cast<JSEventTargetSVGElementInstance*>(thisValue);
    EventTargetSVGElementInstance* elementInstance = static_cast<EventTargetSVGElementInstance*>(jsElementInstance->impl());
    WebCore::Node* eventNode = elementInstance->correspondingElement();

    Frame* frame = eventNode->document()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = toJSDOMWindow(frame)->findJSEventListener(args.at(exec, 1)))
        elementInstance->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

JSValue* jsEventTargetDispatchEvent(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (!thisValue->isObject(&JSSVGElementInstance::s_info))
        return throwError(exec, TypeError);

    JSEventTargetSVGElementInstance* jsElementInstance = static_cast<JSEventTargetSVGElementInstance*>(thisValue);
    EventTargetSVGElementInstance* elementInstance = static_cast<EventTargetSVGElementInstance*>(jsElementInstance->impl());

    ExceptionCode ec = 0;
    JSValue* result = jsBoolean(elementInstance->dispatchEvent(toEvent(args.at(exec, 0)), ec));
    setDOMException(exec, ec);
    return result;
}

#endif // ENABLE(SVG)
