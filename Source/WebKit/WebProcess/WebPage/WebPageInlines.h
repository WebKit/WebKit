/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "WebPage.h"

#include "WebPageProxyIdentifier.h"
#include "WebUserContentController.h"
#include <WebCore/ActivityState.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/Page.h>

namespace WebKit {

inline WebCore::IntRect WebPage::bounds() const
{
    return WebCore::IntRect(WebCore::IntPoint(), size());
}

inline StorageNamespaceIdentifier WebPage::sessionStorageNamespaceIdentifier() const
{
    return LegacyNullableObjectIdentifier<StorageNamespaceIdentifierType>(m_webPageProxyIdentifier.toUInt64());
}

inline void WebPage::setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds limit)
{
    m_page->setDOMTimerAlignmentIntervalIncreaseLimit(limit);
}

inline bool WebPage::isVisible() const
{
    return m_activityState.contains(WebCore::ActivityState::IsVisible);
}

inline bool WebPage::isVisibleOrOccluded() const
{
    return m_activityState.contains(WebCore::ActivityState::IsVisibleOrOccluded);
}

inline UserContentControllerIdentifier WebPage::userContentControllerIdentifier() const
{
    return m_userContentController->identifier();
}

} // namespace WebKit
