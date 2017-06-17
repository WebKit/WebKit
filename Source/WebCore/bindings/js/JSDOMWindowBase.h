/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reseved.
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

#include "JSDOMBinding.h"
#include "JSDOMGlobalObject.h"
#include <wtf/Forward.h>

namespace WebCore {

class DOMWindow;
class Frame;
class DOMWrapperWorld;
class JSDOMWindow;
class JSDOMWindowProxy;

class JSDOMWindowBasePrivate;

class WEBCORE_EXPORT JSDOMWindowBase : public JSDOMGlobalObject {
    typedef JSDOMGlobalObject Base;
protected:
    JSDOMWindowBase(JSC::VM&, JSC::Structure*, RefPtr<DOMWindow>&&, JSDOMWindowProxy*);
    void finishCreation(JSC::VM&, JSDOMWindowProxy*);

    static void destroy(JSCell*);

public:
    void updateDocument();

    DOMWindow& wrapped() const { return *m_wrapped; }
    ScriptExecutionContext* scriptExecutionContext() const;

    // Called just before removing this window from the JSDOMWindowProxy.
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
    static void queueTaskToEventLoop(JSC::JSGlobalObject&, Ref<JSC::Microtask>&&);

    void printErrorMessage(const String&) const;

    JSDOMWindowProxy* proxy() const;

    static void fireFrameClearedWatchpointsForWindow(DOMWindow*);

protected:
    JSC::WatchpointSet m_windowCloseWatchpoints;

private:
    static JSC::JSInternalPromise* moduleLoaderResolve(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue, JSC::JSValue);
    static JSC::JSInternalPromise* moduleLoaderFetch(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue);
    static JSC::JSValue moduleLoaderEvaluate(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue, JSC::JSValue);
    static JSC::JSInternalPromise* moduleLoaderImportModule(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSString*, const JSC::SourceOrigin&);
    static void promiseRejectionTracker(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSPromise*, JSC::JSPromiseRejectionOperation);

    RefPtr<DOMWindow> m_wrapped;
    JSDOMWindowProxy* m_proxy;
};

// The following return a JSDOMWindowProxy or jsNull()
// JSDOMGlobalObject* is ignored, accessing a window in any context will use that DOMWindow's prototype chain.
WEBCORE_EXPORT JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, DOMWindow&);
inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, DOMWindow* window) { return window ? toJS(state, globalObject, *window) : JSC::jsNull(); }
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, Frame&);
inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, Frame* frame) { return frame ? toJS(state, globalObject, *frame) : JSC::jsNull(); }
JSC::JSValue toJS(JSC::ExecState*, DOMWindow&);
inline JSC::JSValue toJS(JSC::ExecState* state, DOMWindow* window) { return window ? toJS(state, *window) : JSC::jsNull(); }

// The following return a JSDOMWindow or nullptr.
JSDOMWindow* toJSDOMWindow(Frame&, DOMWrapperWorld&);
inline JSDOMWindow* toJSDOMWindow(Frame* frame, DOMWrapperWorld& world) { return frame ? toJSDOMWindow(*frame, world) : nullptr; }
WEBCORE_EXPORT JSDOMWindow* toJSDOMWindow(JSC::VM&, JSC::JSValue);

// DOMWindow associated with global object of the "most-recently-entered author function or script
// on the stack, or the author function or script that originally scheduled the currently-running callback."
// (<https://html.spec.whatwg.org/multipage/webappapis.html#concept-incumbent-everything>, 27 April 2017)
// FIXME: Make this work for an "author function or script that originally scheduled the currently-running callback."
// See <https://bugs.webkit.org/show_bug.cgi?id=163412>.
DOMWindow& incumbentDOMWindow(JSC::ExecState&);

DOMWindow& activeDOMWindow(JSC::ExecState&);
DOMWindow& firstDOMWindow(JSC::ExecState&);

// FIXME: This should probably be removed in favor of one of the other DOMWindow accessors. It is intended
//        to provide the document specfied as the 'responsible document' in the algorithm for document.open()
//        (https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-open-steps steps 4
//        and 23 and https://html.spec.whatwg.org/multipage/webappapis.html#responsible-document). It is only
//        used by JSDocument.
Document* responsibleDocument(JSC::ExecState&);

} // namespace WebCore
