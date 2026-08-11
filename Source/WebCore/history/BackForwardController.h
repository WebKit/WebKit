/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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

#include <WebCore/BackForwardFrameItemIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class BackForwardClient;
class HistoryItem;
class Page;

class BackForwardController final : public CanMakeCheckedPtr<BackForwardController> {
    WTF_MAKE_TZONE_ALLOCATED(BackForwardController);
    WTF_MAKE_NONCOPYABLE(BackForwardController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BackForwardController);
public:
    BackForwardController(Page&, Ref<BackForwardClient>&&);
    ~BackForwardController();

    BackForwardClient& client() { return m_client.get(); }
    const BackForwardClient& client() const { return m_client.get(); }

    WEBCORE_EXPORT bool canGoBackOrForward(int distance) const;
    void goBackOrForward(int distance);

    WEBCORE_EXPORT bool goBack();
    WEBCORE_EXPORT bool goForward();

    void addItem(Ref<HistoryItem>&&);
    void setChildItem(BackForwardFrameItemIdentifier, Ref<HistoryItem>&&);
    void setCurrentItem(HistoryItem&);

    unsigned count() const;
    WEBCORE_EXPORT unsigned backCount() const;
    WEBCORE_EXPORT unsigned forwardCount() const;

    bool containsItem(const HistoryItem&) const;

    WEBCORE_EXPORT RefPtr<HistoryItem> backItem(std::optional<FrameIdentifier> = std::nullopt);
    WEBCORE_EXPORT RefPtr<HistoryItem> currentItem(std::optional<FrameIdentifier> = std::nullopt);
    WEBCORE_EXPORT RefPtr<HistoryItem> forwardItem(std::optional<FrameIdentifier> = std::nullopt);
    WEBCORE_EXPORT RefPtr<HistoryItem> itemAtIndex(int, std::optional<FrameIdentifier> = std::nullopt);
    Vector<Ref<HistoryItem>> allItems(std::optional<FrameIdentifier> = std::nullopt);

    void close();

private:
    WeakRef<Page> m_page;
    const Ref<BackForwardClient> m_client;
};

} // namespace WebCore
