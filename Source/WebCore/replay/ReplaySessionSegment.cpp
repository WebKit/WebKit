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
#include "ReplaySessionSegment.h"

#if ENABLE(WEB_REPLAY)

#include "CapturingInputCursor.h"
#include "FunctorInputCursor.h"
#include "ReplayingInputCursor.h"
#include "SegmentedInputStorage.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static unsigned s_nextSegmentIdentifier = 1;

PassRefPtr<ReplaySessionSegment> ReplaySessionSegment::create()
{
    return adoptRef(new ReplaySessionSegment);
}

ReplaySessionSegment::ReplaySessionSegment()
    : m_storage(std::make_unique<SegmentedInputStorage>())
    , m_identifier(s_nextSegmentIdentifier++)
    , m_canCapture(true)
    , m_timestamp(currentTimeMS())
{
}

ReplaySessionSegment::~ReplaySessionSegment()
{
}

PassRefPtr<CapturingInputCursor> ReplaySessionSegment::createCapturingCursor(Page&)
{
    ASSERT(m_canCapture);
    m_canCapture = false;
    return CapturingInputCursor::create(*m_storage);
}

PassRefPtr<ReplayingInputCursor> ReplaySessionSegment::createReplayingCursor(Page& page, EventLoopInputDispatcherClient* client)
{
    return ReplayingInputCursor::create(*m_storage, page, client);
}

PassRefPtr<FunctorInputCursor> ReplaySessionSegment::createFunctorCursor()
{
    return FunctorInputCursor::create(*m_storage);
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
