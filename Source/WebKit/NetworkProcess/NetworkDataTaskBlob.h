/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
#include <WebCore/FileStreamClient.h>
#include <WebCore/FileSystem.h>

namespace WebCore {
class AsyncFileStream;
class BlobDataFileReference;
class BlobData;
class BlobDataItem;
}

namespace WebKit {

class NetworkProcess;

class NetworkDataTaskBlob final : public NetworkDataTask, public WebCore::FileStreamClient {
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& request, WebCore::ContentSniffingPolicy shouldContentSniff, const Vector<RefPtr<WebCore::BlobDataFileReference>>& fileReferences)
    {
        return adoptRef(*new NetworkDataTaskBlob(session, client, request, shouldContentSniff, fileReferences));
    }

    ~NetworkDataTaskBlob();

private:
    NetworkDataTaskBlob(NetworkSession&, NetworkDataTaskClient&, const WebCore::ResourceRequest&, WebCore::ContentSniffingPolicy, const Vector<RefPtr<WebCore::BlobDataFileReference>>&);

    void suspend() override;
    void cancel() override;
    void resume() override;
    void invalidateAndCancel() override;
    NetworkDataTask::State state() const override { return m_state; }

    void setPendingDownloadLocation(const String&, SandboxExtension::Handle&&, bool /*allowOverwrite*/) override;
    String suggestedFilename() const override;

    // FileStreamClient methods.
    void didGetSize(long long) override;
    void didOpen(bool) override;
    void didRead(int) override;

    enum class Error {
        NoError = 0,
        NotFoundError = 1,
        SecurityError = 2,
        RangeError = 3,
        NotReadableError = 4,
        MethodNotAllowed = 5
    };

    void clearStream();
    void getSizeForNext();
    void dispatchDidReceiveResponse(Error = Error::NoError);
    void seek();
    void consumeData(const char* data, int bytesRead);
    void read();
    void readData(const WebCore::BlobDataItem&);
    void readFile(const WebCore::BlobDataItem&);
    void download();
    bool writeDownload(const char* data, int bytesRead);
    void cleanDownloadFiles();
    void didFailDownload(const WebCore::ResourceError&);
    void didFinishDownload();
    void didFail(Error);
    void didFinish();

    enum { kPositionNotSpecified = -1 };

    RefPtr<WebCore::BlobData> m_blobData;
    std::unique_ptr<WebCore::AsyncFileStream> m_stream; // For asynchronous loading.
    Vector<char> m_buffer;
    Vector<long long> m_itemLengthList;
    State m_state { State::Suspended };
    long long m_rangeOffset { kPositionNotSpecified };
    long long m_rangeEnd { kPositionNotSpecified };
    long long m_rangeSuffixLength { kPositionNotSpecified };
    long long m_totalSize { 0 };
    long long m_totalRemainingSize { 0 };
    long long m_currentItemReadSize { 0 };
    unsigned m_sizeItemCount { 0 };
    unsigned m_readItemCount { 0 };
    bool m_fileOpened { false };
    WebCore::FileSystem::PlatformFileHandle m_downloadFile { WebCore::FileSystem::invalidPlatformFileHandle };

    Vector<RefPtr<WebCore::BlobDataFileReference>> m_fileReferences;
    RefPtr<SandboxExtension> m_sandboxExtension;
    Ref<NetworkProcess> m_networkProcess;
};

} // namespace WebKit
