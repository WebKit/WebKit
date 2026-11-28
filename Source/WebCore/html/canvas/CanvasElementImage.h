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

#pragma once

#include <WebCore/DisplayList.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntSize.h>
#include <WebCore/NodeIdentifier.h>
#include <expected>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

class CanvasBase;
class Element;

struct CanvasElementSnapshot {
    Ref<const DisplayList::DisplayList> displayList;
    // The bounds of the recorded display list.
    FloatSize size;
    // The drawable element's border box, and the canvas's replaced content rect (which its pixel grid
    // fills), in layout units, as they were when the snapshot was painted.
    FloatSize borderBoxSize;
    FloatSize canvasContentSize;
    // The canvas the snapshot was painted for.
    NodeIdentifier canvasIdentifier;

    // Maps a size in the snapshot's layout units to the pixel grid of a canvas whose width and height are `canvasSize`.
    FloatSize sizeInCanvasGrid(const FloatSize&, const IntSize& canvasSize) const;
    // https://github.com/WICG/html-in-canvas: the default size of the element in canvas grid coordinates.
    // drawElementImage() draws at this size when it is not given a destination size.
    FloatSize defaultSize(const IntSize& canvasSize) const { return sizeInCanvasGrid(borderBoxSize, canvasSize); }
};

class CanvasElementImage : public RefCounted<CanvasElementImage> {
public:
    NODELETE static Ref<CanvasElementImage> create(CanvasElementSnapshot&&);

    void close();

    // Null once close() has released the snapshot.
    const CanvasElementSnapshot* snapshot() const { return m_snapshot ? &*m_snapshot : nullptr; }

private:
    CanvasElementImage(CanvasElementSnapshot&&);

    std::optional<CanvasElementSnapshot> m_snapshot;
};

using CanvasElementImageSource = Variant<Ref<Element>, Ref<CanvasElementImage>>;

// The snapshot that drawing `source` into `canvas` uses, or an error message if it cannot be drawn
// there: an Element without a painted snapshot in `canvas` (reported with `missingSnapshotMessage`),
// a closed ElementImage, or one captured from another canvas.
std::expected<CanvasElementSnapshot, ASCIILiteral> canvasElementSnapshotOrError(CanvasBase&, const CanvasElementImageSource&, ASCIILiteral missingSnapshotMessage);
ExceptionOr<CanvasElementSnapshot> canvasElementSnapshot(CanvasBase&, const CanvasElementImageSource&, ASCIILiteral missingSnapshotMessage);

} // namespace WebCore
