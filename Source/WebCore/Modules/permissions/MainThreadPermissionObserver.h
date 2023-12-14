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

#pragma once

#include "ClientOrigin.h"
#include "MainThreadPermissionObserverIdentifier.h"
#include "PermissionDescriptor.h"
#include "PermissionObserver.h"
#include "PermissionQuerySource.h"
#include "PermissionState.h"
#include "PermissionStatus.h"
#include "ScriptExecutionContextIdentifier.h"

namespace WebCore {

class Page;

class MainThreadPermissionObserver final : public PermissionObserver {
    WTF_MAKE_NONCOPYABLE(MainThreadPermissionObserver);
    WTF_MAKE_FAST_ALLOCATED;
public:
    MainThreadPermissionObserver(ThreadSafeWeakPtr<PermissionStatus>&&, ScriptExecutionContextIdentifier, PermissionState, PermissionDescriptor, PermissionQuerySource, SingleThreadWeakPtr<Page>&&, ClientOrigin&&);
    ~MainThreadPermissionObserver();

private:
    // PermissionObserver
    PermissionState currentState() const final { return m_state; }
    void stateChanged(PermissionState) final;
    const ClientOrigin& origin() const final { return m_origin; }
    PermissionDescriptor descriptor() const final { return m_descriptor; }
    PermissionQuerySource source() const final { return m_source; }
    const SingleThreadWeakPtr<Page>& page() const final { return m_page; }

    ThreadSafeWeakPtr<PermissionStatus> m_permissionStatus;
    ScriptExecutionContextIdentifier m_contextIdentifier;
    PermissionState m_state;
    PermissionDescriptor m_descriptor;
    PermissionQuerySource m_source;
    SingleThreadWeakPtr<Page> m_page;
    ClientOrigin m_origin;
};

} // namespace WebCore
