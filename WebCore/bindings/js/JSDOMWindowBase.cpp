/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSDOMWindowBase.h"

#include "CString.h"
#include "Console.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "InspectorController.h"
#include "JSDOMWindowCustom.h"
#include "JSHTMLCollection.h"
#include "JSNode.h"
#include "Logging.h"
#include "Page.h"
#include "ScheduledAction.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

////////////////////// JSDOMWindowBase Object ////////////////////////

const ClassInfo JSDOMWindowBase::s_info = { "Window", 0, 0, 0 };

JSDOMWindowBase::JSDOMWindowBaseData::JSDOMWindowBaseData(PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : impl(window)
    , returnValueSlot(0)
    , shell(shell)
{
}

JSDOMWindowBase::JSDOMWindowBase(PassRefPtr<Structure> structure, PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : JSDOMGlobalObject(structure, new JSDOMWindowBaseData(window, shell), shell)
{
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(globalExec(), "document"), jsNull(), DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(globalExec(), "window"), d()->shell, DontDelete | ReadOnly)
    };
    
    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));
}

void JSDOMWindowBase::updateDocument()
{
    ASSERT(d()->impl->document());
    ExecState* exec = globalExec();
    symbolTablePutWithAttributes(Identifier(exec, "document"), toJS(exec, d()->impl->document()), DontDelete | ReadOnly);
}

JSDOMWindowBase::~JSDOMWindowBase()
{
}

ScriptExecutionContext* JSDOMWindowBase::scriptExecutionContext() const
{
    return d()->impl->document();
}

JSValue JSDOMWindowBase::childFrameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(AtomicString(propertyName))->domWindow());
}

JSValue JSDOMWindowBase::indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(slot.index())->domWindow());
}

JSValue JSDOMWindowBase::namedItemGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSDOMWindowBase* thisObj = static_cast<JSDOMWindowBase*>(asObject(slot.slotBase()));
    Document* doc = thisObj->impl()->frame()->document();
    ASSERT(thisObj->allowsAccessFrom(exec));
    ASSERT(doc);
    ASSERT(doc->isHTMLDocument());

    RefPtr<HTMLCollection> collection = doc->windowNamedItems(propertyName);
    if (collection->length() == 1)
        return toJS(exec, collection->firstItem());
    return toJS(exec, collection.get());
}

bool JSDOMWindowBase::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (impl()->frame()->tree()->child(propertyName)) {
        slot.setCustom(this, childFrameGetter);
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue proto = prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccessFrom(exec))
                slot.setUndefined();
            return true;
        }
    }

    // FIXME: Search the whole frame hierachy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    bool ok;
    unsigned i = propertyName.toArrayIndex(&ok);
    if (ok && i < impl()->frame()->tree()->childCount()) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }

    if (!allowsAccessFrom(exec)) {
        slot.setUndefined();
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = impl()->frame()->document();
    if (document->isHTMLDocument()) {
        AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
        if (atomicPropertyName && (static_cast<HTMLDocument*>(document)->hasNamedItem(atomicPropertyName) || document->hasElementWithId(atomicPropertyName))) {
            slot.setCustom(this, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

void JSDOMWindowBase::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (allowsAccessFrom(exec))
        Base::put(exec, propertyName, value, slot);
}

String JSDOMWindowBase::crossDomainAccessErrorMessage(const JSGlobalObject* other) const
{
    KURL originURL = asJSDOMWindow(other)->impl()->url();
    KURL targetURL = impl()->frame()->document()->url();
    if (originURL.isNull() || targetURL.isNull())
        return String();

    // FIXME: this error message should contain more specifics of why the same origin check has failed.
    return String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains, protocols and ports must match.\n",
        targetURL.string().utf8().data(), originURL.string().utf8().data());
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    Frame* frame = impl()->frame();
    if (!frame)
        return;

    Settings* settings = frame->settings();
    if (!settings)
        return;
    
    if (settings->privateBrowsingEnabled())
        return;

    impl()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, 1, String()); // FIXME: provide a real line number and source URL.
}

ExecState* JSDOMWindowBase::globalExec()
{
    // We need to make sure that any script execution happening in this
    // frame does not destroy it
    if (Frame *frame = impl()->frame())
        frame->keepAlive();
    return Base::globalExec();
}

bool JSDOMWindowBase::supportsProfiling() const
{
    Frame* frame = impl()->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    return page->inspectorController()->profilerEnabled();
#else
    return false;
#endif
}

bool JSDOMWindowBase::shouldInterruptScript() const
{
    ASSERT(impl()->frame());
    Page* page = impl()->frame()->page();

    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    if (!page)
        return true;

    return page->chrome()->shouldInterruptJavaScript();
}

void JSDOMWindowBase::clearHelperObjectProperties()
{
    setCurrentEvent(0);
}

void JSDOMWindowBase::clear()
{
    JSLock lock(false);

    if (d()->returnValueSlot && !*d()->returnValueSlot)
        *d()->returnValueSlot = getDirect(Identifier(globalExec(), "returnValue"));

    clearHelperObjectProperties();
}

JSObject* JSDOMWindowBase::toThisObject(ExecState*) const
{
    return shell();
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return d()->shell;
}

JSGlobalData* JSDOMWindowBase::commonJSGlobalData()
{
    static JSGlobalData* globalData;
    if (!globalData) {
        globalData = JSGlobalData::createLeaked().releaseRef();
        globalData->timeoutChecker.setTimeoutInterval(10000); // 10 seconds
    }

    return globalData;
}

void JSDOMWindowBase::setReturnValueSlot(JSValue* slot)
{
    d()->returnValueSlot = slot;
}

void JSDOMWindowBase::disconnectFrame()
{
}

JSValue toJS(ExecState*, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return frame->script()->windowShell();
}

JSDOMWindow* toJSDOMWindow(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->windowShell()->window();
}

JSDOMWindow* toJSDOMWindow(JSValue value)
{
    if (!value.isObject())
        return 0;
    const ClassInfo* classInfo = asObject(value)->classInfo();
    if (classInfo == &JSDOMWindow::s_info)
        return static_cast<JSDOMWindow*>(asObject(value));
    if (classInfo == &JSDOMWindowShell::s_info)
        return static_cast<JSDOMWindowShell*>(asObject(value))->window();
    return 0;
}

} // namespace WebCore
