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
#include "PageHeapAgent.h"

#include "Page.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageHeapAgent);

PageHeapAgent::PageHeapAgent(PageAgentContext& context)
    : WebHeapAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedPage(context.inspectedPage)
{
}

PageHeapAgent::~PageHeapAgent() = default;

Inspector::Protocol::ErrorStringOr<void> PageHeapAgent::enable()
{
    auto result = WebHeapAgent::enable();

    Ref { m_instrumentingAgents.get() }->setEnabledPageHeapAgent(this);

    return result;
}

Inspector::Protocol::ErrorStringOr<void> PageHeapAgent::disable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageHeapAgent(nullptr);

    return WebHeapAgent::disable();
}

bool PageHeapAgent::observesGarbageCollections() const
{
    // Under Site Isolation, each frame target reports the garbage collections of its own process.
    return !protect(m_inspectedPage)->settings().siteIsolationEnabled();
}

void PageHeapAgent::mainFrameNavigated()
{
    clearHeapSnapshots();
}

} // namespace WebCore
