/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
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
    class JSDOMWindowShell;

    class JSDOMWindowBasePrivate;

    class WEBCORE_EXPORT JSDOMWindowBase : public JSDOMGlobalObject {
        typedef JSDOMGlobalObject Base;
    protected:
        JSDOMWindowBase(JSC::VM&, JSC::Structure*, RefPtr<DOMWindow>&&, JSDOMWindowShell*);
        void finishCreation(JSC::VM&, JSDOMWindowShell*);

        static void destroy(JSCell*);

    public:
        void updateDocument();

        DOMWindow& wrapped() const { return *m_wrapped; }
        ScriptExecutionContext* scriptExecutionContext() const;

        // Called just before removing this window from the JSDOMWindowShell.
        void willRemoveFromWindowShell();

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
        static void queueTaskToEventLoop(const JSC::JSGlobalObject*, Ref<JSC::Microtask>&&);
        
        void printErrorMessage(const String&) const;

        JSDOMWindowShell* shell() const;

        static void fireFrameClearedWatchpointsForWindow(DOMWindow*);
        static void visitChildren(JSC::JSCell*, JSC::SlotVisitor&);

    protected:
        JSC::WatchpointSet m_windowCloseWatchpoints;

    private:
        static JSC::JSInternalPromise* moduleLoaderResolve(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue, JSC::JSValue);
        static JSC::JSInternalPromise* moduleLoaderFetch(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue);
        static JSC::JSValue moduleLoaderEvaluate(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSValue, JSC::JSValue, JSC::JSValue);
        static JSC::JSInternalPromise* moduleLoaderImportModule(JSC::JSGlobalObject*, JSC::ExecState*, JSC::JSModuleLoader*, JSC::JSString*, const JSC::SourceOrigin&);

        RefPtr<DOMWindow> m_wrapped;
        JSDOMWindowShell* m_shell;
    };

    // Returns a JSDOMWindow or jsNull()
    // JSDOMGlobalObject* is ignored, accessing a window in any context will
    // use that DOMWindow's prototype chain.
    WEBCORE_EXPORT JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, DOMWindow&);
    inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMWindow* window) { return window ? toJS(exec, globalObject, *window) : JSC::jsNull(); }
    JSC::JSValue toJS(JSC::ExecState*, DOMWindow&);
    inline JSC::JSValue toJS(JSC::ExecState* exec, DOMWindow* window) { return window ? toJS(exec, *window) : JSC::jsNull(); }

    // Returns JSDOMWindow or 0
    JSDOMWindow* toJSDOMWindow(Frame*, DOMWrapperWorld&);
    WEBCORE_EXPORT JSDOMWindow* toJSDOMWindow(JSC::VM&, JSC::JSValue);

    DOMWindow& callerDOMWindow(JSC::ExecState*);
    DOMWindow& activeDOMWindow(JSC::ExecState*);
    DOMWindow& firstDOMWindow(JSC::ExecState*);

} // namespace WebCore
