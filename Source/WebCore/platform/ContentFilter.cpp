/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "ContentFilter.h"

#if ENABLE(CONTENT_FILTERING)

#include "NetworkExtensionContentFilter.h"
#include "ParentalControlsContentFilter.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WebCore {

Vector<ContentFilter::Type>& ContentFilter::types()
{
    static NeverDestroyed<Vector<ContentFilter::Type>> types {
        Vector<ContentFilter::Type> {
            type<ParentalControlsContentFilter>(),
#if HAVE(NETWORK_EXTENSION)
            type<NetworkExtensionContentFilter>()
#endif
        }
    };
    return types;
}

class ContentFilterCollection final : public ContentFilter {
public:
    using Container = Vector<std::unique_ptr<ContentFilter>>;

    explicit ContentFilterCollection(Container);

    void addData(const char* data, int length) override;
    void finishedAddingData() override;
    bool needsMoreData() const override;
    bool didBlockData() const override;
    const char* getReplacementData(int& length) const override;
    ContentFilterUnblockHandler unblockHandler() const override;

private:
    Container m_contentFilters;
};

std::unique_ptr<ContentFilter> ContentFilter::createIfNeeded(const ResourceResponse& response)
{
    ContentFilterCollection::Container filters;
    for (auto& type : types()) {
        if (type.canHandleResponse(response))
            filters.append(type.create(response));
    }

    if (filters.isEmpty())
        return nullptr;

    return std::make_unique<ContentFilterCollection>(WTF::move(filters));
}

ContentFilterCollection::ContentFilterCollection(Container contentFilters)
    : m_contentFilters { WTF::move(contentFilters) }
{
    ASSERT(!m_contentFilters.isEmpty());
}

void ContentFilterCollection::addData(const char* data, int length)
{
    ASSERT(needsMoreData());

    for (auto& contentFilter : m_contentFilters)
        contentFilter->addData(data, length);
}
    
void ContentFilterCollection::finishedAddingData()
{
    ASSERT(needsMoreData());

    for (auto& contentFilter : m_contentFilters)
        contentFilter->finishedAddingData();

    ASSERT(!needsMoreData());
}

bool ContentFilterCollection::needsMoreData() const
{
    for (auto& contentFilter : m_contentFilters) {
        if (contentFilter->needsMoreData())
            return true;
    }

    return false;
}

bool ContentFilterCollection::didBlockData() const
{
    for (auto& contentFilter : m_contentFilters) {
        if (contentFilter->didBlockData())
            return true;
    }

    return false;
}

const char* ContentFilterCollection::getReplacementData(int& length) const
{
    ASSERT(!needsMoreData());

    for (auto& contentFilter : m_contentFilters) {
        if (contentFilter->didBlockData())
            return contentFilter->getReplacementData(length);
    }

    return m_contentFilters[0]->getReplacementData(length);
}

ContentFilterUnblockHandler ContentFilterCollection::unblockHandler() const
{
    ASSERT(didBlockData());

    for (auto& contentFilter : m_contentFilters) {
        if (contentFilter->didBlockData())
            return contentFilter->unblockHandler();
    }

    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

#endif // ENABLE(CONTENT_FILTERING)
