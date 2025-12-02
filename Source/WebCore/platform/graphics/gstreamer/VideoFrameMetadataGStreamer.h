/*
 * Copyright (C) 2021 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "VideoFrame.h"
#include "VideoFrameContentHint.h"
#include "VideoFrameMetadata.h"
#include "VideoFrameTimeMetadata.h"

// Modifies the buffer in-place.
void webkitGstBufferAddVideoFrameMetadata(GstBuffer*, std::optional<WebCore::VideoFrameTimeMetadata>, WebCore::VideoFrame::Rotation, bool isMirrored, WebCore::VideoFrameContentHint);

// Makes the buffer writable before modifying it.
WARN_UNUSED_RETURN GRefPtr<GstBuffer> webkitGstBufferSetVideoFrameMetadata(GRefPtr<GstBuffer>&&, std::optional<WebCore::VideoFrameTimeMetadata>, WebCore::VideoFrame::Rotation = WebCore::VideoFrame::Rotation::None, bool isMirrored = false, WebCore::VideoFrameContentHint = WebCore::VideoFrameContentHint::None);

void webkitGstTraceProcessingTimeForElement(GstElement*);
WebCore::VideoFrameMetadata webkitGstBufferGetVideoFrameMetadata(GstBuffer*);
std::pair<WebCore::VideoFrame::Rotation, bool> webkitGstBufferGetVideoRotation(GstBuffer*);

WebCore::VideoFrameContentHint webkitGstBufferGetContentHint(GstBuffer*);

MediaTime webkitGstBufferGetProcessingTime(GstBuffer*, GstElement*);

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
