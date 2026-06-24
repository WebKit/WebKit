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

#pragma once

#include <WebCore/LayoutRect.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

enum class FrameOwnerElementAppearance : uint8_t {
    // Whether the used color scheme of the frame embedder is dark or not.
    // This could either come from `color-scheme` CSS property or system preference.
    IsDark = 1 << 0,

    // Whether the color scheme is explicitly set using `color-scheme` CSS property or not.
    ExplicitlySet = 1 << 1
};

// Collection of style/layout info regarding a (potentially remote) frame.
// This is synchronized from LocalFrame in one process to RemoteFrames
// in other processes using FrameTreeSyncData.
class RemoteFrameLayoutInfo : public RefCounted<RemoteFrameLayoutInfo> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(RemoteFrameLayoutInfo, WEBCORE_EXPORT);

public:
    WEBCORE_EXPORT static Ref<RemoteFrameLayoutInfo> create(std::optional<LayoutRect>, TransformationMatrix, TransformationMatrix, float, LayoutPoint, OptionSet<FrameOwnerElementAppearance>);

    std::optional<LayoutRect> visibleRectInParent() const { return m_visibleRectInParent; }
    const TransformationMatrix& childFrameOwnerToRootContentTransform() const { return m_childFrameOwnerToRootContentTransform; }
    const TransformationMatrix& absoluteToChildFrameOwnerLocalTransform() const { return m_absoluteToChildFrameOwnerLocalTransform; }
    float usedZoom() const { return m_usedZoom; }
    LayoutPoint contentBoxLocation() const { return m_contentBoxLocation; }
    OptionSet<FrameOwnerElementAppearance> ownerElementAppearance() const { return m_ownerElementAppearance; }

private:
    RemoteFrameLayoutInfo(std::optional<LayoutRect>, TransformationMatrix, TransformationMatrix, float, LayoutPoint, OptionSet<FrameOwnerElementAppearance>);

    // Rectangle of the visible portion of the frame in its parent frame,
    // in the coordinate space of the document of the parent frame.
    std::optional<LayoutRect> m_visibleRectInParent;

    // The transformation matrix to project from the frame owner's
    // coordinate space to its RenderView's (root) coordinate space.
    // Note: this DOES NOT include the frame scale transform on the
    // RenderView.
    TransformationMatrix m_childFrameOwnerToRootContentTransform;

    // The transformation matrix to project from current frame's
    // absolute coordinate to the child frame owner's local coordinate.
    TransformationMatrix m_absoluteToChildFrameOwnerLocalTransform;

    // Style::ComputedStyle::usedZoom of the owner renderer of the frame.
    float m_usedZoom;

    // The offset of the content box of the frame's owner element
    // from its border box.
    LayoutPoint m_contentBoxLocation;

    OptionSet<FrameOwnerElementAppearance> m_ownerElementAppearance;
};

};
