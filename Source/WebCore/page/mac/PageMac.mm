/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Page.h"

#import "DocumentLoader.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameTree.h"
#import "LayoutTreeBuilder.h"
#import "Logging.h"
#import "RenderObject.h"
#import "SVGDocument.h"
#import <pal/Logging.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThreadInternal.h"
#endif

namespace WebCore {

void Page::platformInitialize()
{
#if PLATFORM(IOS_FAMILY)
    addSchedulePair(SchedulePair::create(WebThreadNSRunLoop(), kCFRunLoopCommonModes));
#else
    addSchedulePair(SchedulePair::create([[NSRunLoop currentRunLoop] getCFRunLoop], kCFRunLoopCommonModes));
#endif

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
#if ENABLE(TREE_DEBUGGING)
        PAL::registerNotifyCallback("com.apple.WebKit.showRenderTree", printRenderTreeForLiveDocuments);
        PAL::registerNotifyCallback("com.apple.WebKit.showLayerTree", printLayerTreeForLiveDocuments);
        PAL::registerNotifyCallback("com.apple.WebKit.showGraphicsLayerTree", printGraphicsLayerTreeForLiveDocuments);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
        PAL::registerNotifyCallback("com.apple.WebKit.showLayoutTree", Layout::printLayoutTreeForLiveDocuments);
#endif
#endif // ENABLE(TREE_DEBUGGING)

        PAL::registerNotifyCallback("com.apple.WebKit.showAllDocuments", [] {
            unsigned numPages = 0;
            Page::forEachPage([&numPages](Page&) {
                ++numPages;
            });

            WTFLogAlways("%u live pages:", numPages);

            Page::forEachPage([](Page& page) {
                const auto* mainFrameDocument = page.mainFrame().document();
                WTFLogAlways("Page %p with main document %p %s", &page, mainFrameDocument, mainFrameDocument ? mainFrameDocument->url().string().utf8().data() : "");
            });

            WTFLogAlways("%u live documents:", Document::allDocuments().size());
            for (const auto* document : Document::allDocuments()) {
                const char* documentType = is<SVGDocument>(document) ? "SVGDocument" : "Document";
                WTFLogAlways("%s %p %llu (refCount %d, referencingNodeCount %d) %s", documentType, document, document->identifier().toUInt64(), document->refCount(), document->referencingNodeCount(), document->url().string().utf8().data());
            }
        });
    });
}

void Page::addSchedulePair(Ref<SchedulePair>&& pair)
{
    if (!m_scheduledRunLoopPairs)
        m_scheduledRunLoopPairs = makeUnique<SchedulePairHashSet>();
    m_scheduledRunLoopPairs->add(pair.ptr());

    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (DocumentLoader* documentLoader = frame->loader().documentLoader())
            documentLoader->schedule(pair);
        if (DocumentLoader* documentLoader = frame->loader().provisionalDocumentLoader())
            documentLoader->schedule(pair);
    }

    // FIXME: make SharedTimerMac use these SchedulePairs.
}

void Page::removeSchedulePair(Ref<SchedulePair>&& pair)
{
    ASSERT(m_scheduledRunLoopPairs);
    if (!m_scheduledRunLoopPairs)
        return;

    m_scheduledRunLoopPairs->remove(pair.ptr());

    for (Frame* frame = &m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (DocumentLoader* documentLoader = frame->loader().documentLoader())
            documentLoader->unschedule(pair);
        if (DocumentLoader* documentLoader = frame->loader().provisionalDocumentLoader())
            documentLoader->unschedule(pair);
    }
}

} // namespace
