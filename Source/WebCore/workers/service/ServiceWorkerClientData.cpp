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
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "SWClientConnection.h"
#include "WorkerGlobalScope.h"
#include <wtf/CrossThreadCopier.h>

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

ServiceWorkerClientData ServiceWorkerClientData::isolatedCopy() const &
{
    return { identifier, type, frameType, url.isolatedCopy(), ownerURL.isolatedCopy(), pageIdentifier, frameIdentifier, lastNavigationWasAppInitiated, isVisible, isFocused, focusOrder, crossThreadCopy(ancestorOrigins) };
}

ServiceWorkerClientData ServiceWorkerClientData::isolatedCopy() &&
{
    return { identifier, type, frameType, WTFMove(url).isolatedCopy(), WTFMove(ownerURL).isolatedCopy(), pageIdentifier, frameIdentifier, lastNavigationWasAppInitiated, isVisible, isFocused, focusOrder, crossThreadCopy(WTFMove(ancestorOrigins)) };
}

ServiceWorkerClientData ServiceWorkerClientData::from(ScriptExecutionContext& context)
{
    if (auto* document = dynamicDowncast<Document>(context)) {
        auto lastNavigationWasAppInitiated = document->loader() && document->loader()->lastNavigationWasAppInitiated() ? LastNavigationWasAppInitiated::Yes : LastNavigationWasAppInitiated::No;

        Vector<String> ancestorOrigins;
        if (auto* frame = document->frame()) {
            for (auto* ancestor = frame->tree().parent(); ancestor; ancestor = ancestor->tree().parent()) {
                if (auto* ancestorFrame = dynamicDowncast<LocalFrame>(ancestor))
                    ancestorOrigins.append(ancestorFrame->document()->securityOrigin().toString());
            }
        }

        return {
            context.identifier(),
            ServiceWorkerClientType::Window,
            toServiceWorkerClientFrameType(context),
            document->creationURL(),
            URL(),
            document->pageID(),
            document->frameID(),
            lastNavigationWasAppInitiated,
            !document->hidden(),
            document->hasFocus(),
            0,
            WTFMove(ancestorOrigins)
        };
    }

    RELEASE_ASSERT(is<WorkerGlobalScope>(context));
    auto& scope = downcast<WorkerGlobalScope>(context);
    return {
        scope.identifier(),
        scope.type() == WebCore::WorkerGlobalScope::Type::SharedWorker ? ServiceWorkerClientType::Sharedworker : ServiceWorkerClientType::Worker,
        ServiceWorkerClientFrameType::None,
        scope.url(),
        scope.ownerURL(),
        { },
        { },
        LastNavigationWasAppInitiated::No,
        false,
        false,
        0,
        { }
    };
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
