/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2017 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchBodySource.h"

#if ENABLE(STREAMS_API)

#include "FetchResponse.h"

namespace WebCore {

FetchBodySource::FetchBodySource(FetchBodyOwner& bodyOwner)
    : m_bodyOwner(&bodyOwner)
{
}

void FetchBodySource::setActive()
{
    ASSERT(m_bodyOwner);
    if (m_bodyOwner)
        m_bodyOwner->setPendingActivity(*m_bodyOwner);
}

void FetchBodySource::setInactive()
{
    ASSERT(m_bodyOwner);
    if (m_bodyOwner)
        m_bodyOwner->unsetPendingActivity(*m_bodyOwner);
}

void FetchBodySource::doStart()
{
    ASSERT(m_bodyOwner);
    if (m_bodyOwner)
        m_bodyOwner->consumeBodyAsStream();
}

void FetchBodySource::doPull()
{
    ASSERT(m_bodyOwner);
    if (m_bodyOwner)
        m_bodyOwner->feedStream();
}

void FetchBodySource::doCancel()
{
    m_isCancelling = true;
    ASSERT(m_bodyOwner || m_isClosed);
    if (!m_bodyOwner)
        return;

    m_bodyOwner->cancel();
    m_bodyOwner = nullptr;
}

void FetchBodySource::close()
{
#ifndef NDEBUG
    m_isClosed = true;
#endif

    controller().close();
    clean();
    m_bodyOwner = nullptr;
}

void FetchBodySource::error(const Exception& value)
{
    controller().error(value);
    clean();
    m_bodyOwner = nullptr;
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
