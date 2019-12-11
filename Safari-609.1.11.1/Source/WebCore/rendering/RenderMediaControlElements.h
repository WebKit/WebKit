/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc.
 * All rights reserved.
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

#if ENABLE(VIDEO)

#include "RenderBlockFlow.h"
#include "RenderFlexibleBox.h"

namespace WebCore {

class RenderMediaVolumeSliderContainer final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderMediaVolumeSliderContainer);
public:
    RenderMediaVolumeSliderContainer(Element&, RenderStyle&&);

private:
    void layout() override;
};

// ----------------------------

class RenderMediaControlTimelineContainer final : public RenderFlexibleBox {
    WTF_MAKE_ISO_ALLOCATED(RenderMediaControlTimelineContainer);
public:
    RenderMediaControlTimelineContainer(Element&, RenderStyle&&);

private:
    void layout() override;
    bool isFlexibleBoxImpl() const override { return true; }
};

// ----------------------------

#if ENABLE(VIDEO_TRACK)

class RenderTextTrackContainerElement final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED(RenderTextTrackContainerElement);
public:
    RenderTextTrackContainerElement(Element&, RenderStyle&&);

private:
    void layout() override;
};

#endif // ENABLE(VIDEO_TRACK)

} // namespace WebCore

#endif // ENABLE(VIDEO)
