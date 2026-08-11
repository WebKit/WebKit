/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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

#include "NetworkDataTask.h"
#include <WebCore/BlobResourceHandleBase.h>
#include <WebCore/FileStreamClient.h>
#include <WebCore/HTTPParsers.h>
#include <wtf/FileHandle.h>

namespace WebCore {
class AsyncFileStream;
class BlobDataFileReference;
class BlobData;
class BlobDataItem;
}

namespace WebKit {

class NetworkProcess;

class NetworkDataTaskBlob final : public NetworkDataTask, public WebCore::BlobResourceHandleBase {
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& request, const Vector<Ref<WebCore::BlobDataFileReference>>& fileReferences, const RefPtr<WebCore::SecurityOrigin>& topOrigin)
    {
        return adoptRef(*new NetworkDataTaskBlob(session, client, request, fileReferences, topOrigin));
    }

    ~NetworkDataTaskBlob();

    // FileStreamClient.
    void ref() const final { NetworkDataTask::ref(); }
    void deref() const final { NetworkDataTask::deref(); }

private:
    NetworkDataTaskBlob(NetworkSession&, NetworkDataTaskClient&, const WebCore::ResourceRequest&, const Vector<Ref<WebCore::BlobDataFileReference>>&, const RefPtr<WebCore::SecurityOrigin>& topOrigin);

    // NetworkDataTask.
    void cancel() final;
    void resume() final;
    void invalidateAndCancel() final;
    NetworkDataTask::State state() const final { return m_state; }
    void setPendingDownloadLocation(const String&, SandboxExtension::Handle&&, bool /*allowOverwrite*/) final;
    String suggestedFilename() const final;

    // BlobResourceHandleBase.
    bool didReceiveData(std::span<const uint8_t>) final;
    void didReceiveResponse(WebCore::ResourceResponse&&) final;
    void didFail(Error) final;
    bool erroredOrAborted() const final;
    void didFinish() final;
    const WebCore::ResourceRequest& firstRequest() const final { return NetworkDataTask::firstRequest(); }
    void clearStream() final;

    void download();
    bool writeDownload(std::span<const uint8_t>);
    void cleanDownloadFiles();
    void didFailDownload(const WebCore::ResourceError&);
    void didFinishDownload();

    State m_state { State::Suspended };
    uint64_t m_downloadBytesWritten { 0 };
    FileSystem::FileHandle m_downloadFile;
    Vector<Ref<WebCore::BlobDataFileReference>> m_fileReferences;
    RefPtr<SandboxExtension> m_sandboxExtension;
    const Ref<NetworkProcess> m_networkProcess;
};

} // namespace WebKit
