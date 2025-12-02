/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "RemoteLayerTreeDrawingArea.h"
#include <wtf/TZoneMalloc.h>

#if PLATFORM(MAC)

namespace WebCore {
class TiledBacking;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaMac final : public RemoteLayerTreeDrawingArea {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeDrawingAreaMac);
public:
    static Ref<RemoteLayerTreeDrawingAreaMac> create(WebPage& webPage, const WebPageCreationParameters& parameters)
    {
        return adoptRef(*new RemoteLayerTreeDrawingAreaMac(webPage, parameters));
    }

    virtual ~RemoteLayerTreeDrawingAreaMac();

private:
    RemoteLayerTreeDrawingAreaMac(WebPage&, const WebPageCreationParameters&);

    WebCore::DelegatedScrollingMode delegatedScrollingMode() const final;

    void setColorSpace(std::optional<WebCore::DestinationColorSpace>) final;
    std::optional<WebCore::DestinationColorSpace> displayColorSpace() const final;

    std::optional<WebCore::DestinationColorSpace> m_displayColorSpace;

    bool usesDelegatedPageScaling() const override { return false; }

    void mainFrameContentSizeChanged(WebCore::FrameIdentifier, const WebCore::IntSize&) final;

    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) final;

    void willCommitMainFrameData(MainFrameData&) final;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
