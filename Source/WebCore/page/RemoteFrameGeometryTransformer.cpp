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

#include "config.h"
#include "RemoteFrameGeometryTransformer.h"

#include "DoublePoint.h"
#include "LocalFrameView.h"
#include "RemoteFrame.h"
#include "RemoteFrameView.h"

namespace WebCore {

RemoteFrameGeometryTransformer::RemoteFrameGeometryTransformer(Ref<RemoteFrameView>&& remoteView, Ref<LocalFrameView>&& localView, FrameIdentifier remoteFrameID)
    : m_remoteView(WTF::move(remoteView))
    , m_localView(WTF::move(localView))
    , m_remoteFrameID(remoteFrameID) { }

RemoteFrameGeometryTransformer::~RemoteFrameGeometryTransformer() = default;

RemoteFrameGeometryTransformer::RemoteFrameGeometryTransformer(RemoteFrameGeometryTransformer&&) = default;

RemoteFrameGeometryTransformer& RemoteFrameGeometryTransformer::operator=(RemoteFrameGeometryTransformer&&) = default;

// The returned point is in the remote frame's root-view coordinates (i.e. relative to the remote
// frame's origin, without the remote frame's own scroll offset applied). Consumers deliver it into
// the remote frame's process as a root-view point, where it is converted to contents coordinates
// (re-applying the remote frame's scroll) during hit-testing.
//
// The mapping applies the CSS transform on the remote frame's owner element -- obtained from the
// local (parent) frame view, whose owner renderer lives in this process -- so a scaled, rotated, or
// translated iframe maps its coordinates correctly. A plain translation offset could not represent a
// scale.
FloatPoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(FloatPoint pointInContents) const
{
    Ref localView = m_localView;
    Ref remoteFrame = Ref { m_remoteView }->frame();
    auto local = localView->absoluteToChildFrameOwnerLocalTransform(remoteFrame).projectPoint(pointInContents);
    FloatPoint contentBoxLocation = localView->childFrameOwnerContentBoxLocation(remoteFrame);
    local.moveBy(-contentBoxLocation);
    return local;
}

IntPoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(IntPoint pointInContents) const
{
    return roundedIntPoint(transformToRemoteFrameCoordinates(FloatPoint { pointInContents }));
}

DoublePoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(DoublePoint pointInContents) const
{
    auto transformed = transformToRemoteFrameCoordinates(FloatPoint(pointInContents.x(), pointInContents.y()));
    return { transformed.x(), transformed.y() };
}

DoublePoint RemoteFrameGeometryTransformer::transformRootViewPointToRemoteFrameCoordinates(DoublePoint pointInRootView) const
{
    return Ref { m_remoteView }->convertFromRootView(pointInRootView);
}

} // namespace WebCore
