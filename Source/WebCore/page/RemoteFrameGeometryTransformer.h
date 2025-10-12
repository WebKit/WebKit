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

#include <WebCore/FrameIdentifier.h>

namespace WebCore {

class DoublePoint;
class IntPoint;
class FloatPoint;
class LocalFrameView;
class RemoteFrameView;

class RemoteFrameGeometryTransformer {
public:
    WEBCORE_EXPORT RemoteFrameGeometryTransformer(Ref<RemoteFrameView>&&, Ref<LocalFrameView>&&, FrameIdentifier);
    WEBCORE_EXPORT RemoteFrameGeometryTransformer(RemoteFrameGeometryTransformer&&);
    RemoteFrameGeometryTransformer(const RemoteFrameGeometryTransformer&) = delete;
    WEBCORE_EXPORT RemoteFrameGeometryTransformer& operator=(RemoteFrameGeometryTransformer&&);
    RemoteFrameGeometryTransformer& operator=(const RemoteFrameGeometryTransformer&) = delete;
    WEBCORE_EXPORT ~RemoteFrameGeometryTransformer();

    WEBCORE_EXPORT IntPoint transformToRemoteFrameCoordinates(IntPoint pointInContents) const;
    WEBCORE_EXPORT FloatPoint transformToRemoteFrameCoordinates(FloatPoint pointInContents) const;
    WEBCORE_EXPORT DoublePoint transformToRemoteFrameCoordinates(DoublePoint pointInContents) const;
    FrameIdentifier remoteFrameID() const { return m_remoteFrameID; }

private:
    Ref<RemoteFrameView> m_remoteView;
    Ref<LocalFrameView> m_localView;
    FrameIdentifier m_remoteFrameID;
};

} // namespace WebCore
