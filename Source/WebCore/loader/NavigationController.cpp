/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "NavigationController.h"

#include "EventNames.h"
#include "JavaScriptCore/JSCJSValue.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "NavigateEvent.h"
#include "Navigation.h"
#include "NavigationDestination.h"
#include "NavigationNavigationType.h"
#include "ResourceRequest.h"
#include <optional>
#include <wtf/UUID.h>

namespace WebCore {

FrameLoader::NavigationController::NavigationController(LocalFrame& frame)
    : m_frame(frame)
{
}

FrameLoader::NavigationController::~NavigationController() = default;

void FrameLoader::NavigationController::dispatchNavigationEvent(const RefPtr<NavigationEntry> entry, const URL& oldURL, const Document& document, NavigationNavigationType type)
{
    URL newURL { entry->url() };
    bool isSameDocument = equalIgnoringFragmentIdentifier(newURL, oldURL);
    bool isHashChange = isSameDocument && !equalRespectingNullity(newURL.fragmentIdentifier(), oldURL.fragmentIdentifier());
    bool isTraversal = type == NavigationNavigationType::Traverse;
    size_t index = std::distance(m_navigations.begin(), m_navigations.find(entry->key()));

    auto destination = NavigationDestination::create(*document.domWindow(), entry->url(), isTraversal ? entry->id() : ""_s, isTraversal ? entry->key() : ""_s, isTraversal ? index : -1);
    auto init = NavigateEvent::Init {
        { false, true, false },
        type,
        WTFMove(destination),
        nullptr,
        nullptr,
        ""_s,
        JSC::jsUndefined(),
        true,
        false,
        isHashChange,
        false,
    };
    auto event = NavigateEvent::create({ "navigate"_s }, init);
    m_frame.window()->navigation().dispatchEvent(event);
}

std::tuple<const RefPtr<NavigationEntry>, int64_t> FrameLoader::NavigationController::currentEntry() const
{
    if (!m_currentEntry)
        return { nullptr, -1 };

    auto iter = m_navigations.find(m_currentEntry->key());
    int64_t index = iter == m_navigations.end() ? -1 : std::distance(m_navigations.begin(), iter);

    return { m_currentEntry, index };
}

RefPtr<NavigationEntry> FrameLoader::NavigationController::addEntry(const String& url)
{
    auto key = createVersion4UUIDString();
    auto entry = NavigationEntry::create(url, key);
    m_navigations.insert({ key, entry });
    m_currentEntry = entry;
    return entry;
}

RefPtr<NavigationEntry> NavigationEntry::create(const String& url, const String& key)
{
    return adoptRef(*new NavigationEntry(url, key));
}

NavigationEntry::NavigationEntry(const String& url, const String& key)
    : m_url(url)
    , m_id(createVersion4UUIDString())
    , m_key(key)
{
}

} // namespace WebCore
