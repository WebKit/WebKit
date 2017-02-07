/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <inspector/agents/InspectorHeapAgent.h>
#include <wtf/Vector.h>

namespace WebCore {

class SendGarbageCollectionEventsTask;
struct GarbageCollectionData;
typedef String ErrorString;

class WebHeapAgent : public Inspector::InspectorHeapAgent {
    WTF_MAKE_NONCOPYABLE(WebHeapAgent);
    WTF_MAKE_FAST_ALLOCATED;
    friend class SendGarbageCollectionEventsTask;
public:
    WebHeapAgent(Inspector::AgentContext&);
    virtual ~WebHeapAgent();

protected:
    void disable(ErrorString&) override;

    void dispatchGarbageCollectedEvent(Inspector::Protocol::Heap::GarbageCollection::Type, double startTime, double endTime) override;

    void dispatchGarbageCollectionEventsAfterDelay(Vector<GarbageCollectionData>&& collections);

    std::unique_ptr<SendGarbageCollectionEventsTask> m_sendGarbageCollectionEventsTask;
};

} // namespace WebCore
