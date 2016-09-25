/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "BlobDownloadClient.h"

#include "DataReference.h"
#include "Download.h"
#include "WebErrors.h"
#include <WebCore/ResourceError.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

using namespace WebCore;

BlobDownloadClient::BlobDownloadClient(Download& download)
    : m_download(download)
{
}

void BlobDownloadClient::didReceiveResponse(ResourceHandle*, ResourceResponse&& response)
{
    m_download.didReceiveResponse(WTFMove(response));

    bool allowOverwrite = false;
    m_destinationPath = m_download.decideDestinationWithSuggestedFilename(m_download.suggestedName(), allowOverwrite);
    if (m_destinationPath.isEmpty()) {
        didFail(nullptr, cancelledError(m_download.request()));
        return;
    }
    if (fileExists(m_destinationPath)) {
        if (!allowOverwrite) {
            m_destinationPath = emptyString();
            didFail(nullptr, cancelledError(m_download.request()));
            return;
        }
        deleteFile(m_destinationPath);
    }

    m_destinationFile = openFile(m_destinationPath, OpenForWrite);
    m_download.didCreateDestination(m_destinationPath);
}

void BlobDownloadClient::didReceiveBuffer(ResourceHandle*, Ref<SharedBuffer>&& buffer, int)
{
    writeToFile(m_destinationFile, buffer->data(), buffer->size());
    m_download.didReceiveData(buffer->size());
}

void BlobDownloadClient::didFinishLoading(ResourceHandle*, double)
{
    closeFile(m_destinationFile);
    m_download.didFinish();
}

void BlobDownloadClient::didFail(ResourceHandle*, const ResourceError& error)
{
    closeFile(m_destinationFile);
    if (!m_destinationPath.isEmpty())
        deleteFile(m_destinationPath);

    m_download.didFail(error, IPC::DataReference());
}

}
