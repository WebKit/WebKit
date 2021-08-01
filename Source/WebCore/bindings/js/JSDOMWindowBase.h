/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reseved.
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

#include "DOMWindow.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWrapperCache.h"
#include <JavaScriptCore/AuxiliaryBarrierInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/Lookup.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <cstddef>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMWrapperWorld;
class Frame;
class FetchResponse;
class JSDOMWindow;
class JSDOMWindowBasePrivate;
class JSWindowProxy;

class WEBCORE_EXPORT JSDOMWindowBase : public JSDOMGlobalObject {
public:
    using Base = JSDOMGlobalObject;

    static void destroy(JSCell*);

    template<typename, JSC::SubspaceAccess>
    static void subspaceFor(JSC::VM&) { RELEASE_ASSERT_NOT_REACHED(); }

    void updateDocument();

    DOMWindow& wrapped() const { return *m_wrapped; }
    ScriptExecutionContext* scriptExecutionContext() const;

    // Called just before removing this window from the JSWindowProxy.
    void willRemoveFromWindowProxy();

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, 0, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }

    static const JSC::GlobalObjectMethodTable s_globalObjectMethodTable;

    static bool supportsRichSourceInfo(const JSC::JSGlobalObject*);
    static bool shouldInterruptScript(const JSC::JSGlobalObject*);
    static bool shouldInterruptScriptBeforeTimeout(const JSC::JSGlobalObject*);
    static JSC::RuntimeFlags javaScriptRuntimeFlags(const JSC::JSGlobalObject*);
    static void queueMicrotaskToEventLoop(JSC::JSGlobalObject&, Ref<JSC::Microtask>&&);
    static JSC::JSObject* currentScriptExecutionOwner(JSC::JSGlobalObject*);
    static JSC::ScriptExecutionStatus scriptExecutionStatus(JSC::JSGlobalObject*, JSC::JSObject*);

    void printErrorMessage(const String&) const;

    JSWindowProxy& proxy() const;

    static void fireFrameClearedWatchpointsForWindow(DOMWindow*);

    void setCurrentEvent(Event*);
    Event* currentEvent() const;

protected:
    JSDOMWindowBase(JSC::VM&, JSC::Structure*, RefPtr<DOMWindow>&&, JSWindowProxy*);
    void finishCreation(JSC::VM&, JSWindowProxy*);
    void initStaticGlobals(JSC::VM&);

    RefPtr<JSC::WatchpointSet> m_windowCloseWatchpoints;

private:
    using ResponseCallback = WTF::Function<void(const char*, size_t)>;

    RefPtr<DOMWindow> m_wrapped;
    RefPtr<Event> m_currentEvent;
};

WEBCORE_EXPORT JSC::JSValue toJS(JSC::JSGlobalObject*, DOMWindow&);
// The following return a JSWindowProxy or jsNull()
// JSDOMGlobalObject* is ignored, accessing a window in any context will use that DOMWindow's prototype chain.
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, DOMWindow& window) { return toJS(lexicalGlobalObject, window); }
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, DOMWindow* window) { return window ? toJS(lexicalGlobalObject, globalObject, *window) : JSC::jsNull(); }
inline JSC::JSValue toJS(JSC::JSGlobalObject* lexicalGlobalObject, DOMWindow* window) { return window ? toJS(lexicalGlobalObject, *window) : JSC::jsNull(); }

// The following return a JSDOMWindow or nullptr.
JSDOMWindow* toJSDOMWindow(Frame&, DOMWrapperWorld&);
inline JSDOMWindow* toJSDOMWindow(Frame* frame, DOMWrapperWorld& world) { return frame ? toJSDOMWindow(*frame, world) : nullptr; }

// DOMWindow associated with global object of the "most-recently-entered author function or script
// on the stack, or the author function or script that originally scheduled the currently-running callback."
// (<https://html.spec.whatwg.org/multipage/webappapis.html#concept-incumbent-everything>, 27 April 2017)
// FIXME: Make this work for an "author function or script that originally scheduled the currently-running callback."
// See <https://bugs.webkit.org/show_bug.cgi?id=163412>.
DOMWindow& incumbentDOMWindow(JSC::JSGlobalObject&, JSC::CallFrame&);
DOMWindow& incumbentDOMWindow(JSC::JSGlobalObject&);

DOMWindow& activeDOMWindow(JSC::JSGlobalObject&);
DOMWindow& firstDOMWindow(JSC::JSGlobalObject&);

// FIXME: This should probably be removed in favor of one of the other DOMWindow accessors. It is intended
//        to provide the document specfied as the 'responsible document' in the algorithm for document.open()
//        (https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-open-steps steps 4
//        and 23 and https://html.spec.whatwg.org/multipage/webappapis.html#responsible-document). It is only
//        used by JSDocument.
Document* responsibleDocument(JSC::VM&, JSC::CallFrame&);

} // namespace WebCore
