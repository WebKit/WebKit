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

#include <WebCore/FrameIdentifier.h>
#include <WebCore/IntSize.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/RemoteFrameLayoutInfo.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

// Geometry for a frame that is sent from a parent frame process to remote child frame processes
// when Site Isolation is enabled. The data in this struct generally doesn't change while scrolling,
// which enables optimizations like not sending this payload if it hasn't changed.
struct FrameGeometrySyncData {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(FrameGeometrySyncData, WEBCORE_EXPORT);

    IntSize contentsSize;
    HashMap<FrameIdentifier, Ref<RemoteFrameLayoutInfo>> childrenFrameLayoutInfo;
};

WEBCORE_EXPORT bool operator==(const FrameGeometrySyncData&, const FrameGeometrySyncData&);

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FrameGeometrySyncData&);

// Unlike FrameGeometrySyncData, the data in this struct does change while scrolling. However, we
// suppress broadcasting this if all remote frames are offscreen for consecutive rendering updates.
struct FrameViewportInfo {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(FrameViewportInfo, WEBCORE_EXPORT);

    LayoutRect layoutViewportRect;
    ScrollPosition scrollPosition;

    friend bool operator==(const FrameViewportInfo&, const FrameViewportInfo&) = default;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FrameViewportInfo&);

} // namespace WebCore
