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

namespace WebKit {
using namespace WebCore;

static WebMouseEvent::Button mouseButtonForMouseEventData(const Optional<NavigationAction::MouseEventData>& mouseEventData)
{
    if (mouseEventData && mouseEventData->buttonDown && mouseEventData->isTrusted)
        return static_cast<WebMouseEvent::Button>(mouseEventData->button);
    return WebMouseEvent::NoButton;
}

static WebMouseEvent::SyntheticClickType syntheticClickTypeForMouseEventData(const Optional<NavigationAction::MouseEventData>& mouseEventData)
{
    if (mouseEventData && mouseEventData->buttonDown && mouseEventData->isTrusted)
        return static_cast<WebMouseEvent::SyntheticClickType>(mouseEventData->syntheticClickType);
    return WebMouseEvent::NoTap;
}
    
static FloatPoint clickLocationInRootViewCoordinatesForMouseEventData(const Optional<NavigationAction::MouseEventData>& mouseEventData)
{
    if (mouseEventData && mouseEventData->buttonDown && mouseEventData->isTrusted)
        return mouseEventData->locationInRootViewCoordinates;
    return { };
}

OptionSet<WebEvent::Modifier> InjectedBundleNavigationAction::modifiersForNavigationAction(const NavigationAction& navigationAction)
{
    OptionSet<WebEvent::Modifier> modifiers;
    auto keyStateEventData = navigationAction.keyStateEventData();
    if (keyStateEventData && keyStateEventData->isTrusted) {
        if (keyStateEventData->shiftKey)
            modifiers.add(WebEvent::Modifier::ShiftKey);
        if (keyStateEventData->ctrlKey)
            modifiers.add(WebEvent::Modifier::ControlKey);
        if (keyStateEventData->altKey)
            modifiers.add(WebEvent::Modifier::AltKey);
        if (keyStateEventData->metaKey)
            modifiers.add(WebEvent::Modifier::MetaKey);
    }
    return modifiers;
}

WebMouseEvent::Button InjectedBundleNavigationAction::mouseButtonForNavigationAction(const NavigationAction& navigationAction)
{
    return mouseButtonForMouseEventData(navigationAction.mouseEventData());
}

WebMouseEvent::SyntheticClickType InjectedBundleNavigationAction::syntheticClickTypeForNavigationAction(const NavigationAction& navigationAction)
{
    return syntheticClickTypeForMouseEventData(navigationAction.mouseEventData());
}
    
FloatPoint InjectedBundleNavigationAction::clickLocationInRootViewCoordinatesForNavigationAction(const NavigationAction& navigationAction)
{
    return clickLocationInRootViewCoordinatesForMouseEventData(navigationAction.mouseEventData());
}

Ref<InjectedBundleNavigationAction> InjectedBundleNavigationAction::create(WebFrame* frame, const NavigationAction& action, RefPtr<FormState>&& formState)
{
    return adoptRef(*new InjectedBundleNavigationAction(frame, action, WTFMove(formState)));
}

InjectedBundleNavigationAction::InjectedBundleNavigationAction(WebFrame* frame, const NavigationAction& navigationAction, RefPtr<FormState>&& formState)
    : m_navigationType(navigationAction.type())
    , m_modifiers(modifiersForNavigationAction(navigationAction))
    , m_mouseButton(WebMouseEvent::NoButton)
    , m_downloadAttribute(navigationAction.downloadAttribute())
    , m_shouldOpenExternalURLs(navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllow || navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes)
    , m_shouldTryAppLinks(navigationAction.shouldOpenExternalURLsPolicy() == ShouldOpenExternalURLsPolicy::ShouldAllow)
{
    if (auto mouseEventData = navigationAction.mouseEventData()) {
        m_hitTestResult = InjectedBundleHitTestResult::create(frame->coreFrame()->eventHandler().hitTestResultAtPoint(mouseEventData->absoluteLocation, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent));
        m_mouseButton = mouseButtonForMouseEventData(mouseEventData);
        m_syntheticClickType = syntheticClickTypeForNavigationAction(navigationAction);
        m_clickLocationInRootViewCoordinates = clickLocationInRootViewCoordinatesForNavigationAction(navigationAction);
    }

    if (formState)
        m_formElement = InjectedBundleNodeHandle::getOrCreate(formState->form());
}

} // namespace WebKit
