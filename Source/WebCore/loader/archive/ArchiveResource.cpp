/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ArchiveResource.h"

#include "SharedBuffer.h"
#include <wtf/RunLoop.h>

namespace WebCore {

inline ArchiveResource::ArchiveResource(Ref<FragmentedSharedBuffer>&& data, const URL& url, const String& mimeType, const String& textEncoding, const String& frameName, const ResourceResponse& response, const String& relativeFilePath)
    : SubstituteResource(URL { url }, ResourceResponse { response }, WTFMove(data))
    , m_mimeType(mimeType)
    , m_textEncoding(textEncoding)
    , m_frameName(frameName)
    , m_relativeFilePath(relativeFilePath)
    , m_shouldIgnoreWhenUnarchiving(false)
{
}

RefPtr<ArchiveResource> ArchiveResource::create(RefPtr<FragmentedSharedBuffer>&& data, const URL& url, const String& mimeType, const String& textEncoding, const String& frameName, const ResourceResponse& response, const String& relativeFilePath)
{
    if (!data)
        return nullptr;
    if (response.isNull()) {
        ResourceResponse syntheticResponse(url, mimeType, data->size(), textEncoding);
        // Provide a valid HTTP status code for http URLs since we have logic in WebCore that validates it.
        if (url.protocolIsInHTTPFamily())
            syntheticResponse.setHTTPStatusCode(200);
        return adoptRef(*new ArchiveResource(data.releaseNonNull(), url, mimeType, textEncoding, frameName, WTFMove(syntheticResponse), relativeFilePath));
    }
    return adoptRef(*new ArchiveResource(data.releaseNonNull(), url, mimeType, textEncoding, frameName, response, relativeFilePath));
}

RefPtr<ArchiveResource> ArchiveResource::create(RefPtr<FragmentedSharedBuffer>&& data, const URL& url, const ResourceResponse& response)
{
    return create(WTFMove(data), url, response.mimeType(), response.textEncodingName(), String(), response);
}

Expected<String, ArchiveError> ArchiveResource::saveToDisk(const String& directory)
{
    ASSERT(!RunLoop::isMain());

    if (directory.isEmpty() || m_relativeFilePath.isEmpty())
        return makeUnexpected(ArchiveError::InvalidFilePath);

    auto filePath = FileSystem::pathByAppendingComponent(directory, m_relativeFilePath);
    FileSystem::makeAllDirectories(FileSystem::parentPath(filePath));
    auto fileData = data().extractData();
    int bytesWritten = FileSystem::overwriteEntireFile(filePath, { fileData.data(), fileData.size() });

    if (bytesWritten < 0)
        return makeUnexpected(ArchiveError::FileSystemError);

    if ((size_t)bytesWritten != fileData.size()) {
        FileSystem::deleteFile(filePath);
        return makeUnexpected(ArchiveError::FileSystemError);
    }

    return filePath;
}

}
