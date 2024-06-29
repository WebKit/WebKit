/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "APITargetedElementInfo.h"

#include "APIFrameTreeNode.h"
#include "FrameTreeNodeData.h"
#include "PageClient.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include <WebCore/ShareableBitmap.h>
#include <wtf/Box.h>
#include <wtf/CallbackAggregator.h>

namespace API {
using namespace WebKit;

TargetedElementInfo::TargetedElementInfo(WebPageProxy& page, WebCore::TargetedElementInfo&& info)
    : m_info(WTFMove(info))
    , m_page(page)
{
}

bool TargetedElementInfo::isSameElement(const TargetedElementInfo& other) const
{
    return m_info.elementIdentifier == other.m_info.elementIdentifier
        && m_info.documentIdentifier == other.m_info.documentIdentifier
        && m_page == other.m_page;
}

WebCore::FloatRect TargetedElementInfo::boundsInWebView() const
{
    RefPtr page = m_page.get();
    if (!page)
        return { };
    return page->pageClient().rootViewToWebView(boundsInRootView());
}

void TargetedElementInfo::childFrames(CompletionHandler<void(Vector<Ref<FrameTreeNode>>&&)>&& completion) const
{
    RefPtr page = m_page.get();
    if (!page)
        return completion({ });

    auto aggregateData = Box<Vector<FrameTreeNodeData>>::create();
    auto aggregator = CallbackAggregator::create([page, aggregateData, completion = WTFMove(completion)]() mutable {
        completion(WTF::map(WTFMove(*aggregateData), [&](auto&& data) {
            return FrameTreeNode::create(WTFMove(data), *page);
        }));
    });

    for (auto identifier : m_info.childFrameIdentifiers) {
        RefPtr frame = WebFrameProxy::webFrame(identifier);
        if (!frame)
            continue;

        if (frame->page() != page)
            continue;

        frame->getFrameInfo([aggregator, aggregateData](auto&& data) {
            aggregateData->append(WTFMove(data));
        });
    }
}

void TargetedElementInfo::takeSnapshot(CompletionHandler<void(std::optional<WebCore::ShareableBitmapHandle>&&)>&& completion)
{
    RefPtr page = m_page.get();
    if (!page)
        return completion({ });

    page->takeSnapshotForTargetedElement(*this, WTFMove(completion));
}

} // namespace API
