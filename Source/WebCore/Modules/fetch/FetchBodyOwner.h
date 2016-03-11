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

#ifndef FetchBodyOwner_h
#define FetchBodyOwner_h

#if ENABLE(FETCH_API)

#include "ActiveDOMObject.h"
#include "FetchBody.h"

namespace WebCore {

enum class FetchLoadingType { Text, ArrayBuffer, Blob };

class FetchBodyOwner : public RefCounted<FetchBodyOwner>, public ActiveDOMObject {
public:
    FetchBodyOwner(ScriptExecutionContext&, FetchBody&&);

    // Exposed Body API
    bool isDisturbed() const { return m_body.isDisturbed(); }
    void arrayBuffer(DeferredWrapper&& promise) { m_body.arrayBuffer(*this, WTFMove(promise)); }
    void blob(DeferredWrapper&& promise) { m_body.blob(*this, WTFMove(promise)); }
    void formData(DeferredWrapper&& promise) { m_body.formData(*this, WTFMove(promise)); }
    void json(DeferredWrapper&& promise) { m_body.json(*this, WTFMove(promise)); }
    void text(DeferredWrapper&& promise) { m_body.text(*this, WTFMove(promise)); }

    void loadBlob(Blob&, FetchLoadingType);

protected:
    FetchBody m_body;
};

inline FetchBodyOwner::FetchBodyOwner(ScriptExecutionContext& context, FetchBody&& body)
    : ActiveDOMObject(&context)
    , m_body(WTFMove(body))
{
    suspendIfNeeded();
}

inline void FetchBodyOwner::loadBlob(Blob& blob, FetchLoadingType type)
{
    if (type == FetchLoadingType::Blob) {
        // FIXME: Clone blob.
        m_body.loadedAsBlob(blob);
        return;
    }
    // FIXME: Implement blob loading.
    m_body.loadingFailed();
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchBodyOwner_h
