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

#pragma once

#include "ActiveDOMObject.h"
#include "ClientOrigin.h"
#include "EventTarget.h"
#include "MainThreadPermissionObserverIdentifier.h"
#include "Page.h"
#include "PermissionDescriptor.h"
#include "PermissionName.h"
#include "PermissionQuerySource.h"
#include "PermissionState.h"

namespace WebCore {

class ScriptExecutionContext;

class PermissionStatus final : public ActiveDOMObject, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PermissionStatus>, public EventTarget  {
    WTF_MAKE_ISO_ALLOCATED(PermissionStatus);
public:
    static Ref<PermissionStatus> create(ScriptExecutionContext&, PermissionState, PermissionDescriptor, PermissionQuerySource, SingleThreadWeakPtr<Page>&&);
    ~PermissionStatus();

    PermissionState state() const { return m_state; }
    PermissionName name() const { return m_descriptor.name; }

    void stateChanged(PermissionState);

    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref;
    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref;

private:
    PermissionStatus(ScriptExecutionContext&, PermissionState, PermissionDescriptor, PermissionQuerySource, SingleThreadWeakPtr<Page>&&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    EventTargetInterface eventTargetInterface() const final { return PermissionStatusEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;

    PermissionState m_state;
    PermissionDescriptor m_descriptor;
    MainThreadPermissionObserverIdentifier m_mainThreadPermissionObserverIdentifier;
    bool m_hasChangeEventListener { false };
};

} // namespace WebCore
