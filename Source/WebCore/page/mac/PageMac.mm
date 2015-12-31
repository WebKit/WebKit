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
#import "FrameLoader.h"
#import "FrameTree.h"
#import "MainFrame.h"

#if PLATFORM(IOS)
#import "WebCoreThread.h"
#endif

namespace WebCore {

void Page::platformInitialize()
{
#if PLATFORM(IOS)
#if USE(CFNETWORK)
    addSchedulePair(SchedulePair::create(WebThreadRunLoop(), kCFRunLoopCommonModes));
#else
    addSchedulePair(SchedulePair::create(WebThreadNSRunLoop(), kCFRunLoopCommonModes));
#endif // USE(CFNETWORK)
#else
    addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));
#endif
}

void Page::addSchedulePair(Ref<SchedulePair>&& pair)
{
    if (!m_scheduledRunLoopPairs)
        m_scheduledRunLoopPairs = std::make_unique<SchedulePairHashSet>();
    m_scheduledRunLoopPairs->add(pair.ptr());

#if !USE(CFNETWORK)
    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (DocumentLoader* documentLoader = frame->loader().documentLoader())
            documentLoader->schedule(pair);
        if (DocumentLoader* documentLoader = frame->loader().provisionalDocumentLoader())
            documentLoader->schedule(pair);
    }
#endif

    // FIXME: make SharedTimerMac use these SchedulePairs.
}

void Page::removeSchedulePair(Ref<SchedulePair>&& pair)
{
    ASSERT(m_scheduledRunLoopPairs);
    if (!m_scheduledRunLoopPairs)
        return;

    m_scheduledRunLoopPairs->remove(pair.ptr());

#if !USE(CFNETWORK)
    for (Frame* frame = m_mainFrame.get(); frame; frame = frame->tree().traverseNext()) {
        if (DocumentLoader* documentLoader = frame->loader().documentLoader())
            documentLoader->unschedule(pair);
        if (DocumentLoader* documentLoader = frame->loader().provisionalDocumentLoader())
            documentLoader->unschedule(pair);
    }
#endif
}

} // namespace
