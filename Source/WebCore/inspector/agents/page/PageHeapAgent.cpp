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


namespace WebCore {

using namespace Inspector;

PageHeapAgent::PageHeapAgent(PageAgentContext& context)
    : WebHeapAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
{
}

PageHeapAgent::~PageHeapAgent() = default;

Protocol::ErrorStringOr<void> PageHeapAgent::enable()
{
    auto result = WebHeapAgent::enable();

    m_instrumentingAgents.setEnabledPageHeapAgent(this);

    return result;
}

Protocol::ErrorStringOr<void> PageHeapAgent::disable()
{
    m_instrumentingAgents.setEnabledPageHeapAgent(nullptr);

    return WebHeapAgent::disable();
}

void PageHeapAgent::mainFrameNavigated()
{
    clearHeapSnapshots();
}

} // namespace WebCore
