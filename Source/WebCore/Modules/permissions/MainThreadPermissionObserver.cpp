/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "MainThreadPermissionObserver.h"

#include "ClientOrigin.h"
#include "Document.h"
#include "PermissionController.h"
#include "PermissionState.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

MainThreadPermissionObserver::MainThreadPermissionObserver(ThreadSafeWeakPtr<PermissionStatus>&& permissionStatus, ScriptExecutionContextIdentifier contextIdentifier, PermissionState state, PermissionDescriptor descriptor, PermissionQuerySource source, SingleThreadWeakPtr<Page>&& page, ClientOrigin&& origin)
    : m_permissionStatus(WTFMove(permissionStatus))
    , m_contextIdentifier(contextIdentifier)
    , m_state(state)
    , m_descriptor(descriptor)
    , m_source(source)
    , m_page(WTFMove(page))
    , m_origin(WTFMove(origin))
{
    ASSERT(isMainThread());
    PermissionController::shared().addObserver(*this);
}

MainThreadPermissionObserver::~MainThreadPermissionObserver()
{
    ASSERT(isMainThread());
    PermissionController::shared().removeObserver(*this);
}

void MainThreadPermissionObserver::stateChanged(PermissionState newPermissionState)
{
    m_state = newPermissionState;

    ScriptExecutionContext::ensureOnContextThread(m_contextIdentifier, [weakPermissionStatus = m_permissionStatus, newPermissionState](auto&) {
        if (RefPtr permissionStatus = weakPermissionStatus.get())
            permissionStatus->stateChanged(newPermissionState);
    });
}

}
