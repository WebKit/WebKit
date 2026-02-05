/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WPE_PLATFORM)
#include "FenceMonitor.h"
#include "MessageReceiver.h"
#include "RendererBufferDescription.h"
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

typedef struct _WPEBuffer WPEBuffer;
typedef struct _WPEView WPEView;

#if OS(ANDROID)
typedef struct AHardwareBuffer AHardwareBuffer;
#endif

namespace WebCore {
class ShareableBitmapHandle;
}

namespace WTF {
class UnixFileDescriptor;
}

namespace WebKit {

class ViewSnapshot;
class WebPageProxy;
class WebProcessProxy;

class AcceleratedBackingStore final : public IPC::MessageReceiver, public RefCounted<AcceleratedBackingStore> {
    WTF_MAKE_TZONE_ALLOCATED(AcceleratedBackingStore);
    WTF_MAKE_NONCOPYABLE(AcceleratedBackingStore);
public:
    using Rects = Vector<WebCore::IntRect, 1>;

    static Ref<AcceleratedBackingStore> create(WebPageProxy&, WPEView*);
    ~AcceleratedBackingStore();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void updateSurfaceID(uint64_t);

    Expected<Ref<ViewSnapshot>, String> takeSnapshot(std::optional<WebCore::IntRect>&&);

    RendererBufferDescription bufferDescription() const;

private:
    AcceleratedBackingStore(WebPageProxy&, WPEView*);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didCreateDMABufBuffer(uint64_t id, const WebCore::IntSize&, uint32_t format, Vector<WTF::UnixFileDescriptor>&&, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, RendererBufferFormat::Usage);
    void didCreateSHMBuffer(uint64_t id, WebCore::ShareableBitmapHandle&&);
#if OS(ANDROID)
    void didCreateAndroidBuffer(uint64_t id, RefPtr<AHardwareBuffer>&&);
#endif
    void didChangeBufferConfiguration(uint32_t bufferCount);
    void didDestroyBuffer(uint64_t id);
    void frame(uint64_t bufferID, Rects&&, WTF::UnixFileDescriptor&&);
    void frameDone();
    void renderPendingBuffer();
    void bufferRendered();
    void bufferReleased(WPEBuffer*);
    void toplevelChanged();

    void notifyBufferConfigurationIfNeeded();

    WeakPtr<WebPageProxy> m_webPage;
    GRefPtr<WPEView> m_wpeView;
    FenceMonitor m_fenceMonitor;
    uint64_t m_surfaceID { 0 };
    WeakPtr<WebProcessProxy> m_legacyMainFrameProcess;
    GRefPtr<WPEBuffer> m_pendingBuffer;
    GRefPtr<WPEBuffer> m_committedBuffer;
    Rects m_pendingDamageRects;
    HashMap<uint64_t, GRefPtr<WPEBuffer>> m_buffers;
    HashMap<WPEBuffer*, uint64_t> m_bufferIDs;
    std::optional<unsigned> m_targetBufferCount;
};

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
