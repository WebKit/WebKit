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

#include "PermissionState.h"
#include <wtf/CompletionHandler.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class PermissionObserver;
struct ClientOrigin;
struct PermissionDescriptor;

class PermissionController : public ThreadSafeRefCounted<PermissionController> {
public:
    virtual ~PermissionController() = default;
    virtual PermissionState query(ClientOrigin&&, PermissionDescriptor&&) = 0;
    virtual void request(ClientOrigin&&, PermissionDescriptor&&, CompletionHandler<void(PermissionState)>&&) = 0;
    virtual void addObserver(PermissionObserver&) = 0;
    virtual void removeObserver(PermissionObserver&) = 0;
protected:
    PermissionController() = default;
};

class DummyPermissionController final : public PermissionController {
public:
    static Ref<DummyPermissionController> create() { return adoptRef(*new DummyPermissionController); }
private:
    DummyPermissionController() = default;
    PermissionState query(ClientOrigin&&, PermissionDescriptor&&) final { return PermissionState::Denied; }
    void request(ClientOrigin&&, PermissionDescriptor&&, CompletionHandler<void(PermissionState)>&& completionHandler) final { completionHandler(PermissionState::Denied); }
    void addObserver(PermissionObserver&) final { }
    void removeObserver(PermissionObserver&) final { }
};

} // namespace WebCore
