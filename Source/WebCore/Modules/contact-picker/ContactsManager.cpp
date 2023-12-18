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
#include "ContactsManager.h"

#include "Chrome.h"
#include "ContactInfo.h"
#include "ContactProperty.h"
#include "ContactsRequestData.h"
#include "ContactsSelectOptions.h"
#include "Document.h"
#include "JSContactInfo.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "Navigator.h"
#include "Page.h"
#include "UserGestureIndicator.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ContactsManager);

Ref<ContactsManager> ContactsManager::create(Navigator& navigator)
{
    return adoptRef(*new ContactsManager(navigator));
}

ContactsManager::ContactsManager(Navigator& navigator)
    : m_navigator(navigator)
{
}

ContactsManager::~ContactsManager() = default;


LocalFrame* ContactsManager::frame() const
{
    return m_navigator ? m_navigator->frame() : nullptr;
}

Navigator* ContactsManager::navigator()
{
    return m_navigator.get();
}

void ContactsManager::getProperties(Ref<DeferredPromise>&& promise)
{
    Vector<ContactProperty> properties = { ContactProperty::Email, ContactProperty::Name, ContactProperty::Tel };
    promise->resolve<IDLSequence<IDLEnumeration<ContactProperty>>>(properties);
}

void ContactsManager::select(const Vector<ContactProperty>& properties, const ContactsSelectOptions& options, Ref<DeferredPromise>&& promise)
{
    RefPtr frame = this->frame();
    if (!frame || !frame->isMainFrame() || !frame->document() || !frame->page()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    if (!UserGestureIndicator::processingUserGesture()) {
        promise->reject(ExceptionCode::SecurityError);
        return;
    }

    if (m_contactPickerIsShowing) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    if (properties.isEmpty()) {
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    ContactsRequestData requestData;
    requestData.properties = properties;
    requestData.multiple = options.multiple;
    requestData.url = frame->document()->url().truncatedForUseAsBase().string();

    m_contactPickerIsShowing = true;

    frame->page()->chrome().showContactPicker(requestData, [promise = WTFMove(promise), weakThis = WeakPtr { *this }] (std::optional<Vector<ContactInfo>>&& info) {
        if (weakThis)
            weakThis->m_contactPickerIsShowing = false;

        if (info) {
            promise->resolve<IDLSequence<IDLDictionary<ContactInfo>>>(*info);
            return;
        }

        promise->reject(ExceptionCode::UnknownError);
    });
}

} // namespace WebCore
