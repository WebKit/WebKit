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

#include "LocalFrameView.h"
#include "RemoteFrameView.h"

namespace WebCore {

RemoteFrameGeometryTransformer::RemoteFrameGeometryTransformer(Ref<RemoteFrameView>&& remoteView, Ref<LocalFrameView>&& localView, FrameIdentifier remoteFrameID)
    : m_remoteView(WTFMove(remoteView))
    , m_localView(WTFMove(localView))
    , m_remoteFrameID(remoteFrameID) { }

RemoteFrameGeometryTransformer::~RemoteFrameGeometryTransformer() = default;

RemoteFrameGeometryTransformer::RemoteFrameGeometryTransformer(RemoteFrameGeometryTransformer&&) = default;

RemoteFrameGeometryTransformer& RemoteFrameGeometryTransformer::operator=(RemoteFrameGeometryTransformer&&) = default;

IntPoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(IntPoint pointInContents) const
{
    return Ref { m_remoteView }->rootViewToContents(Ref { m_localView }->contentsToRootView(pointInContents));
}

FloatPoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(FloatPoint pointInContents) const
{
    return Ref { m_remoteView }->rootViewToContents(Ref { m_localView }->contentsToRootView(pointInContents));
}

DoublePoint RemoteFrameGeometryTransformer::transformToRemoteFrameCoordinates(DoublePoint pointInContents) const
{
    return Ref { m_remoteView }->rootViewToContents(Ref { m_localView }->contentsToRootView(pointInContents));
}

} // namespace WebCore
