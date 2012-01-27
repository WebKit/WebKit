/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PointerLockController.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Element.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "VoidCallback.h"

#if ENABLE(POINTER_LOCK)

namespace WebCore {

PointerLockController::PointerLockController(Page* page)
    : m_page(page)
{
}

PassOwnPtr<PointerLockController> PointerLockController::create(Page* page)
{
    return adoptPtr(new PointerLockController(page));
}

void PointerLockController::requestPointerLock(Element* target, PassRefPtr<VoidCallback> successCallback, PassRefPtr<VoidCallback> failureCallback)
{
    if (isLocked()) {
        if (m_element == target) {
            if (successCallback)
                successCallback->handleEvent();
        } else {
            didLosePointerLock();
            m_element = target;
            if (successCallback)
                successCallback->handleEvent();
        }
    } else if (m_page->chrome()->client()->requestPointerLock()) {
        m_element = target;
        m_successCallback = successCallback;
        m_failureCallback = failureCallback;
    } else if (failureCallback)
        failureCallback->handleEvent();
}

void PointerLockController::requestPointerUnlock()
{
    return m_page->chrome()->client()->requestPointerUnlock();
}

bool PointerLockController::isLocked()
{
    return m_page->chrome()->client()->isPointerLocked();
}

void PointerLockController::didAcquirePointerLock()
{
    RefPtr<Element> elementToNotify(m_element);
    RefPtr<VoidCallback> callbackToIssue(m_successCallback);
    m_successCallback = 0;
    m_failureCallback = 0;

    if (callbackToIssue && elementToNotify && elementToNotify->document()->frame())
        callbackToIssue->handleEvent();
}

void PointerLockController::didNotAcquirePointerLock()
{
    RefPtr<Element> elementToNotify(m_element);
    RefPtr<VoidCallback> callbackToIssue(m_failureCallback);
    m_element = 0;
    m_successCallback = 0;
    m_failureCallback = 0;

    if (callbackToIssue && elementToNotify && elementToNotify->document()->frame())
        callbackToIssue->handleEvent();
}

void PointerLockController::didLosePointerLock()
{
    RefPtr<Element> elementToNotify(m_element);
    m_element = 0;
    m_successCallback = 0;
    m_failureCallback = 0;
    if (elementToNotify && elementToNotify->document()->frame())
        elementToNotify->dispatchEvent(Event::create(eventNames().webkitpointerlocklostEvent, true, false));
}

void PointerLockController::dispatchLockedMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType)
{
    if (!m_element || !m_element->document()->frame())
        return;

    m_element->dispatchMouseEvent(event, eventType, event.clickCount());

    // Create click events
    if (eventType == eventNames().mouseupEvent)
        m_element->dispatchMouseEvent(event, eventNames().clickEvent, event.clickCount());
}

} // namespace WebCore

#endif // ENABLE(POINTER_LOCK)
