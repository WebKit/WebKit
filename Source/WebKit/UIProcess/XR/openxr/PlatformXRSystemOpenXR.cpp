/*
 * Copyright (C) 2025 Igalia, S.L.
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
#include "PlatformXRSystem.h"

#if ENABLE(WEBXR) && USE(OPENXR)

#include "PlatformXROpenXR.h"
#include "WebPageProxy.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

PlatformXRCoordinator* PlatformXRSystem::xrCoordinator()
{
    static NeverDestroyed<OpenXRCoordinator> xrCoordinator;
    return &xrCoordinator.get();
}

void PlatformXRSystem::createLayerProjection(IPC::Connection&, uint32_t width, uint32_t height, bool alpha, CompletionHandler<void(std::optional<PlatformXR::LayerHandle>)>&& reply)
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page) {
        reply(std::nullopt);
        return;
    }

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->createLayerProjection(width, height, alpha, WTFMove(reply));
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
