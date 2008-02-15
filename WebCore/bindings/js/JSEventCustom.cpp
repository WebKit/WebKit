/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSEvent.h"

#include "Clipboard.h"
#include "Event.h"
#include "JSKeyboardEvent.h"
#include "JSMessageEvent.h"
#include "JSMouseEvent.h"
#include "JSMutationEvent.h"
#include "JSOverflowEvent.h"
#include "JSProgressEvent.h"
#include "JSTextEvent.h"
#include "JSUIEvent.h"
#include "JSWheelEvent.h"
#include "KeyboardEvent.h"
#include "MessageEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "OverflowEvent.h"
#include "ProgressEvent.h"
#include "TextEvent.h"
#include "UIEvent.h"
#include "WheelEvent.h"
#include "kjs_events.h"

using namespace KJS;

namespace WebCore {

JSValue* JSEvent::clipboardData(ExecState* exec) const
{
    return impl()->isClipboardEvent() ? toJS(exec, impl()->clipboardData()) : jsUndefined();
}

JSValue* toJS(ExecState* exec, Event* event)
{
    JSLock lock;

    if (!event)
        return jsNull();


    DOMObject* ret = ScriptInterpreter::getDOMObject(event);
    if (ret)
        return ret;

    if (event->isKeyboardEvent())
        ret = new JSKeyboardEvent(JSKeyboardEventPrototype::self(exec), static_cast<KeyboardEvent*>(event));
    else if (event->isTextEvent())
        ret = new JSTextEvent(JSTextEventPrototype::self(exec), static_cast<TextEvent*>(event));
    else if (event->isMouseEvent())
        ret = new JSMouseEvent(JSMouseEventPrototype::self(exec), static_cast<MouseEvent*>(event));
    else if (event->isWheelEvent())
        ret = new JSWheelEvent(JSWheelEventPrototype::self(exec), static_cast<WheelEvent*>(event));
    else if (event->isUIEvent())
        ret = new JSUIEvent(JSUIEventPrototype::self(exec), static_cast<UIEvent*>(event));
    else if (event->isMutationEvent())
        ret = new JSMutationEvent(JSMutationEventPrototype::self(exec), static_cast<MutationEvent*>(event));
    else if (event->isOverflowEvent())
        ret = new JSOverflowEvent(JSOverflowEventPrototype::self(exec), static_cast<OverflowEvent*>(event));
#if ENABLE(CROSS_DOCUMENT_MESSAGING)
    else if (event->isMessageEvent())
        ret = new JSMessageEvent(JSMessageEventPrototype::self(exec), static_cast<MessageEvent*>(event));
#endif
    else if (event->isProgressEvent())
        ret = new JSProgressEvent(JSProgressEventPrototype::self(exec), static_cast<ProgressEvent*>(event));
    else
        ret = new JSEvent(JSEventPrototype::self(exec), event);

    ScriptInterpreter::putDOMObject(event, ret);
    return ret;
}

} // namespace WebCore
