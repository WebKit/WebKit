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

#pragma once

#include "APIObject.h"
#include <WebCore/ElementTargetingTypes.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class ShareableBitmapHandle;
}

namespace WebKit {
class WebPageProxy;
}

namespace API {

class FrameTreeNode;

class TargetedElementInfo final : public ObjectImpl<Object::Type::TargetedElementInfo> {
public:
    static Ref<TargetedElementInfo> create(WebKit::WebPageProxy& page, WebCore::TargetedElementInfo&& info)
    {
        return adoptRef(*new TargetedElementInfo(page, WTFMove(info)));
    }

    explicit TargetedElementInfo(WebKit::WebPageProxy&, WebCore::TargetedElementInfo&&);

    WebCore::RectEdges<bool> offsetEdges() const { return m_info.offsetEdges; }

    const WTF::String& renderedText() const { return m_info.renderedText; }
    const WTF::String& searchableText() const { return m_info.searchableText; }
    const WTF::String& screenReaderText() const { return m_info.screenReaderText; }
    const Vector<Vector<WTF::String>>& selectors() const { return m_info.selectors; }
    WebCore::PositionType positionType() const { return m_info.positionType; }
    WebCore::FloatRect boundsInRootView() const { return m_info.boundsInRootView; }
    WebCore::FloatRect boundsInWebView() const;
    WebCore::FloatRect boundsInClientCoordinates() const { return m_info.boundsInClientCoordinates; }

    bool isNearbyTarget() const { return m_info.isNearbyTarget; }
    bool isPseudoElement() const { return m_info.isPseudoElement; }
    bool isInShadowTree() const { return m_info.isInShadowTree; }
    bool isInVisibilityAdjustmentSubtree() const { return m_info.isInVisibilityAdjustmentSubtree; }
    bool hasLargeReplacedDescendant() const { return m_info.hasLargeReplacedDescendant; }
    bool hasAudibleMedia() const { return m_info.hasAudibleMedia; }

    const HashSet<WTF::URL>& mediaAndLinkURLs() const { return m_info.mediaAndLinkURLs; }

    void childFrames(CompletionHandler<void(Vector<Ref<FrameTreeNode>>&&)>&&) const;

    bool isSameElement(const TargetedElementInfo&) const;

    WebCore::ElementIdentifier elementIdentifier() const { return m_info.elementIdentifier; }
    WebCore::ScriptExecutionContextIdentifier documentIdentifier() const { return m_info.documentIdentifier; }

    void takeSnapshot(CompletionHandler<void(std::optional<WebCore::ShareableBitmapHandle>&&)>&&);

private:
    WebCore::TargetedElementInfo m_info;
    WeakPtr<WebKit::WebPageProxy> m_page;
};

} // namespace API
