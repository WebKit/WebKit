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

#include "ResourceUsageThread.h"
#include <inspector/InspectorEnvironment.h>
#include <wtf/Stopwatch.h>

using namespace Inspector;

namespace WebCore {

InspectorMemoryAgent::InspectorMemoryAgent(PageAgentContext& context)
    : InspectorAgentBase(ASCIILiteral("Memory"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::MemoryFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::MemoryBackendDispatcher::create(context.backendDispatcher, this))
{
}

void InspectorMemoryAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorMemoryAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    ErrorString ignored;
    stopTracking(ignored);
}

void InspectorMemoryAgent::startTracking(ErrorString&)
{
    if (m_tracking)
        return;

    ResourceUsageThread::addObserver(this, [this] (const ResourceUsageData& data) {
        collectSample(data);
    });

    m_tracking = true;

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch()->elapsedTime());
}

void InspectorMemoryAgent::stopTracking(ErrorString&)
{
    if (!m_tracking)
        return;

    ResourceUsageThread::removeObserver(this);

    m_tracking = false;

    m_frontendDispatcher->trackingComplete();
}

void InspectorMemoryAgent::collectSample(const ResourceUsageData& data)
{
    auto javascriptCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Javascript)
        .setSize(data.categories[MemoryCategory::GCHeap].dirtySize + data.categories[MemoryCategory::GCOwned].dirtySize)
        .release();

    auto jitCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::JIT)
        .setSize(data.categories[MemoryCategory::JSJIT].dirtySize)
        .release();

    auto imagesCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Images)
        .setSize(data.categories[MemoryCategory::Images].dirtySize)
        .release();

    auto layersCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Layers)
        .setSize(data.categories[MemoryCategory::Layers].dirtySize)
        .release();

    auto pageCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Page)
        .setSize(data.categories[MemoryCategory::bmalloc].dirtySize + data.categories[MemoryCategory::LibcMalloc].dirtySize)
        .release();

    auto otherCategory = Protocol::Memory::CategoryData::create()
        .setType(Protocol::Memory::CategoryData::Type::Other)
        .setSize(data.categories[MemoryCategory::Other].dirtySize)
        .release();

    auto categories = Protocol::Array<Protocol::Memory::CategoryData>::create();
    categories->addItem(WTFMove(javascriptCategory));
    categories->addItem(WTFMove(jitCategory));
    categories->addItem(WTFMove(imagesCategory));
    categories->addItem(WTFMove(layersCategory));
    categories->addItem(WTFMove(pageCategory));
    categories->addItem(WTFMove(otherCategory));

    auto event = Protocol::Memory::Event::create()
        .setTimestamp(m_environment.executionStopwatch()->elapsedTime())
        .setCategories(WTFMove(categories))
        .release();

    m_frontendDispatcher->trackingUpdate(WTFMove(event));
}

} // namespace Inspector

#endif // ENABLE(RESOURCE_USAGE)
