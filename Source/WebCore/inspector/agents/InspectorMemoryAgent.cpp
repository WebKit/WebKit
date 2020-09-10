/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "InspectorMemoryAgent.h"

#if ENABLE(RESOURCE_USAGE)

#include "InstrumentingAgents.h"
#include "ResourceUsageThread.h"
#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/Stopwatch.h>


namespace WebCore {

using namespace Inspector;

InspectorMemoryAgent::InspectorMemoryAgent(PageAgentContext& context)
    : InspectorAgentBase("Memory"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::MemoryFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::MemoryBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorMemoryAgent::~InspectorMemoryAgent() = default;

void InspectorMemoryAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    m_instrumentingAgents.setPersistentMemoryAgent(this);
}

void InspectorMemoryAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();

    m_instrumentingAgents.setPersistentMemoryAgent(nullptr);
}

Protocol::ErrorStringOr<void> InspectorMemoryAgent::enable()
{
    if (m_instrumentingAgents.enabledMemoryAgent() == this)
        return makeUnexpected("Memory domain already enabled"_s);

    m_instrumentingAgents.setEnabledMemoryAgent(this);

    return { };
}

Protocol::ErrorStringOr<void> InspectorMemoryAgent::disable()
{
    if (m_instrumentingAgents.enabledMemoryAgent() != this)
        return makeUnexpected("Memory domain already disabled"_s);

    m_instrumentingAgents.setEnabledMemoryAgent(nullptr);

    m_tracking = false;

    ResourceUsageThread::removeObserver(this);

    return { };
}

Protocol::ErrorStringOr<void> InspectorMemoryAgent::startTracking()
{
    if (m_tracking)
        return { };

    ResourceUsageThread::addObserver(this, Memory, [this] (const ResourceUsageData& data) {
        collectSample(data);
    });

    m_tracking = true;

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

Protocol::ErrorStringOr<void> InspectorMemoryAgent::stopTracking()
{
    if (!m_tracking)
        return { };

    ResourceUsageThread::removeObserver(this);

    m_tracking = false;

    m_frontendDispatcher->trackingComplete(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

void InspectorMemoryAgent::didHandleMemoryPressure(Critical critical)
{
    MemoryFrontendDispatcher::Severity severity = critical == Critical::Yes ? MemoryFrontendDispatcher::Severity::Critical : MemoryFrontendDispatcher::Severity::NonCritical;
    m_frontendDispatcher->memoryPressure(m_environment.executionStopwatch().elapsedTime().seconds(), Protocol::Helpers::getEnumConstantValue(severity));
}

void InspectorMemoryAgent::collectSample(const ResourceUsageData& data)
{
    auto javascriptCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::JavaScript)
        .setSize(data.categories[MemoryCategory::GCHeap].totalSize() + data.categories[MemoryCategory::GCOwned].totalSize())
        .release();

    auto jitCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::JIT)
        .setSize(data.categories[MemoryCategory::JSJIT].totalSize())
        .release();

    auto imagesCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Images)
        .setSize(data.categories[MemoryCategory::Images].totalSize())
        .release();

    auto layersCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Layers)
        .setSize(data.categories[MemoryCategory::Layers].totalSize())
        .release();

    auto pageCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Page)
        .setSize(data.categories[MemoryCategory::bmalloc].totalSize() + data.categories[MemoryCategory::LibcMalloc].totalSize())
        .release();

    auto otherCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Other)
        .setSize(data.categories[MemoryCategory::Other].totalSize())
        .release();

    auto categories = JSON::ArrayOf<Protocol::Memory::CategoryData>::create();
    categories->addItem(WTFMove(javascriptCategory));
    categories->addItem(WTFMove(jitCategory));
    categories->addItem(WTFMove(imagesCategory));
    categories->addItem(WTFMove(layersCategory));
    categories->addItem(WTFMove(pageCategory));
    categories->addItem(WTFMove(otherCategory));

    auto event = Protocol::Memory::Event::create()
        .setTimestamp(m_environment.executionStopwatch().elapsedTimeSince(data.timestamp).seconds())
        .setCategories(WTFMove(categories))
        .release();

    m_frontendDispatcher->trackingUpdate(WTFMove(event));
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
