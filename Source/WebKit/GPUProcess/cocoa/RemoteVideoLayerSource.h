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

#if ENABLE(GPU_PROCESS)

#include <WebCore/PlatformLayer.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS CALayer;

namespace WTF {
struct MachSendRightAnnotated;
}

namespace WebCore {
class FloatSize;
}

namespace WebKit {

// Something in the GPU process that produces a video layer for RemoteLayerHostingManager to
// host: a RemoteMediaPlayerProxy's MediaPlayer, or a RemoteAudioVideoRendererProxyManager
// renderer. Several sources can exist for one media element at once, for example while
// HTMLMediaElement replaces its player, so the manager tracks which one currently owns the
// element's hosted layer, and only that one can tear it down.
//
// A source is owned by its producer and attached by reference, so the manager holds it weakly.
class RemoteVideoLayerSource : public RefCounted<RemoteVideoLayerSource>, public CanMakeWeakPtr<RemoteVideoLayerSource> {
public:
    virtual ~RemoteVideoLayerSource() = default;

    virtual PlatformLayer* platformVideoLayer() const = 0;

    // Called inside the fence the UI process opened for a resize, so that the producer can
    // size whatever it renders into within the same transaction as the hosted layer.
    virtual void setVideoLayerSizeFenced(const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&) = 0;

protected:
    RemoteVideoLayerSource() = default;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
