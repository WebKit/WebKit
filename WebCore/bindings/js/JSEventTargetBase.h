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

namespace JSC {

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

    // Helper function for getValueProperty/putValueProperty
    const AtomicString& eventNameForPropertyToken(int token);

    template<class JSEventTarget>
    class JSEventTargetBase {
    public:
        JSEventTargetBase() { }

        JSC::JSValue* getValueProperty(const JSEventTarget* owner, JSC::ExecState* exec, int token) const
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                return owner->getListener(eventName);

            return JSC::jsUndefined();
        }

        void putValueProperty(const JSEventTarget* owner, JSC::ExecState* exec, int token, JSC::JSValue* value)
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                owner->setListener(exec, eventName, value);
        }

    private:
        friend class JSEventTargetNode;
        friend class JSEventTargetSVGElementInstance;

        template<class JSParent>
        bool getOwnPropertySlot(JSEventTarget* owner, JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
        {
            return JSC::getStaticValueSlot<JSEventTarget, JSParent>(exec, &JSC::JSEventTargetPropertiesTable, owner, propertyName, slot);
        }

        template<class JSParent>
        void put(JSEventTarget* owner, JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSValue* value, JSC::PutPropertySlot& slot)
        {
            JSC::lookupPut<JSEventTarget, JSParent>(exec, propertyName, value, &JSC::JSEventTargetPropertiesTable, owner, slot);
        }
    };

    // This class is a modified version of the code the JSC_DEFINE_PROTOTYPE_WITH_PROTOTYPE
    // and JSC_IMPLEMENT_PROTOTYPE macros produce - the idea is that classes like JSEventTargetNode
    // and JSEventTargetSVGElementInstance can share a single prototype just differing in the
    // naming "EventTargetNodePrototype" vs "EventTargetSVGElementInstancePrototype". Above mentioned
    // macros force the existance of several prototype tables for each of the classes - avoid that.
    template<class JSEventTargetPrototypeParent, class JSEventTargetPrototypeInformation>
    class JSEventTargetPrototype : public JSC::JSObject {
    public:
        JSEventTargetPrototype(JSC::ExecState* exec)
            : JSC::JSObject(JSEventTargetPrototypeParent::self(exec))
        {
        }

        static JSC::JSObject* self(JSC::ExecState* exec)
        {
            static JSC::Identifier* prototypeName = new JSC::Identifier(exec, JSEventTargetPrototypeInformation::prototypeClassName());

            JSC::JSGlobalObject* globalObject = exec->lexicalGlobalObject();
            if (JSC::JSValue* objectValue = globalObject->getDirect(*prototypeName)) {
                ASSERT(objectValue->isObject());
                return static_cast<JSC::JSObject*>(objectValue);
            }

            JSC::JSObject* newObject = new (exec) JSEventTargetPrototype<JSEventTargetPrototypeParent, JSEventTargetPrototypeInformation>(exec);
            globalObject->putDirect(*prototypeName, newObject, JSC::DontEnum);
            return newObject;
        }

        bool getOwnPropertySlot(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
        {
            return JSC::getStaticFunctionSlot<JSC::JSObject>(exec, &JSC::JSEventTargetPrototypeTable, this, propertyName, slot);
        }

        virtual const JSC::ClassInfo* classInfo() const
        {
            static const JSC::ClassInfo s_classInfo = { JSEventTargetPrototypeInformation::prototypeClassName(), 0, &JSC::JSEventTargetPrototypeTable, 0 };
            return &s_classInfo;
        }
    };

    JSC::JSValue* toJS(JSC::ExecState*, EventTarget*);

} // namespace WebCore

#endif // JSEventTargetBase_h
