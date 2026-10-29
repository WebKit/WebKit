/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "RemoteGraphicsContextProxy.h"
#include "RemoteSnapshotRecorderIdentifier.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WebKit {

// RemoteSnapshotRecorder is a display list recorder that can convert a snapshot subframe rendering into
// its own draw item. In other words, RemoteSnapshotRecorder has the right to source snapshot subframe
// renderings.
class RemoteSnapshotRecorderProxy : public RemoteGraphicsContextProxy {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSnapshotRecorderProxy);

public:
    // renderingMode is RenderingMode::PDFDocument when the snapshot will be sunk into a PDF, and RenderingMode::DisplayList otherwise.
    RemoteSnapshotRecorderProxy(const WebCore::FloatRect& initialClip, WebCore::RenderingMode, RemoteDrawingBackendProxy&);
    ~RemoteSnapshotRecorderProxy();
    RemoteSnapshotRecorderIdentifier identifier() const { return RemoteSnapshotRecorderIdentifier { RemoteGraphicsContextProxy::identifier().toUInt64() }; }

    void drawSnapshotFrame(WebCore::FrameIdentifier);

private:
    bool drawPDFPage(WebCore::RenderingResourceIdentifier documentIdentifier, NOESCAPE const Function<RefPtr<WebCore::FragmentedSharedBuffer>()>& documentData, unsigned pageIndex, const WebCore::FloatRect& source, const WebCore::FloatRect& destination, bool includeLinks) final;

    // The PDF documents sent for this recording, by the identifiers that the drawing gives them. They
    // are sent with identifiers of their own, so that recordings drawing the same document do not
    // depend on each other.
    HashMap<WebCore::RenderingResourceIdentifier, WebCore::RenderingResourceIdentifier> m_cachedPDFDocuments;
};

}

#endif
