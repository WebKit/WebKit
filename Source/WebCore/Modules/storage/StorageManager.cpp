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
#include "StorageManager.h"

#include "ClientOrigin.h"
#include "JSDOMPromiseDeferred.h"
#include "NavigatorBase.h"
#include "SecurityOrigin.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StorageManager);

Ref<StorageManager> StorageManager::create(NavigatorBase& navigator)
{
    return adoptRef(*new StorageManager(navigator));
}

StorageManager::StorageManager(NavigatorBase& navigator)
    : m_navigator(navigator)
{
}

static ClientOrigin clientOrigin(ScriptExecutionContext& context)
{
    auto* origin = context.securityOrigin();
    return { context.topOrigin().data(), origin ? origin->data() : SecurityOriginData { } };
}

void StorageManager::persisted(DOMPromiseDeferred<IDLBoolean>&& promise)
{
    auto context = m_navigator.scriptExecutionContext();
    if (!context)
        return promise.reject(Exception { InvalidStateError, "The context is invalid"_s });

    if (auto connection = context->storageConnection()) {
        return connection->persisted(clientOrigin(*context), [promise = WTFMove(promise)](bool persisted) mutable {
            promise.resolve(persisted);
        });
    }

    return promise.reject(Exception { InvalidStateError, "The connection is invalid"_s });
}

void StorageManager::persist(DOMPromiseDeferred<IDLBoolean>&& promise)
{
    auto context = m_navigator.scriptExecutionContext();
    if (!context)
        return promise.reject(Exception { InvalidStateError, "The context is invalid"_s });

    if (auto connection = context->storageConnection()) {
        return connection->persist(clientOrigin(*context), [promise = WTFMove(promise)](bool persisted) mutable {
            promise.resolve(persisted);
        });
    }

    return promise.reject(Exception { InvalidStateError, "The connection is invalid"_s });
}

} // namespace WebCore
