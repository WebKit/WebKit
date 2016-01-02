/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ReplayingInputCursor.h"

#if ENABLE(WEB_REPLAY)

#include "EventLoopInputDispatcher.h"
#include "ReplaySessionSegment.h"
#include "SegmentedInputStorage.h"
#include "SerializationMethods.h"
#include "WebReplayInputs.h"
#include <wtf/text/CString.h>

namespace WebCore {

ReplayingInputCursor::ReplayingInputCursor(RefPtr<ReplaySessionSegment>&& segment, Page& page, EventLoopInputDispatcherClient* client)
    : m_segment(WTFMove(segment))
    , m_dispatcher(std::make_unique<EventLoopInputDispatcher>(page, *this, client))
{
    for (size_t i = 0; i < static_cast<size_t>(InputQueue::Count); i++)
        m_positions.append(0);
}

ReplayingInputCursor::~ReplayingInputCursor()
{
}

Ref<ReplayingInputCursor> ReplayingInputCursor::create(RefPtr<ReplaySessionSegment>&& segment, Page& page, EventLoopInputDispatcherClient* client)
{
    return adoptRef(*new ReplayingInputCursor(WTFMove(segment), page, client));
}

void ReplayingInputCursor::storeInput(std::unique_ptr<NondeterministicInputBase>)
{
    // Cannot store inputs from a replaying input cursor.
    ASSERT_NOT_REACHED();
}

NondeterministicInputBase* ReplayingInputCursor::loadInput(InputQueue queue, const String& type)
{
    NondeterministicInputBase* input = uncheckedLoadInput(queue);

    if (input->type() != type) {
        LOG_ERROR("%-25s ERROR: Expected replay input of type %s, but got type %s\n", "[ReplayingInputCursor]", type.ascii().data(), input->type().ascii().data());
        return nullptr;
    }

    return input;
}

NondeterministicInputBase* ReplayingInputCursor::uncheckedLoadInput(InputQueue queue)
{
    if (m_positions[static_cast<size_t>(queue)] >= m_segment->storage().queueSize(queue)) {
        String queueString = EncodingTraits<InputQueue>::encodeValue(queue).convertTo<String>();
        LOG_ERROR("%-30s ERROR No more inputs remain for determinism queue %s, but one was requested.", "[ReplayingInputCursor]", queueString.ascii().data());
        return nullptr;
    }

    return m_segment->storage().load(queue, m_positions[static_cast<size_t>(queue)]++);
}

EventLoopInputData ReplayingInputCursor::loadEventLoopInput()
{
    ASSERT(m_segment);

    size_t offset = m_positions.at(static_cast<size_t>(InputQueue::EventLoopInput));
    ASSERT(offset < m_segment->eventLoopTimings().size());

    EventLoopInputData data;
    data.timestamp = m_segment->eventLoopTimings().at(offset);
    data.input = static_cast<EventLoopInputBase*>(uncheckedLoadInput(InputQueue::EventLoopInput));
    return data;
}

}; // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
