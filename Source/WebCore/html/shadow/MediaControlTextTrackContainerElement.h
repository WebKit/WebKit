/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO)

#include "GenericTaskQueue.h"
#include "HTMLDivElement.h"
#include "MediaControllerInterface.h"
#include "TextTrackRepresentation.h"
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLMediaElement;
class VTTCue;

class MediaControlTextTrackContainerElement final
    : public HTMLDivElement
    , public TextTrackRepresentationClient
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(MediaControlTextTrackContainerElement);
public:
    static Ref<MediaControlTextTrackContainerElement> create(Document&, HTMLMediaElement&);

    enum class ForceUpdate { Yes, No };
    void updateSizes(ForceUpdate force = ForceUpdate::No);
    void updateDisplay();

    void updateTextTrackRepresentationImageIfNeeded();

    void enteredFullscreen();
    void exitedFullscreen();

private:
    explicit MediaControlTextTrackContainerElement(Document&, HTMLMediaElement&);

    // Element
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

    // TextTrackRepresentationClient
    RefPtr<Image> createTextTrackRepresentationImage() override;
    void textTrackRepresentationBoundsChanged(const IntRect&) override;

    void updateTextTrackRepresentationIfNeeded();
    void clearTextTrackRepresentation();

    bool updateVideoDisplaySize();
    void updateActiveCuesFontSize();
    void updateTextStrokeStyle();
    void processActiveVTTCue(VTTCue&);
    void updateTextTrackStyle();

    void hide();
    void show();
    bool isShowing() const;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    const void* logIdentifier() const final;
    WTFLogChannel& logChannel() const final;
    const char* logClassName() const final { return "MediaControlTextTrackContainerElement"; }
    mutable RefPtr<Logger> m_logger;
    mutable const void* m_logIdentifier { nullptr };
#endif

    std::unique_ptr<TextTrackRepresentation> m_textTrackRepresentation;

    GenericTaskQueue<Timer> m_taskQueue;
    WeakPtr<HTMLMediaElement> m_mediaElement;
    IntRect m_videoDisplaySize;
    int m_fontSize { 0 };
    bool m_fontSizeIsImportant { false };
    bool m_needsGenerateTextTrackRepresentation { false };
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
