/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PermissionStatus.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "EventNames.h"
#include "MainThreadPermissionObserver.h"
#include "PermissionController.h"
#include "PermissionState.h"
#include "Permissions.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PermissionStatus);

static HashMap<MainThreadPermissionObserverIdentifier, std::unique_ptr<MainThreadPermissionObserver>>& allMainThreadPermissionObservers()
{
    static MainThreadNeverDestroyed<HashMap<MainThreadPermissionObserverIdentifier, std::unique_ptr<MainThreadPermissionObserver>>> map;
    return map;
}

Ref<PermissionStatus> PermissionStatus::create(ScriptExecutionContext& context, PermissionState state, PermissionDescriptor descriptor, PermissionQuerySource source, SingleThreadWeakPtr<Page>&& page)
{
    auto status = adoptRef(*new PermissionStatus(context, state, descriptor, source, WTFMove(page)));
    status->suspendIfNeeded();
    return status;
}

PermissionStatus::PermissionStatus(ScriptExecutionContext& context, PermissionState state, PermissionDescriptor descriptor, PermissionQuerySource source, SingleThreadWeakPtr<Page>&& page)
    : ActiveDOMObject(&context)
    , m_state(state)
    , m_descriptor(descriptor)
{
    RefPtr origin = context.securityOrigin();
    auto originData = origin ? origin->data() : SecurityOriginData { };
    ClientOrigin clientOrigin { context.topOrigin().data(), WTFMove(originData) };

    m_mainThreadPermissionObserverIdentifier = MainThreadPermissionObserverIdentifier::generate();

    ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, contextIdentifier = context.identifier(), state = m_state, descriptor = m_descriptor, source, page = WTFMove(page), origin = WTFMove(clientOrigin).isolatedCopy(), identifier = m_mainThreadPermissionObserverIdentifier]() mutable {
        auto mainThreadPermissionObserver = makeUnique<MainThreadPermissionObserver>(WTFMove(weakThis), contextIdentifier, state, descriptor, source, WTFMove(page), WTFMove(origin));
        allMainThreadPermissionObservers().add(identifier, WTFMove(mainThreadPermissionObserver));
    });
}

PermissionStatus::~PermissionStatus()
{
    if (!m_mainThreadPermissionObserverIdentifier)
        return;

    callOnMainThread([identifier = m_mainThreadPermissionObserverIdentifier] {
        allMainThreadPermissionObservers().remove(identifier);
    });
}

void PermissionStatus::stateChanged(PermissionState newState)
{
    if (m_state == newState)
        return;

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    RefPtr document = dynamicDowncast<Document>(context.get());
    if (document && !document->isFullyActive())
        return;

    m_state = newState;
    queueTaskToDispatchEvent(*this, TaskSource::Permission, Event::create(eventNames().changeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

const char* PermissionStatus::activeDOMObjectName() const
{
    return "PermissionStatus";
}

bool PermissionStatus::virtualHasPendingActivity() const
{
    if (!m_hasChangeEventListener)
        return false;

    if (auto* document = dynamicDowncast<Document>(scriptExecutionContext()))
        return document->hasBrowsingContext();

    return true;
}

void PermissionStatus::eventListenersDidChange()
{
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);
}

} // namespace WebCore
