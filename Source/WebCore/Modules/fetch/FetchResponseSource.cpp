/*
 * Copyright (C) 2016 Canon Inc.
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
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "FetchResponseSource.h"

#if ENABLE(FETCH_API) && ENABLE(READABLE_STREAM_API)

#include "FetchResponse.h"

namespace WebCore {

FetchResponseSource::FetchResponseSource(FetchResponse& response)
    : m_response(response)
{
}

bool FetchResponseSource::isReadableStreamLocked() const
{
    return controller().isControlledReadableStreamLocked();
}

void FetchResponseSource::setActive()
{
    m_response.setPendingActivity(&m_response);
}

void FetchResponseSource::setInactive()
{
    m_response.unsetPendingActivity(&m_response);
}

void FetchResponseSource::doStart()
{
    m_response.consumeBodyAsStream();
}

void FetchResponseSource::doPull()
{
    m_response.feedStream();
}

void FetchResponseSource::doCancel()
{
    m_isCancelling = true;
    m_response.cancel();
}

void FetchResponseSource::close()
{
    controller().close();
    clean();
}
void FetchResponseSource::error(const String& value)
{
    controller().error(value);
    clean();
}

} // namespace WebCore

#endif // ENABLE(FETCH_API) && ENABLE(READABLE_STREAM_API)
