/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(OFFSCREEN_CANVAS) && ENABLE(GPU_PROCESS)

#include <WebCore/PlaceholderRenderingContextSource.h>
#include <atomic>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

// The source for an OffscreenCanvas whose placeholder canvas element is not reachable from this
// process's main thread, normally because it lives in another web content process. Each committed
// frame goes to the UI process, which checks that this process may commit to the placeholder and
// relays the frame to the process that owns it.
class RemotePlaceholderRenderingContextSource final : public WebCore::PlaceholderRenderingContextSource {
    WTF_MAKE_TZONE_ALLOCATED(RemotePlaceholderRenderingContextSource);
public:
    static Ref<RemotePlaceholderRenderingContextSource> create(WebCore::PlaceholderRenderingContextIdentifier);

private:
    explicit RemotePlaceholderRenderingContextSource(WebCore::PlaceholderRenderingContextIdentifier);

    void setPlaceholderBuffer(WebCore::ImageBuffer&, bool originClean, bool opaque) final;

    // Set once the UI process declines a frame: the placeholder is gone or control of it has moved on,
    // and neither is ever undone.
    std::atomic<bool> m_placeholderUnreachable { false };
};

} // namespace WebKit

#endif // ENABLE(OFFSCREEN_CANVAS) && ENABLE(GPU_PROCESS)
