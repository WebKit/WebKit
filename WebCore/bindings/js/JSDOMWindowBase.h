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

#ifndef JSDOMWindowBase_h
#define JSDOMWindowBase_h

#include "PlatformString.h"
#include "JSDOMBinding.h"
#include <runtime/Protect.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

    class AtomicString;
    class DOMWindow;
    class Event;
    class Frame;
    class JSDOMWindow;
    class JSDOMWindowShell;
    class JSLocation;
    class JSEventListener;
    class ScheduledAction;
    class SecurityOrigin;

    class JSDOMWindowBasePrivate;

    // This is the only WebCore JS binding which does not inherit from DOMObject
    class JSDOMWindowBase : public JSDOMGlobalObject {
        typedef JSDOMGlobalObject Base;

        friend class ScheduledAction;
    protected:
        JSDOMWindowBase(PassRefPtr<JSC::Structure>, PassRefPtr<DOMWindow>, JSDOMWindowShell*);

    public:
        virtual ~JSDOMWindowBase();

        void updateDocument();

        DOMWindow* impl() const { return d()->impl.get(); }
        virtual ScriptExecutionContext* scriptExecutionContext() const;

        void disconnectFrame();

        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&);
        virtual void put(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSValue, JSC::PutPropertySlot&);

        void clear();

        // Set a place to put a dialog return value when the window is cleared.
        void setReturnValueSlot(JSC::JSValue* slot);

        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;

        virtual JSC::ExecState* globalExec();
        
        virtual bool supportsProfiling() const;

        virtual bool shouldInterruptScript() const;

        bool allowsAccessFrom(JSC::ExecState*) const;
        bool allowsAccessFromNoErrorMessage(JSC::ExecState*) const;
        bool allowsAccessFrom(JSC::ExecState*, String& message) const;

        void printErrorMessage(const String&) const;

        // Don't call this version of allowsAccessFrom -- it's a slightly incorrect implementation used only by WebScriptObject
        virtual bool allowsAccessFrom(const JSC::JSGlobalObject*) const;

        virtual JSC::JSObject* toThisObject(JSC::ExecState*) const;
        JSDOMWindowShell* shell() const;

        static JSC::JSGlobalData* commonJSGlobalData();

    private:
        struct JSDOMWindowBaseData : public JSDOMGlobalObjectData {
            JSDOMWindowBaseData(PassRefPtr<DOMWindow>, JSDOMWindowShell*);

            RefPtr<DOMWindow> impl;

            JSC::JSValue* returnValueSlot;
            JSDOMWindowShell* shell;
        };

        static JSC::JSValue childFrameGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
        static JSC::JSValue indexGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
        static JSC::JSValue namedItemGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);

        void clearHelperObjectProperties();

        bool allowsAccessFromPrivate(const JSC::JSGlobalObject*) const;
        String crossDomainAccessErrorMessage(const JSC::JSGlobalObject*) const;
        
        JSDOMWindowBaseData* d() const { return static_cast<JSDOMWindowBaseData*>(JSC::JSVariableObject::d); }
    };

    // Returns a JSDOMWindow or jsNull()
    JSC::JSValue toJS(JSC::ExecState*, DOMWindow*);

    // Returns JSDOMWindow or 0
    JSDOMWindow* toJSDOMWindow(Frame*);
    JSDOMWindow* toJSDOMWindow(JSC::JSValue);

} // namespace WebCore

#endif // JSDOMWindowBase_h
