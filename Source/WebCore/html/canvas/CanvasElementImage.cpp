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
#include "CanvasElementImage.h"

#include "HTMLCanvasElement.h"
#include "OffscreenCanvas.h"

namespace WebCore {

FloatSize CanvasElementSnapshot::sizeInCanvasGrid(const FloatSize& size, const IntSize& canvasSize) const
{
    // The canvas grid is stretched over the canvas's replaced content rect.
    auto scaleAxis = [](float length, int gridLength, float contentLength) -> float {
        if (!gridLength)
            return 0;
        if (contentLength <= 0)
            return length;
        return length * gridLength / contentLength;
    };
    return {
        scaleAxis(size.width(), canvasSize.width(), canvasContentSize.width()),
        scaleAxis(size.height(), canvasSize.height(), canvasContentSize.height()),
    };
}

Ref<CanvasElementImage> CanvasElementImage::create(CanvasElementSnapshot&& snapshot)
{
    return adoptRef(*new CanvasElementImage(WTF::move(snapshot)));
}

CanvasElementImage::CanvasElementImage(CanvasElementSnapshot&& snapshot)
    : m_snapshot(WTF::move(snapshot))
{
}

void CanvasElementImage::close()
{
    m_snapshot = std::nullopt;
}

// The canvas whose ElementImages can be drawn into `canvas`.
static std::optional<NodeIdentifier> elementImageCanvasIdentifier(CanvasBase& canvas)
{
    if (RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(canvas))
        return canvasElement->nodeIdentifier();
#if ENABLE(OFFSCREEN_CANVAS)
    if (RefPtr offscreenCanvas = dynamicDowncast<OffscreenCanvas>(canvas))
        return offscreenCanvas->placeholderCanvasIdentifier();
#endif
    return std::nullopt;
}

std::expected<CanvasElementSnapshot, ASCIILiteral> canvasElementSnapshotOrError(CanvasBase& canvas, const CanvasElementImageSource& source, ASCIILiteral missingSnapshotMessage)
{
    return WTF::switchOn(source,
        [&](const Ref<Element>& element) -> std::expected<CanvasElementSnapshot, ASCIILiteral> {
            if (RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(canvas)) {
                if (auto snapshot = canvasElement->canvasDrawableSnapshot(element.get()))
                    return WTF::move(*snapshot);
            }
            return makeUnexpected(missingSnapshotMessage);
        },
        [&](const Ref<CanvasElementImage>& elementImage) -> std::expected<CanvasElementSnapshot, ASCIILiteral> {
            auto* snapshot = elementImage->snapshot();
            if (!snapshot)
                return makeUnexpected("The ElementImage has been closed."_s);
            if (snapshot->canvasIdentifier != elementImageCanvasIdentifier(canvas))
                return makeUnexpected("The ElementImage was captured from a different canvas."_s);
            return *snapshot;
        }
    );
}

ExceptionOr<CanvasElementSnapshot> canvasElementSnapshot(CanvasBase& canvas, const CanvasElementImageSource& source, ASCIILiteral missingSnapshotMessage)
{
    auto snapshot = canvasElementSnapshotOrError(canvas, source, missingSnapshotMessage);
    if (!snapshot)
        return Exception { ExceptionCode::InvalidStateError, snapshot.error() };
    return WTF::move(*snapshot);
}

} // namespace WebCore
