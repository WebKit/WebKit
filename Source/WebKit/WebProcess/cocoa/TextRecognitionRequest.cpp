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
#include "TextRecognitionRequest.h"

#include "PlaybackSessionManager.h"
#include "WebPage.h"
#include <WebCore/HTMLVideoElement.h>
#include <wtf/RunLoop.h>

#if ENABLE(IMAGE_ANALYSIS)

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextRecognitionRequest);

TextRecognitionRequest::TextRecognitionRequest(WebPage& page, PlaybackSessionManager& manager)
    : m_page(page)
    , m_manager(manager)
{
}

TextRecognitionRequest::~TextRecognitionRequest()
{
    cancel();
}

void TextRecognitionRequest::requestTextRecognitionFor(HTMLMediaElementIdentifier identifier)
{
    RefPtr element = dynamicDowncast<HTMLVideoElement>(m_manager->mediaElementWithContextId(identifier));
    if (!element)
        return;

    if (!element->isInFullscreenOrPictureInPicture()) {
        if (identifier == m_identifier)
            cancel();
        return;
    }
    if (!element->paused() || element->seeking()) {
        cancel();
        return;
    }
    if (m_timer)
        m_timer->stop();
    m_identifier = identifier;
    m_timer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "TextRecognitionRequest"_s, [page = m_page, manager = m_manager, identifier] {
        if (!page)
            return;
        RefPtr element = dynamicDowncast<HTMLVideoElement>(manager->mediaElementWithContextId(identifier));
        if (!element)
            return;
        page->beginTextRecognitionForVideoInElementFullScreen(*element);
    });
    m_timer->startOneShot(250_ms);
}

void TextRecognitionRequest::cancel()
{
    if (auto timer = std::exchange(m_timer, { }))
        timer->stop();

    m_identifier.reset();
    if (RefPtr page = m_page.get())
        page->cancelTextRecognitionForVideoInElementFullScreen();
}

} // namespace WebKit

#endif
