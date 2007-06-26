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
#include "JSMouseEvent.h"
#include "JSMutationEvent.h"
#include "JSOverflowEvent.h"
#include "JSTextEvent.h"
#include "JSUIEvent.h"
#include "JSWheelEvent.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "OverflowEvent.h"
#include "TextEvent.h"
#include "UIEvent.h"
#include "WheelEvent.h"
#include "kjs_events.h"

namespace WebCore {

KJS::JSValue* JSEvent::clipboardData(KJS::ExecState* exec) const
{
    return impl()->isClipboardEvent() ? toJS(exec, impl()->clipboardData()) : KJS::jsUndefined();
}

KJS::JSValue* toJS(KJS::ExecState* exec, Event* event)
{
    KJS::JSLock lock;

    if (!event)
        return KJS::jsNull();

    KJS::ScriptInterpreter* interp = static_cast<KJS::ScriptInterpreter*>(exec->dynamicInterpreter());

    KJS::DOMObject* ret = interp->getDOMObject(event);
    if (ret)
        return ret;

    if (event->isKeyboardEvent())
        ret = new JSKeyboardEvent(exec, static_cast<KeyboardEvent*>(event));
    else if (event->isTextEvent())
        ret = new JSTextEvent(exec, static_cast<TextEvent*>(event));
    else if (event->isMouseEvent())
        ret = new JSMouseEvent(exec, static_cast<MouseEvent*>(event));
    else if (event->isWheelEvent())
        ret = new JSWheelEvent(exec, static_cast<WheelEvent*>(event));
    else if (event->isUIEvent())
        ret = new JSUIEvent(exec, static_cast<UIEvent*>(event));
    else if (event->isMutationEvent())
        ret = new JSMutationEvent(exec, static_cast<MutationEvent*>(event));
    else if (event->isOverflowEvent())
        ret = new JSOverflowEvent(exec, static_cast<OverflowEvent*>(event));
    else
        ret = new JSEvent(exec, event);

    interp->putDOMObject(event, ret);
    return ret;
}

} // namespace WebCore
