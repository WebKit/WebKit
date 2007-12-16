/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef JSEventTargetBase_h
#define JSEventTargetBase_h

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "JSEvent.h"
#include "kjs_events.h"
#include "kjs_window.h"

namespace KJS {

    extern const struct HashTable JSEventTargetPropertiesTable;
    extern const struct HashTable JSEventTargetPrototypeTable;

}

namespace WebCore {

    using namespace EventNames;

    class AtomicString;
    class EventTarget;

    // Event target properties (shared across all JSEventTarget* classes)
    struct JSEventTargetProperties {
        enum {
            AddEventListener, RemoveEventListener, DispatchEvent,
            OnAbort, OnBlur, OnChange, OnClick, OnContextMenu, OnDblClick, OnError,
            OnDragEnter, OnDragOver, OnDragLeave, OnDrop, OnDragStart, OnDrag, OnDragEnd,
            OnBeforeCut, OnCut, OnBeforeCopy, OnCopy, OnBeforePaste, OnPaste, OnSelectStart,
            OnFocus, OnInput, OnKeyDown, OnKeyPress, OnKeyUp, OnLoad, OnMouseDown,
            OnMouseMove, OnMouseOut, OnMouseOver, OnMouseUp, OnMouseWheel, OnReset,
            OnResize, OnScroll, OnSearch, OnSelect, OnSubmit, OnUnload
        };
    };

    // Event target prototype sharing functionality
    template<int JSEventTargetPrototypeFunctionIdentifier>
    class JSEventTargetPrototypeFunctionBase : public KJS::InternalFunctionImp {
    public:
        JSEventTargetPrototypeFunctionBase(KJS::ExecState* exec, int len, const KJS::Identifier& name) 
            : KJS::InternalFunctionImp(static_cast<KJS::FunctionPrototype*>(exec->lexicalGlobalObject()->functionPrototype()), name) 
        { 
            put(exec, exec->propertyNames().length, KJS::jsNumber(len), KJS::DontDelete | KJS::ReadOnly | KJS::DontEnum); 
        }

        static KJS::InternalFunctionImp* create(KJS::ExecState*, int len, const KJS::Identifier& name);
        virtual KJS::JSValue* callAsFunction(KJS::ExecState*, KJS::JSObject*, const KJS::List&) = 0;
    };

    template<int JSEventTargetPrototypeFunctionIdentifier>
    class JSEventTargetPrototypeFunction : public JSEventTargetPrototypeFunctionBase<JSEventTargetPrototypeFunctionIdentifier> {
    public:
        JSEventTargetPrototypeFunction(KJS::ExecState* exec, int len, const KJS::Identifier& name) 
            : JSEventTargetPrototypeFunctionBase<JSEventTargetPrototypeFunctionIdentifier>(exec, len, name)
        {
            // This constructor is not meant to be called, the template spezializations need to implement it.
            ASSERT_NOT_REACHED();    
        }
    };

    // Helper function for the partial specializated template functions below 
    bool retrieveEventTargetAndCorrespondingNode(KJS::ExecState*, KJS::JSObject* thisObj, Node*&, EventTarget*&);

    template<>
    class JSEventTargetPrototypeFunction<JSEventTargetProperties::AddEventListener> : public JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::AddEventListener> {
    public:
        JSEventTargetPrototypeFunction<JSEventTargetProperties::AddEventListener>(KJS::ExecState* exec, int len, const KJS::Identifier& name) 
            : JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::AddEventListener>(exec, len, name)
        { 
        }

        virtual KJS::JSValue* callAsFunction(KJS::ExecState* exec, KJS::JSObject* thisObj, const KJS::List& args)
        {
            KJS::DOMExceptionTranslator exception(exec);

            Node* eventNode = 0;
            EventTarget* eventTarget = 0;
            if (!retrieveEventTargetAndCorrespondingNode(exec, thisObj, eventNode, eventTarget))
                return KJS::throwError(exec, KJS::TypeError);

            Frame* frame = eventNode->document()->frame();
            if (!frame)
                return KJS::jsUndefined();

            if (JSEventListener* listener = KJS::Window::retrieveWindow(frame)->findOrCreateJSEventListener(args[1]))
                eventTarget->addEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));

            return KJS::jsUndefined();
        }
    };

    template<>
    class JSEventTargetPrototypeFunction<JSEventTargetProperties::RemoveEventListener> : public JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::RemoveEventListener> {
    public:
        JSEventTargetPrototypeFunction<JSEventTargetProperties::RemoveEventListener>(KJS::ExecState* exec, int len, const KJS::Identifier& name) 
            : JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::RemoveEventListener>(exec, len, name)
        { 
        }

        virtual KJS::JSValue* callAsFunction(KJS::ExecState* exec, KJS::JSObject* thisObj, const KJS::List& args)
        {
            KJS::DOMExceptionTranslator exception(exec);

            Node* eventNode = 0;
            EventTarget* eventTarget = 0;
            if (!retrieveEventTargetAndCorrespondingNode(exec, thisObj, eventNode, eventTarget))
                return KJS::throwError(exec, KJS::TypeError);

            Frame* frame = eventNode->document()->frame();
            if (!frame)
                return KJS::jsUndefined();

            if (JSEventListener* listener = KJS::Window::retrieveWindow(frame)->findJSEventListener(args[1]))
                eventTarget->removeEventListener(args[0]->toString(exec), listener, args[2]->toBoolean(exec));

            return KJS::jsUndefined();
        }
    };
 
    template<>
    class JSEventTargetPrototypeFunction<JSEventTargetProperties::DispatchEvent> : public JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::DispatchEvent> {
    public:
        JSEventTargetPrototypeFunction<JSEventTargetProperties::DispatchEvent>(KJS::ExecState* exec, int len, const KJS::Identifier& name) 
            : JSEventTargetPrototypeFunctionBase<JSEventTargetProperties::DispatchEvent>(exec, len, name)
        { 
        }

        virtual KJS::JSValue* callAsFunction(KJS::ExecState* exec, KJS::JSObject* thisObj, const KJS::List& args)
        {
            Node* eventNode = 0;
            EventTarget* eventTarget = 0;
            if (!retrieveEventTargetAndCorrespondingNode(exec, thisObj, eventNode, eventTarget))
                return KJS::throwError(exec, KJS::TypeError);

            KJS::DOMExceptionTranslator exception(exec);
            return KJS::jsBoolean(eventTarget->dispatchEvent(toEvent(args[0]), exception));
        }
    };

    // This creation function relies upon above specializations
    template<int JSEventTargetPrototypeFunctionIdentifier>
    KJS::InternalFunctionImp* JSEventTargetPrototypeFunctionBase<JSEventTargetPrototypeFunctionIdentifier>::create(KJS::ExecState* exec, int len, const KJS::Identifier& name)
    {
        return new JSEventTargetPrototypeFunction<JSEventTargetPrototypeFunctionIdentifier>(exec, len, name); 
    }

    // Helper function for getValueProperty/putValueProperty
    AtomicString eventNameForPropertyToken(int token);

    template<class JSEventTarget>
    class JSEventTargetBase {
    public:
        JSEventTargetBase() { }

        KJS::JSValue* getValueProperty(const JSEventTarget* owner, KJS::ExecState* exec, int token) const
        {
            AtomicString eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                return owner->getListener(eventName);

            return KJS::jsUndefined();
        }

        void putValueProperty(const JSEventTarget* owner, KJS::ExecState* exec, int token, KJS::JSValue* value, int attr)
        {
            AtomicString eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                owner->setListener(exec, eventName, value);
        }

    private:
        friend class JSEventTargetNode;
        friend class JSEventTargetSVGElementInstance;

        template<class JSParent>
        bool getOwnPropertySlot(JSEventTarget* owner, KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
        {
            return KJS::getStaticValueSlot<JSEventTarget, JSParent>(exec, &KJS::JSEventTargetPropertiesTable, owner, propertyName, slot);
        }

        template<class JSParent>
        void put(JSEventTarget* owner, KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value, int attr)
        {
            KJS::lookupPut<JSEventTarget, JSParent>(exec, propertyName, value, attr, &KJS::JSEventTargetPropertiesTable, owner);
        }
    };

    // This class is a modified version of the code the KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE
    // and KJS_IMPLEMENT_PROTOTYPE macros produce - the idea is that classes like JSEventTargetNode
    // and JSEventTargetSVGElementInstance can share a single prototype just differing in the
    // naming "EventTargetNodePrototype" vs "EventTargetSVGElementInstancePrototype". Above mentioned
    // macros force the existance of several prototype tables for each of the classes - avoid that.
    template<class JSEventTargetPrototypeParent, class JSEventTargetPrototypeInformation>
    class JSEventTargetPrototype : public KJS::JSObject {
    public:
        JSEventTargetPrototype(KJS::ExecState* exec)
            : KJS::JSObject(JSEventTargetPrototypeParent::self(exec))
        {
        }

        static KJS::JSObject* self(KJS::ExecState* exec)
        {
            static KJS::Identifier* prototypeName = new KJS::Identifier(JSEventTargetPrototypeInformation::prototypeClassName());

            KJS::JSGlobalObject* globalObject = exec->lexicalGlobalObject();
            if (KJS::JSValue* objectValue = globalObject->getDirect(*prototypeName)) {
                ASSERT(objectValue->isObject());
                return static_cast<KJS::JSObject*>(objectValue);
            }

            KJS::JSObject* newObject = new JSEventTargetPrototype<JSEventTargetPrototypeParent, JSEventTargetPrototypeInformation>(exec);
            globalObject->put(exec, *prototypeName, newObject, KJS::Internal | KJS::DontEnum);
            return newObject;
        }

        bool getOwnPropertySlot(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
        {
            return KJS::getStaticFunctionSlot<KJS::JSObject>(exec, &KJS::JSEventTargetPrototypeTable, this, propertyName, slot);
        }

        virtual const KJS::ClassInfo* classInfo() const
        {
            static const KJS::ClassInfo s_classInfo = { JSEventTargetPrototypeInformation::prototypeClassName(), 0, &KJS::JSEventTargetPrototypeTable };
            return &s_classInfo;
        }
    };

} // namespace WebCore

#endif // JSEventTargetBase_h
