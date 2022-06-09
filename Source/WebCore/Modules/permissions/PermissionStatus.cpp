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
#include "EventNames.h"
#include "PermissionController.h"
#include "SecurityOrigin.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PermissionStatus);

Ref<PermissionStatus> PermissionStatus::create(ScriptExecutionContext& context, PermissionState state, const PermissionDescriptor& descriptor)
{
    auto status = adoptRef(*new PermissionStatus(context, state, descriptor));
    status->suspendIfNeeded();
    return status;
}

PermissionStatus::PermissionStatus(ScriptExecutionContext& context, PermissionState state, const PermissionDescriptor& descriptor)
    : ActiveDOMObject(&context)
    , m_state(state)
    , m_descriptor(descriptor)
    , m_controller(context.permissionController())
{
    auto* origin = context.securityOrigin();
    auto originData = origin ? origin->data() : SecurityOriginData { };
    m_origin = ClientOrigin { context.topOrigin().data(), WTFMove(originData) };

    if (m_controller)
        m_controller->addObserver(*this);
}

PermissionStatus::~PermissionStatus()
{
    if (m_controller)
        m_controller->removeObserver(*this);
}

void PermissionStatus::stateChanged(PermissionState newState)
{
    if (m_state == newState)
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

    auto* context = scriptExecutionContext();
    if (is<Document>(context))
        return downcast<Document>(*context).hasBrowsingContext();

    return true;
}

void PermissionStatus::eventListenersDidChange()
{
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);
}

} // namespace WebCore
