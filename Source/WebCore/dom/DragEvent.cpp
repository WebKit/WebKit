/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DragEvent.h"

#include "DataTransfer.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DragEvent);

Ref<DragEvent> DragEvent::create(const AtomString& eventType, DragEventInit&& init)
{
    return adoptRef(*new DragEvent(eventType, WTFMove(init)));
}

Ref<DragEvent> DragEvent::createForBindings()
{
    return adoptRef(*new DragEvent);
}

Ref<DragEvent> DragEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed, MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers, short button, unsigned short buttons,
    EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer* dataTransfer, IsSimulated isSimulated, IsTrusted isTrusted)
{
    return adoptRef(*new DragEvent(type, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail,
        screenLocation, windowLocation, movementDelta, modifiers, button, buttons, relatedTarget, force, syntheticClickType, dataTransfer, isSimulated, isTrusted));
}

DragEvent::DragEvent(const AtomString& eventType, DragEventInit&& init)
    : MouseEvent(eventType, init)
    , m_dataTransfer(WTFMove(init.dataTransfer))
{
}

DragEvent::DragEvent(const AtomString& eventType, CanBubble canBubble, IsCancelable isCancelable, IsComposed isComposed,
    MonotonicTime timestamp, RefPtr<WindowProxy>&& view, int detail,
    const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers, short button, unsigned short buttons,
    EventTarget* relatedTarget, double force, unsigned short syntheticClickType, DataTransfer* dataTransfer, IsSimulated isSimulated, IsTrusted isTrusted)
    : MouseEvent(eventType, canBubble, isCancelable, isComposed, timestamp, WTFMove(view), detail, screenLocation, windowLocation, movementDelta, modifiers, button, buttons, relatedTarget, force, syntheticClickType, isSimulated, isTrusted)
    , m_dataTransfer(dataTransfer)
{
}

DragEvent::DragEvent() = default;

EventInterface DragEvent::eventInterface() const
{
    return DragEventInterfaceType;
}

} // namespace WebCore
