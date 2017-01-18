/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
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
#include "InjectedBundleNavigationAction.h"

#include "WebFrame.h"
#include <WebCore/EventHandler.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/UIEventWithKeyState.h>

using namespace WebCore;

namespace WebKit {

static const MouseEvent* mouseEventForNavigationAction(const NavigationAction& navigationAction)
{
    for (const Event* e = navigationAction.event(); e; e = e->underlyingEvent()) {
        if (e->isMouseEvent())
            return static_cast<const MouseEvent*>(e);
    }
    return 0;
}

static WebMouseEvent::Button mouseButtonForMouseEvent(const MouseEvent* mouseEvent)
{
    if (!mouseEvent)
        return WebMouseEvent::NoButton;

    if (!mouseEvent->buttonDown() || !mouseEvent->isTrusted())
        return WebMouseEvent::NoButton;

    return static_cast<WebMouseEvent::Button>(mouseEvent->button());
}

static WebMouseEvent::SyntheticClickType syntheticClickTypeForMouseEvent(const MouseEvent* mouseEvent)
{
    if (!mouseEvent)
        return WebMouseEvent::NoTap;
    
    if (!mouseEvent->buttonDown() || !mouseEvent->isTrusted())
        return WebMouseEvent::NoTap;
    
    return static_cast<WebMouseEvent::SyntheticClickType>(mouseEvent->syntheticClickType());
}

WebEvent::Modifiers InjectedBundleNavigationAction::modifiersForNavigationAction(const NavigationAction& navigationAction)
{
    uint32_t modifiers = 0;
    const UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(navigationAction.event()));
    if (keyStateEvent && keyStateEvent->isTrusted()) {
        if (keyStateEvent->shiftKey())
            modifiers |= WebEvent::ShiftKey;
        if (keyStateEvent->ctrlKey())
            modifiers |= WebEvent::ControlKey;
        if (keyStateEvent->altKey())
            modifiers |= WebEvent::AltKey;
        if (keyStateEvent->metaKey())
            modifiers |= WebEvent::MetaKey;
    }

    return static_cast<WebEvent::Modifiers>(modifiers);
}

WebMouseEvent::Button InjectedBundleNavigationAction::mouseButtonForNavigationAction(const NavigationAction& navigationAction)
{
    return mouseButtonForMouseEvent(mouseEventForNavigationAction(navigationAction));
}

WebMouseEvent::SyntheticClickType InjectedBundleNavigationAction::syntheticClickTypeForNavigationAction(const NavigationAction& navigationAction)
{
    return syntheticClickTypeForMouseEvent(mouseEventForNavigationAction(navigationAction));
}

Ref<InjectedBundleNavigationAction> InjectedBundleNavigationAction::create(WebFrame* frame, const NavigationAction& action, PassRefPtr<FormState> formState)
{
    return adoptRef(*new InjectedBundleNavigationAction(frame, action, formState));
}

InjectedBundleNavigationAction::InjectedBundleNavigationAction(WebFrame* frame, const NavigationAction& navigationAction, PassRefPtr<FormState> formState)
    : m_navigationType(navigationAction.type())
    , m_modifiers(modifiersForNavigationAction(navigationAction))
    , m_mouseButton(WebMouseEvent::NoButton)
    , m_downloadAttribute(navigationAction.downloadAttribute())
    , m_shouldOpenExternalURLs(navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllow || navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes)
    , m_shouldTryAppLinks(navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllow)
{
    if (const MouseEvent* mouseEvent = mouseEventForNavigationAction(navigationAction)) {
        m_hitTestResult = InjectedBundleHitTestResult::create(frame->coreFrame()->eventHandler().hitTestResultAtPoint(mouseEvent->absoluteLocation()));
        m_mouseButton   = mouseButtonForMouseEvent(mouseEvent);
        m_syntheticClickType = syntheticClickTypeForNavigationAction(navigationAction);
    }

    if (formState)
        m_formElement = InjectedBundleNodeHandle::getOrCreate(formState->form());
}

} // namespace WebKit
