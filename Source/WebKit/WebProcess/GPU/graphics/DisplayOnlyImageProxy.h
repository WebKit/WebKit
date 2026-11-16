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

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include "RemoteSnapshotIdentifier.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class FloatSize;
class GraphicsContext;
}

namespace WebKit {

class WebPage;

// Stands in for an image held by the UI process, which this process must not see because frames
// hosted in other processes recorded into it. This page's layers display the image by identifier,
// for as long as the proxy is alive.
class DisplayOnlyImageProxy final : public WebCore::NativeImage {
    WTF_MAKE_TZONE_ALLOCATED(DisplayOnlyImageProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DisplayOnlyImageProxy);
public:
    static RefPtr<NativePromise<Ref<WebCore::NativeImage>, void>> create(WebPage&, WebCore::FrameIdentifier rootFrameIdentifier, const WebCore::FloatSize&, float scale, const WebCore::ColorSpace&, NOESCAPE const Function<void(WebCore::GraphicsContext&)>& paint);
    ~DisplayOnlyImageProxy();

    RemoteSnapshotIdentifier identifier() const { return m_identifier; }

private:
    DisplayOnlyImageProxy(RemoteSnapshotIdentifier, WebCore::PageIdentifier, const WebCore::IntSize&, const WebCore::ColorSpace&);

    WebCore::PlatformImagePtr platformImage() const final { return nullptr; }
    WebCore::IntSize size() const final { return m_size; }
    bool hasAlpha() const final { return true; }
    WebCore::ColorSpace colorSpace() const final { return m_colorSpace; }
    bool isDisplayOnly() const final { return true; }

    const RemoteSnapshotIdentifier m_identifier;
    const WebCore::PageIdentifier m_pageIdentifier;
    const WebCore::IntSize m_size;
    const WebCore::ColorSpace m_colorSpace;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::DisplayOnlyImageProxy)
    static bool isType(const WebCore::NativeImage& image) { return image.isDisplayOnly(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
