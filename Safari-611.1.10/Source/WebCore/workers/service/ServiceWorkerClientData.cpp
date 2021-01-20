/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)
#include "ServiceWorkerClientData.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "SWClientConnection.h"

namespace WebCore {

static ServiceWorkerClientFrameType toServiceWorkerClientFrameType(ScriptExecutionContext& context)
{
    if (!is<Document>(context))
        return ServiceWorkerClientFrameType::None;

    auto& document = downcast<Document>(context);
    auto* frame = document.frame();
    if (!frame)
        return ServiceWorkerClientFrameType::None;

    if (frame->isMainFrame()) {
        if (auto* window = document.domWindow()) {
            if (window->opener())
                return ServiceWorkerClientFrameType::Auxiliary;
        }
        return ServiceWorkerClientFrameType::TopLevel;
    }
    return ServiceWorkerClientFrameType::Nested;
}

ServiceWorkerClientData ServiceWorkerClientData::isolatedCopy() const
{
    return { identifier, type, frameType, url.isolatedCopy() };
}

ServiceWorkerClientData ServiceWorkerClientData::from(ScriptExecutionContext& context, SWClientConnection& connection)
{
    bool isDocument = is<Document>(context);
    RELEASE_ASSERT(isDocument); // We do not support dedicated workers as clients yet.

    return {
        { connection.serverConnectionIdentifier(), downcast<Document>(context).identifier() },
        isDocument ? ServiceWorkerClientType::Window : ServiceWorkerClientType::Worker,
        toServiceWorkerClientFrameType(context),
        context.url()
    };
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
