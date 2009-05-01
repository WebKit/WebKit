/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#define JS_EVENT_LISTENER_FOR_EACH_LISTENER(specificEventTarget, macro) \
    macro(specificEventTarget, OnAbort, abortEvent) \
    macro(specificEventTarget, OnBlur, blurEvent) \
    macro(specificEventTarget, OnChange, changeEvent) \
    macro(specificEventTarget, OnClick, clickEvent) \
    macro(specificEventTarget, OnContextMenu, contextmenuEvent) \
    macro(specificEventTarget, OnDblClick, dblclickEvent) \
    macro(specificEventTarget, OnError, errorEvent) \
    macro(specificEventTarget, OnFocus, focusEvent) \
    macro(specificEventTarget, OnInput, inputEvent) \
    macro(specificEventTarget, OnKeyDown, keydownEvent) \
    macro(specificEventTarget, OnKeyPress, keypressEvent) \
    macro(specificEventTarget, OnKeyUp, keyupEvent) \
    macro(specificEventTarget, OnLoad, loadEvent) \
    macro(specificEventTarget, OnMouseDown, mousedownEvent) \
    macro(specificEventTarget, OnMouseMove, mousemoveEvent) \
    macro(specificEventTarget, OnMouseOut, mouseoutEvent) \
    macro(specificEventTarget, OnMouseOver, mouseoverEvent) \
    macro(specificEventTarget, OnMouseUp, mouseupEvent) \
    macro(specificEventTarget, OnMouseWheel, mousewheelEvent) \
    macro(specificEventTarget, OnBeforeCut, beforecutEvent) \
    macro(specificEventTarget, OnCut, cutEvent) \
    macro(specificEventTarget, OnBeforeCopy, beforecopyEvent) \
    macro(specificEventTarget, OnCopy, copyEvent) \
    macro(specificEventTarget, OnBeforePaste, beforepasteEvent) \
    macro(specificEventTarget, OnPaste, pasteEvent) \
    macro(specificEventTarget, OnDragEnter, dragenterEvent) \
    macro(specificEventTarget, OnDragOver, dragoverEvent) \
    macro(specificEventTarget, OnDragLeave, dragleaveEvent) \
    macro(specificEventTarget, OnDrop, dropEvent) \
    macro(specificEventTarget, OnDragStart, dragstartEvent) \
    macro(specificEventTarget, OnDrag, dragEvent) \
    macro(specificEventTarget, OnDragEnd, dragendEvent) \
    macro(specificEventTarget, OnReset, resetEvent) \
    macro(specificEventTarget, OnResize, resizeEvent) \
    macro(specificEventTarget, OnScroll, scrollEvent) \
    macro(specificEventTarget, OnSearch, searchEvent) \
    macro(specificEventTarget, OnSelect, selectEvent) \
    macro(specificEventTarget, OnSelectStart, selectstartEvent) \
    macro(specificEventTarget, OnSubmit, submitEvent) \
    macro(specificEventTarget, OnUnload, unloadEvent) \

#define EVENT_LISTENER_GETTER(specificEventTarget, name, event) \
JSC::JSValue js##specificEventTarget##name(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot& slot) \
{ \
    return static_cast<JS##specificEventTarget*>(slot.slotBase())->getListener(event); \
} \

#define EVENT_LISTENER_SETTER(specificEventTarget, name, event) \
void setJS##specificEventTarget##name(JSC::ExecState* exec, JSC::JSObject* baseObject, JSC::JSValue value) \
{ \
    static_cast<JS##specificEventTarget*>(baseObject)->setListener(exec, event, value); \
} \

#define DECLARE_JS_EVENT_LISTENERS(specificEventTarget) \
    JS_EVENT_LISTENER_FOR_EACH_LISTENER(specificEventTarget, EVENT_LISTENER_GETTER) \
    JS_EVENT_LISTENER_FOR_EACH_LISTENER(specificEventTarget, EVENT_LISTENER_SETTER) \

#endif // JSEventTargetBase_h
