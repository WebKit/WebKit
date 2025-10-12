/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebEventModifier.h"

#include <WebCore/Document.h>
#include <WebCore/LocalDOMWindow.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/PlatformEvent.h>
#include <wtf/OptionSet.h>

namespace WebKit {

OptionSet<WebEventModifier> modifiersFromPlatformEventModifiers(OptionSet<WebCore::PlatformEventModifier> modifiers)
{
    OptionSet<WebEventModifier> result;
    if (modifiers.contains(WebCore::PlatformEventModifier::ShiftKey))
        result.add(WebEventModifier::ShiftKey);
    if (modifiers.contains(WebCore::PlatformEventModifier::ControlKey))
        result.add(WebEventModifier::ControlKey);
    if (modifiers.contains(WebCore::PlatformEventModifier::AltKey))
        result.add(WebEventModifier::AltKey);
    if (modifiers.contains(WebCore::PlatformEventModifier::MetaKey))
        result.add(WebEventModifier::MetaKey);
    if (modifiers.contains(WebCore::PlatformEventModifier::CapsLockKey))
        result.add(WebEventModifier::CapsLockKey);
    return result;
}

OptionSet<WebEventModifier> modifiersForNavigationAction(const WebCore::NavigationAction& navigationAction)
{
    OptionSet<WebEventModifier> modifiers;
    if (auto keyStateEventData = navigationAction.keyStateEventData()) {
        RefPtr document = navigationAction.requester() ? WebCore::Document::allDocumentsMap().get(navigationAction.requester()->documentIdentifier) : nullptr;
        RefPtr domWindow = document ? document->window() : nullptr;
        auto lastMouseClickEvent = domWindow ? domWindow->consumeLastUserClickEvent() : std::nullopt;
        // For security reasons, we normally only set modifiers when the navigation comes from a trusted event.
        // However, a common pattern on the web is to intercept the mouse click event, do some logic (e.g. link tracking),
        // and then fire a "cloned" version of the event, which is no longer trusted. In such case, we want to carry forward
        // the modifiers from the original event as this is what the user intended. This is particularly important when the
        // user CMD + clicks a link and rightfully expects the link to open in a new tab.
        auto shouldMaintainModifier = [isTrusted = keyStateEventData->isTrusted, lastMouseClickEvent](WebCore::PlatformEventModifier modifier) {
            if (isTrusted)
                return true;
            if (!lastMouseClickEvent || (MonotonicTime::now() - lastMouseClickEvent->time) > WebCore::LocalDOMWindow::transientActivationDuration())
                return false;
            return lastMouseClickEvent->modifiers.contains(modifier);
        };
        if (keyStateEventData->shiftKey && shouldMaintainModifier(WebCore::PlatformEventModifier::ShiftKey))
            modifiers.add(WebEventModifier::ShiftKey);
        if (keyStateEventData->ctrlKey && shouldMaintainModifier(WebCore::PlatformEventModifier::ControlKey))
            modifiers.add(WebEventModifier::ControlKey);
        if (keyStateEventData->altKey && shouldMaintainModifier(WebCore::PlatformEventModifier::AltKey))
            modifiers.add(WebEventModifier::AltKey);
        if (keyStateEventData->metaKey && shouldMaintainModifier(WebCore::PlatformEventModifier::MetaKey))
            modifiers.add(WebEventModifier::MetaKey);
    }
    return modifiers;
}

} // namespace WebKit
