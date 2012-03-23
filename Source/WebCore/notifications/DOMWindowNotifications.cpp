/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "DOMWindowNotifications.h"

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#include "DOMWindow.h"
#include "Document.h"
#include "NotificationCenter.h"
#include "NotificationController.h"
#include "Page.h"

namespace WebCore {

DOMWindowNotifications::DOMWindowNotifications()
{
}

DOMWindowNotifications::~DOMWindowNotifications()
{
}

DOMWindowNotifications* DOMWindowNotifications::from(DOMWindow* window)
{
    DOMWindowNotifications* supplement = static_cast<DOMWindowNotifications*>(Supplement<DOMWindow>::from(window, supplementName()));
    if (!supplement) {
        supplement = new DOMWindowNotifications();
        Supplement<DOMWindow>::provideTo(window, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

NotificationCenter* DOMWindowNotifications::webkitNotifications(DOMWindow* window)
{
    if (!window->isCurrentlyDisplayedInFrame())
        return 0;

    DOMWindowNotifications* supplement = DOMWindowNotifications::from(window);

    if (supplement->m_notificationCenter)
        return supplement->m_notificationCenter.get();

    Document* document = window->document();
    if (!document)
        return 0;
    
    Page* page = document->page();
    if (!page)
        return 0;

    NotificationClient* provider = NotificationController::clientFrom(page);
    if (provider) 
        supplement->m_notificationCenter = NotificationCenter::create(document, provider);    

    return supplement->m_notificationCenter.get();
}

void DOMWindowNotifications::reset(DOMWindow* window)
{
    DOMWindowNotifications* supplement = static_cast<DOMWindowNotifications*>(Supplement<DOMWindow>::from(window, supplementName()));
    if (!supplement)
        return;
    if (!supplement->m_notificationCenter)
        return;
    supplement->m_notificationCenter->disconnectFrame();
    supplement->m_notificationCenter = 0;
}

const AtomicString& DOMWindowNotifications::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, supplementName, ("DOMWindowNotifications"));
    return supplementName;
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
