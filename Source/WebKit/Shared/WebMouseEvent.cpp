/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebMouseEvent.h"

#include "WebEventConversion.h"
#include <WebCore/MouseEventTypes.h>
#include <WebCore/NavigationAction.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebMouseEvent);

Ref<WebMouseEvent> WebMouseEvent::create(WebEventData&& eventData, WebMouseEventData&& mouseData)
{
    return adoptRef(*new WebMouseEvent(WTF::move(eventData), WTF::move(mouseData)));
}

Ref<WebMouseEvent> WebMouseEvent::create(WebMouseEventInit&& init)
{
    return create(WTF::move(init.event), WTF::move(init.mouse));
}

Ref<WebMouseEvent> WebMouseEvent::copy() const
{
    Ref copy = create(WebEventData { eventData() }, WebMouseEventData { m_data });
    copy->setPredictedEvents(m_predictedEvents);
    return copy;
}

WebMouseEvent::WebMouseEvent(WebEventData&& eventData, WebMouseEventData&& mouseData)
    : WebEvent(WTF::move(eventData))
    , m_data(WTF::move(mouseData))
{
    ASSERT(isMouseEventType(type()));
}

bool WebMouseEvent::isMouseEventType(WebEventType type)
{
    return type == WebEventType::MouseDown || type == WebEventType::MouseUp || type == WebEventType::MouseMove || type == WebEventType::MouseForceUp || type == WebEventType::MouseForceDown || type == WebEventType::MouseForceChanged;
}

WebMouseEventButton NODELETE mouseButton(const WebCore::NavigationAction& navigationAction)
{
    auto& mouseEventData = navigationAction.mouseEventData();
    if (mouseEventData && mouseEventData->buttonDown && mouseEventData->isTrusted)
        return kit(mouseEventData->button);
    return WebMouseEventButton::None;
}

WebMouseEventSyntheticClickType syntheticClickType(const WebCore::NavigationAction& navigationAction)
{
    auto& mouseEventData = navigationAction.mouseEventData();
    if (mouseEventData && mouseEventData->buttonDown && mouseEventData->isTrusted)
        return static_cast<WebMouseEventSyntheticClickType>(mouseEventData->syntheticClickType);
    return WebMouseEventSyntheticClickType::NoTap;
}

WebCore::SyntheticClickType NODELETE coreSyntheticClickType(WebMouseEventSyntheticClickType type)
{
    switch (type) {
    case WebMouseEventSyntheticClickType::NoTap: return WebCore::SyntheticClickType::NoTap;
    case WebMouseEventSyntheticClickType::OneFingerTap: return WebCore::SyntheticClickType::OneFingerTap;
    case WebMouseEventSyntheticClickType::TwoFingerTap: return WebCore::SyntheticClickType::TwoFingerTap;
    }
    ASSERT_NOT_REACHED();
    return WebCore::SyntheticClickType::NoTap;
}

} // namespace WebKit
