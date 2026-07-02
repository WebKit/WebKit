/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/BlobResourceHandleBase.h>
#include <WebCore/ResourceHandle.h>

namespace WebCore {

class ResourceHandleClient;

class BlobResourceHandle final : public BlobResourceHandleBase, public ResourceHandle  {
public:
    static Ref<BlobResourceHandle> createAsync(BlobData*, const ResourceRequest&, ResourceHandleClient*);

    static void loadResourceSynchronously(BlobData*, const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<uint8_t>& data);

    using BlobResourceHandleBase::start;
    int readSync(std::span<uint8_t>);

    bool aborted() const { return m_aborted; }

    bool isBlobResourceHandle() const final { return true; }

    // FileStreamClient.
    void ref() const final { ResourceHandle::ref(); }
    void deref() const final { ResourceHandle::deref(); }

private:
    BlobResourceHandle(BlobData*, const ResourceRequest&, ResourceHandleClient*, bool async);
    virtual ~BlobResourceHandle();

    // ResourceHandle methods.
    void cancel() final;

    // BlobResourceHandleBase.
    bool didReceiveData(std::span<const uint8_t>) final;
    void didReceiveResponse(ResourceResponse&&) final;
    void didFail(Error) final;
    bool erroredOrAborted() const final { return m_aborted || m_errorCode != Error::NoError; }
    bool shouldAbortDispatchDidReceiveResponse() final;
    void didFinish() final;
    const ResourceRequest& firstRequest() const final { return ResourceHandle::firstRequest(); }

    void doStart();

    int readDataSync(const BlobDataItem&, DataSegment&, std::span<uint8_t>);
    int readFileSync(const BlobDataItem&, BlobDataFileReference&, std::span<uint8_t>);

    Error m_errorCode { Error::NoError };
    bool m_aborted { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::BlobResourceHandle)
    static bool isType(const WebCore::ResourceHandle& handle) { return handle.isBlobResourceHandle(); }
SPECIALIZE_TYPE_TRAITS_END()
