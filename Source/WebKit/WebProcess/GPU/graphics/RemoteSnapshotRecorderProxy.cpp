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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteSnapshotRecorderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteDrawingBackendProxy.h"
#include "RemoteSnapshotRecorderMessages.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSnapshotRecorderProxy);

RemoteSnapshotRecorderProxy::RemoteSnapshotRecorderProxy(const FloatRect& initialClip, RenderingMode renderingMode, RemoteDrawingBackendProxy& drawingBackend)
    : RemoteGraphicsContextProxy(GraphicsContextState::initialIndeterminate(), initialClip, { }, ColorSpace::SRGB(), DrawGlyphsMode::Normal, std::nullopt, renderingMode, RemoteGraphicsContextIdentifier::generate(), drawingBackend)
{
    ASSERT(renderingMode == RenderingMode::DisplayList || renderingMode == RenderingMode::PDFDocument);
}

RemoteSnapshotRecorderProxy::~RemoteSnapshotRecorderProxy()
{
    // What the recording drew of the documents keeps them alive in the snapshot.
    RefPtr drawingBackend = this->drawingBackend();
    if (!drawingBackend)
        return;
    for (auto identifier : m_cachedPDFDocuments.values())
        drawingBackend->releasePDFDocument(identifier);
}

void RemoteSnapshotRecorderProxy::drawSnapshotFrame(FrameIdentifier frameIdentifier)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteSnapshotRecorder::DrawSnapshotFrame(frameIdentifier));
}

bool RemoteSnapshotRecorderProxy::drawPDFPage(RenderingResourceIdentifier documentIdentifier, NOESCAPE const Function<RefPtr<FragmentedSharedBuffer>()>& documentData, unsigned pageIndex, const FloatRect& source, const FloatRect& destination, bool includeLinks)
{
    RefPtr drawingBackend = this->drawingBackend();
    if (!drawingBackend || !drawingBackend->canDrawPDFPages())
        return false;

    // The document is sent once for each recording, so that a recording draws the data the document
    // has when it is recorded.
    auto cachedIdentifier = m_cachedPDFDocuments.getOptional(documentIdentifier);
    if (!cachedIdentifier) {
        RefPtr data = documentData();
        if (!data)
            return false;
        auto identifier = RenderingResourceIdentifier::generate();
        if (!drawingBackend->cachePDFDocument(identifier, *data))
            return false;
        m_cachedPDFDocuments.add(documentIdentifier, identifier);
        cachedIdentifier = identifier;
    }

    // The page is drawn with the state that this has now.
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    drawingBackend->drawPDFPage(identifier(), *cachedIdentifier, pageIndex, source, destination, includeLinks);
    return true;
}

}

#endif
