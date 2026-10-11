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

#include "config.h"
#include "DisplayOnlyImageProxy.h"

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include "WebPage.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/FloatSize.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/MainThread.h>
#include <wtf/NativePromise.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DisplayOnlyImageProxy);

RefPtr<NativePromise<Ref<NativeImage>, void>> DisplayOnlyImageProxy::create(WebPage& page, FrameIdentifier rootFrameIdentifier, const FloatSize& size, float scale, const ColorSpace& colorSpace, NOESCAPE const Function<void(GraphicsContext&)>& paint)
{
    if (size.isEmpty())
        return nullptr;

    auto identifier = generateRemoteSnapshotIdentifier();
    // Created first, so that the snapshot is released if the root cannot be recorded.
    Ref image = adoptRef(*new DisplayOnlyImageProxy(identifier, page.identifier(), expandedIntSize(size.scaled(scale)), colorSpace));

    // A root that fails to record fails the snapshot in the GPU process, so that the UI process
    // fails to complete it below.
    auto rootRecorded = MainRunLoopSuccessCallbackAggregator::create(WebPage::failRemoteSnapshotIfRootFails(identifier, [](bool) { }));
    if (!page.recordRemoteSnapshot(identifier, rootFrameIdentifier, WebPage::RemoteSnapshotRole::Root, RenderingMode::DisplayList, FloatRect { { }, size }, size, WTF::move(rootRecorded), paint))
        return nullptr;

    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return nullptr;

    // Not waiting for the root: the GPU process waits for it, and for the frames hosted elsewhere,
    // before drawing. Holding the image until the reply keeps the release behind it, on the same
    // connection.
    NativePromise<Ref<NativeImage>, void>::Producer producer;
    Ref promise = producer.promise();
    connection->sendWithAsyncReply(Messages::WebProcessProxy::CompleteDisplayOnlyImage(page.identifier(), identifier, rootFrameIdentifier, scale, colorSpace), [image = WTF::move(image), producer = WTF::move(producer)](bool success, bool isMissingFrames) mutable {
        image->m_isMissingFrames = isMissingFrames;
        if (success)
            producer.resolve(Ref<NativeImage> { WTF::move(image) });
        else
            producer.reject();
    });

    return promise;
}

DisplayOnlyImageProxy::DisplayOnlyImageProxy(RemoteSnapshotIdentifier identifier, PageIdentifier pageIdentifier, const IntSize& size, const ColorSpace& colorSpace)
    : m_identifier(identifier)
    , m_pageIdentifier(pageIdentifier)
    , m_size(size)
    , m_colorSpace(colorSpace)
{
}

// Releases the image, and the snapshot it is drawn from. The root creates the snapshot before
// painting, so the release cannot overtake that, and a frame hosted elsewhere that records
// afterwards records into nothing.
DisplayOnlyImageProxy::~DisplayOnlyImageProxy()
{
    // Not through the WebPage, which may be gone while the UI process still holds the rendering.
    // Images are thread safe, so the last reference may be dropped on any thread.
    ensureOnMainRunLoop([pageIdentifier = m_pageIdentifier, identifier = m_identifier] {
        if (RefPtr connection = WebProcess::singleton().parentProcessConnection())
            connection->send(Messages::WebProcessProxy::ReleaseDisplayOnlyImage(pageIdentifier, identifier), 0);
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
