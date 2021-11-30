/*
 * Copyright (C) 2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#pragma once

#include "ClientOrigin.h"
#include "ScriptExecutionContextIdentifier.h"
#include "WebLockIdentifier.h"
#include "WebLockManager.h"
#include "WebLockMode.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Exception;
struct WebLockManagerSnapshot;

class WebLockRegistry : public RefCounted<WebLockRegistry>, public CanMakeWeakPtr<WebLockRegistry> {
public:
    static Ref<WebLockRegistry> registryForOrigin(const ClientOrigin&);
    ~WebLockRegistry();

    void requestLock(WebLockIdentifier, ScriptExecutionContextIdentifier, const String& name, WebLockMode, bool steal, bool ifAvailable, Function<void(bool)>&& grantedHandler, Function<void(Exception&&)>&& releaseHandler);
    void releaseLock(WebLockIdentifier, const String& name);
    void abortLockRequest(WebLockIdentifier, const String& name, CompletionHandler<void(bool)>&&);
    void snapshot(CompletionHandler<void(WebLockManagerSnapshot&&)>&&);
    void clientIsGoingAway(ScriptExecutionContextIdentifier);

private:
    explicit WebLockRegistry(const ClientOrigin&);

    struct LockInfo;
    struct LockRequest;

    bool isGrantable(const LockRequest&) const;
    void processLockRequestQueue(const String& name, Deque<LockRequest>&);

    const ClientOrigin m_origin;
    HashMap<String, Deque<LockRequest>> m_lockRequestQueueMap;
    HashMap<String, Vector<LockInfo>> m_heldLocks;
};

} // namespace WebCore
