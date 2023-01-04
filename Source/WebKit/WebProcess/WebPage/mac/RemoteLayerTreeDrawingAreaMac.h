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

#if PLATFORM(MAC)

namespace WebCore {
class TiledBacking;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaMac final : public RemoteLayerTreeDrawingArea {
public:
    RemoteLayerTreeDrawingAreaMac(WebPage&, const WebPageCreationParameters&);
    virtual ~RemoteLayerTreeDrawingAreaMac();

private:
    WebCore::DelegatedScrollingMode delegatedScrollingMode() const override;

    void setColorSpace(std::optional<WebCore::DestinationColorSpace>) override;
    std::optional<WebCore::DestinationColorSpace> displayColorSpace() const override;

    std::optional<WebCore::DestinationColorSpace> m_displayColorSpace;

    bool usesDelegatedPageScaling() const override { return false; }

    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void applyTransientZoomToPage(double scale, WebCore::FloatPoint);

    void willCommitLayerTree(RemoteLayerTreeTransaction&) override;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
