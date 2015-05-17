/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2015 Igalia S.L.
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
#include "ReadableStreamReader.h"

#if ENABLE(STREAMS_API)

#include "NotImplemented.h"

namespace WebCore {

void ReadableStreamReader::clean()
{
    m_closedSuccessCallback = nullptr;
    m_closedErrorCallback = nullptr;
}

void ReadableStreamReader::closed(ClosedSuccessCallback successCallback, ClosedErrorCallback errorCallback)
{
    if (m_state == State::Closed) {
        successCallback();
        return;
    }
    if (m_state == State::Errored) {
        errorCallback(m_stream);
        return;
    }
    m_closedSuccessCallback = WTF::move(successCallback);
    m_closedErrorCallback = WTF::move(errorCallback);
}

void ReadableStreamReader::changeStateToClosed()
{
    ASSERT(m_state == State::Readable);
    m_state = State::Closed;

    if (m_closedSuccessCallback)
        m_closedSuccessCallback();

    // FIXME: Implement read promise fulfilling.

    clean();
}

void ReadableStreamReader::changeStateToErrored()
{
    ASSERT(m_state == State::Readable);
    m_state = State::Errored;

    if (m_closedErrorCallback)
        m_closedErrorCallback(m_stream);

    // FIXME: Implement read promise rejection.

    clean();
}

}

#endif
