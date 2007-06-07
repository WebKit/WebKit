/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef JSEventTargetNode_h
#define JSEventTargetNode_h

#include "JSNode.h"

namespace WebCore {

    class EventTargetNode;
    class Node;

    class JSEventTargetNode : public JSNode {
    public:
        JSEventTargetNode(KJS::ExecState*, Node* n);

        void setListener(KJS::ExecState*, const AtomicString& eventType, KJS::JSValue* func) const;
        KJS::JSValue* getListener(const AtomicString& eventType) const;
        virtual void pushEventHandlerScope(KJS::ExecState*, KJS::ScopeChain&) const;

        bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual void put(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue* value, int attr);
        void putValueProperty(KJS::ExecState*, int token, KJS::JSValue* value, int attr);

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

    EventTargetNode* toEventTargetNode(KJS::JSValue*);

    KJS_DEFINE_PROTOTYPE_WITH_PROTOTYPE(JSEventTargetNodePrototype, JSNodePrototype)

} // namespace WebCore

#endif // JSEventTargetNode_h
