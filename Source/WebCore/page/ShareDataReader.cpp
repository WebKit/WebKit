/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ShareDataReader.h"

#include "BlobLoader.h"
#include "Document.h"
#include "SharedBuffer.h"

namespace WebCore {

ShareDataReader::ShareDataReader(CompletionHandler<void(ExceptionOr<ShareDataWithParsedURL&>)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
{

}

ShareDataReader::~ShareDataReader()
{
    cancel();
}

void ShareDataReader::start(Document* document, ShareDataWithParsedURL&& shareData)
{
    m_filesReadSoFar = 0;
    m_shareData = WTFMove(shareData);
    int count = 0;
    m_pendingFileLoads.reserveInitialCapacity(m_shareData.shareData.files.size());
    for (auto& blob : m_shareData.shareData.files) {
        m_pendingFileLoads.append(makeUniqueRef<BlobLoader>([this, count, fileName = blob->name()](BlobLoader&) {
            this->didFinishLoading(count, fileName);
        }));
        m_pendingFileLoads.last()->start(*blob, document, FileReaderLoader::ReadAsArrayBuffer);
        if (m_pendingFileLoads.isEmpty()) {
            // The previous load failed synchronously and cancel() was called. We should not attempt to do any further loads.
            break;
        }
        ++count;
    }
}

void ShareDataReader::didFinishLoading(int loadIndex, const String& fileName)
{
    if (m_pendingFileLoads.isEmpty()) {
        // cancel() was called.
        return;
    }

    if (m_pendingFileLoads[loadIndex]->errorCode()) {
        if (auto completionHandler = std::exchange(m_completionHandler, { }))
            completionHandler(Exception { ExceptionCode::AbortError, "Abort due to error while reading files."_s });
        cancel();
        return;
    }

    auto arrayBuffer = m_pendingFileLoads[loadIndex]->arrayBufferResult();

    RawFile file;
    file.fileName = fileName;
    file.fileData = SharedBuffer::create(static_cast<const unsigned char*>(arrayBuffer->data()), arrayBuffer->byteLength());
    m_shareData.files.append(WTFMove(file));
    m_filesReadSoFar++;

    if (m_filesReadSoFar == static_cast<int>(m_pendingFileLoads.size())) {
        m_pendingFileLoads.clear();
        if (auto completionHandler = std::exchange(m_completionHandler, { }))
            completionHandler(m_shareData);
    }
}

void ShareDataReader::cancel()
{
    // Don't call m_pendingFileLoads.clear() here since destroying a BlobLoader will cause its completion handler
    // to get called, which will call didFinishLoading() and try to access m_pendingFileLoads.
    std::exchange(m_pendingFileLoads, { });
}

}
