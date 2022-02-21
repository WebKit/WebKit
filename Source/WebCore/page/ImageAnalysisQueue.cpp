/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ImageAnalysisQueue.h"

#if ENABLE(IMAGE_ANALYSIS)

#include "Chrome.h"
#include "ChromeClient.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLImageElement.h"
#include "ImageOverlay.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "Timer.h"

namespace WebCore {

static constexpr unsigned maximumPendingImageAnalysisCount = 5;
static constexpr float minimumWidthForAnalysis = 20;
static constexpr float minimumHeightForAnalysis = 20;
static constexpr Seconds resumeProcessingDelay = 100_ms;

ImageAnalysisQueue::ImageAnalysisQueue(Page& page)
    : m_page(page)
    , m_resumeProcessingTimer(*this, &ImageAnalysisQueue::resumeProcessing)
{
}

ImageAnalysisQueue::~ImageAnalysisQueue() = default;

void ImageAnalysisQueue::enqueueIfNeeded(HTMLImageElement& element)
{
    if (!is<RenderImage>(element.renderer()))
        return;

    auto& renderer = downcast<RenderImage>(*element.renderer());
    auto* cachedImage = renderer.cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;

    if (renderer.size().width() < minimumWidthForAnalysis || renderer.size().height() < minimumHeightForAnalysis)
        return;

    if (!m_queuedElements.add(element).isNewEntry)
        return;

    Ref view = renderer.view().frameView();
    m_queue.enqueue({
        element,
        renderer.isVisibleInDocumentRect(view->windowToContents(view->windowClipRect())) ? Priority::High : Priority::Low,
        nextTaskNumber()
    });
    resumeProcessingSoon();
}

void ImageAnalysisQueue::resumeProcessingSoon()
{
    if (m_queue.isEmpty() || m_resumeProcessingTimer.isActive())
        return;

    m_resumeProcessingTimer.startOneShot(resumeProcessingDelay);
}

void ImageAnalysisQueue::enqueueAllImages(Document& document, const String& identifier)
{
    if (!m_page)
        return;

    if (m_identifier != identifier) {
        clear();
        m_identifier = identifier;
    }

    for (auto& image : descendantsOfType<HTMLImageElement>(document))
        enqueueIfNeeded(image);
}

void ImageAnalysisQueue::resumeProcessing()
{
    if (!m_page)
        return;

    while (!m_queue.isEmpty() && m_pendingRequestCount < maximumPendingImageAnalysisCount) {
        RefPtr element = m_queue.dequeue().element.get();
        if (!element || !element->isConnected())
            continue;

        m_pendingRequestCount++;
        m_page->resetTextRecognitionResult(*element);
        m_page->chrome().client().requestTextRecognition(*element, m_identifier, [this, page = m_page] (auto&&) {
            if (!page || page->imageAnalysisQueueIfExists() != this)
                return;

            if (m_pendingRequestCount)
                m_pendingRequestCount--;

            resumeProcessingSoon();
        });
    }
}

void ImageAnalysisQueue::clear()
{
    // FIXME: This should cancel pending requests in addition to emptying the task queue.
    m_pendingRequestCount = 0;
    m_resumeProcessingTimer.stop();
    m_queue = { };
    m_queuedElements.clear();
    m_identifier = { };
    m_currentTaskNumber = 0;
}

} // namespace WebCore

#endif // ENABLE(IMAGE_ANALYSIS)
