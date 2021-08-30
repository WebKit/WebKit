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
#include "PermissionDescriptor.h"
#include "PermissionName.h"
#include "PermissionObserver.h"
#include "PermissionState.h"

namespace WebCore {

class PermissionController;
class ScriptExecutionContext;

class PermissionStatus final : public PermissionObserver, public ActiveDOMObject, public RefCounted<PermissionStatus>, public EventTargetWithInlineData  {
    WTF_MAKE_ISO_ALLOCATED(PermissionStatus);
public:
    static Ref<PermissionStatus> create(ScriptExecutionContext&, PermissionState, const PermissionDescriptor&);
    ~PermissionStatus();

    PermissionState state() const { return m_state; }
    PermissionName name() const { return m_descriptor.name; }

    using RefCounted::ref;
    using RefCounted::deref;

    using PermissionObserver::weakPtrFactory;
    using WeakValueType = PermissionObserver::WeakValueType;

private:
    PermissionStatus(ScriptExecutionContext&, PermissionState, const PermissionDescriptor&);

    // PermissionObserver
    void stateChanged(PermissionState) final;
    const ClientOrigin& origin() const final { return m_origin; }
    const PermissionDescriptor& descriptor() const final { return m_descriptor; }

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
    ClientOrigin m_origin;
    RefPtr<PermissionController> m_controller;
    std::atomic<bool> m_hasChangeEventListener;
};

} // namespace WebCore
