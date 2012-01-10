/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef KeychainShimResponseMap_h
#define KeychainShimResponseMap_h

#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

template<typename T>
class KeychainShimResponseMap {
public:
    PassOwnPtr<T> waitForResponse(uint64_t requestID)
    {
        while (true) {
            MutexLocker locker(m_mutex);

            if (OwnPtr<T> response = m_responses.take(requestID))
                return response.release();

            m_condition.wait(m_mutex);
        }

        return nullptr;
    }

    void didReceiveResponse(uint64_t requestID, PassOwnPtr<T> response)
    {
        MutexLocker locker(m_mutex);
        ASSERT(!m_responses.contains(requestID));

        m_responses.set(requestID, response);
        m_condition.signal();
    }

private:
    Mutex m_mutex;
    ThreadCondition m_condition;

    HashMap<uint64_t, OwnPtr<T> > m_responses;
};



#endif // KeychainShimResponseMap_h
