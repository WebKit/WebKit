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
#include "SegmentedInputStorage.h"

#if ENABLE(WEB_REPLAY)

#if !LOG_DISABLED
#include "Logging.h"
#include "SerializationMethods.h"
#include <replay/EncodedValue.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#endif

namespace WebCore {

#if !LOG_DISABLED
// These are used to make the log spew from LOG(WebReplay, ...) more readable.
static const char* queueTypeToLogPrefix(InputQueue inputQueue, bool isLoad)
{
    if (isLoad) {
        switch (inputQueue) {
        case InputQueue::EventLoopInput: return "(DSPTCH-LOAD)";
        case InputQueue::LoaderMemoizedData: return "<LDMEMO-LOAD";
        case InputQueue::ScriptMemoizedData: return "<---<---<---JSMEMO-LOAD";
        case InputQueue::Count: return "ERROR!";
        }
    } else {
        switch (inputQueue) {
        case InputQueue::EventLoopInput: return ">DSPTCH-STORE";
        case InputQueue::LoaderMemoizedData: return "<LDMEMO-STORE";
        case InputQueue::ScriptMemoizedData: return "<---<---<---JSMEMO-STORE";
        case InputQueue::Count: return "ERROR!";
        }
    }
}

static String jsonStringForInput(const NondeterministicInputBase& input)
{
    EncodedValue encodedValue = EncodingTraits<NondeterministicInputBase>::encodeValue(input);
    return encodedValue.asObject()->toJSONString();
}
#endif // !LOG_DISABLED

static size_t offsetForInputQueue(InputQueue inputQueue)
{
    return static_cast<size_t>(inputQueue);
}

SegmentedInputStorage::SegmentedInputStorage()
    : m_inputCount(0)
{
    for (size_t i = 0; i < offsetForInputQueue(InputQueue::Count); i++)
        m_queues.append(new QueuedInputs);
}

SegmentedInputStorage::~SegmentedInputStorage()
{
    for (size_t i = 0; i < offsetForInputQueue(InputQueue::Count); i++)
        delete m_queues.at(i);
}

NondeterministicInputBase* SegmentedInputStorage::load(InputQueue inputQueue, size_t offset)
{
    ASSERT(offset < queueSize(inputQueue));

    NondeterministicInputBase* input = queue(inputQueue).at(offset).get();
    ASSERT(input);

    LOG(WebReplay, "%-20s %s: %s %s\n", "ReplayEvents", queueTypeToLogPrefix(inputQueue, true), input->type().string().utf8().data(), jsonStringForInput(*input).utf8().data());

    return input;
}

void SegmentedInputStorage::store(std::unique_ptr<NondeterministicInputBase> input)
{
    ASSERT(input);
    ASSERT(input->queue() < InputQueue::Count);

    LOG(WebReplay, "%-14s#%-5u %s: %s %s\n", "ReplayEvents", m_inputCount++, queueTypeToLogPrefix(input->queue(), false), input->type().string().utf8().data(), jsonStringForInput(*input).utf8().data());

    m_queues.at(offsetForInputQueue(input->queue()))->append(std::move(input));
}

size_t SegmentedInputStorage::queueSize(InputQueue inputQueue) const
{
    return queue(inputQueue).size();
}

const SegmentedInputStorage::QueuedInputs& SegmentedInputStorage::queue(InputQueue queue) const
{
    ASSERT(queue < InputQueue::Count);
    return *m_queues.at(offsetForInputQueue(queue));
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
