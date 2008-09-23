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

#include "Event.h"
#include "EventNames.h"
#include "JSEvent.h"

// FIXME: The name clash between this and WebCore::JSEventTargetPrototypeTable is only
// OK because this is in the JSC namespace; and this shouldn't be in that namespace!
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

    template<class JSSpecificEventTarget>
    class JSEventTargetBase {
    public:
        JSC::JSValue* getValueProperty(const JSSpecificEventTarget* owner, JSC::ExecState* exec, int token) const
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                return owner->getListener(eventName);

            return JSC::jsUndefined();
        }

        void putValueProperty(const JSSpecificEventTarget* owner, JSC::ExecState* exec, int token, JSC::JSValue* value)
        {
            const AtomicString& eventName = eventNameForPropertyToken(token);
            if (!eventName.isEmpty())
                owner->setListener(exec, eventName, value);
        }

        template<class JSParent>
        bool getOwnPropertySlot(JSSpecificEventTarget* owner, JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
        {
            return JSC::getStaticValueSlot<JSSpecificEventTarget, JSParent>(exec, &JSC::JSEventTargetPropertiesTable, owner, propertyName, slot);
        }

        template<class JSParent>
        void put(JSSpecificEventTarget* owner, JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSValue* value, JSC::PutPropertySlot& slot)
        {
            JSC::lookupPut<JSSpecificEventTarget, JSParent>(exec, propertyName, value, &JSC::JSEventTargetPropertiesTable, owner, slot);
        }
    };

    // The idea here is that classes like JSEventTargetNode and JSEventTargetSVGElementInstance
    // can share a prototype with the only difference being the name.
    template<class JSSpecificEventTarget>
    class JSEventTargetBasePrototype : public JSC::JSObject {
    public:
        JSEventTargetBasePrototype(PassRefPtr<JSC::StructureID> structure)
            : JSC::JSObject(structure)
        {
        }

        static JSC::JSObject* self(JSC::ExecState* exec)
        {
            return getDOMPrototype<JSSpecificEventTarget>(exec);
        }

        virtual bool getOwnPropertySlot(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
        {
            return JSC::getStaticFunctionSlot<JSC::JSObject>(exec, &JSC::JSEventTargetPrototypeTable, this, propertyName, slot);
        }

        virtual const JSC::ClassInfo* classInfo() const
        {
            static const JSC::ClassInfo s_classInfo = { JSSpecificEventTarget::prototypeClassName(), 0, &JSC::JSEventTargetPrototypeTable, 0 };
            return &s_classInfo;
        }
    };

    JSC::JSValue* toJS(JSC::ExecState*, EventTarget*);

} // namespace WebCore

#endif // JSEventTargetBase_h
