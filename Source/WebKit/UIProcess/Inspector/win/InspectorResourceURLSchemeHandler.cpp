/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
#include "InspectorResourceURLSchemeHandler.h"

#include <WebCore/File.h>
#include <WebCore/ResourceError.h>
#include <WebCore/WebCoreBundleWin.h>
#include <winsock2.h> // This is required for curl.h
#include <wtf/FileSystem.h>
#include <wtf/URL.h>

#if USE(CURL)
#include <curl/curl.h>
#else
#error Unknown network backend
#endif

namespace WebKit {

void InspectorResourceURLSchemeHandler::platformStartTask(WebPageProxy&, WebURLSchemeTask& task)
{
    auto requestURL = task.request().url();
    auto requestPath = makeStringByReplacingAll(requestURL.path(), '/', '\\');
    if (requestPath.startsWith("\\"_s))
        requestPath = requestPath.substring(1);
    auto path = WebCore::webKitBundlePath({ "WebInspectorUI"_s, requestPath });
    bool success;
    FileSystem::MappedFileData file(path, FileSystem::MappedFileMode::Private, success);
    if (!success) {
        task.didComplete(WebCore::ResourceError(CURLE_READ_ERROR, requestURL));
        return;
    }
    auto contentType = WebCore::File::contentTypeForFile(path);
    if (contentType.isEmpty())
        contentType = "application/octet-stream"_s;
    WebCore::ResourceResponse response(requestURL, contentType, file.size(), "UTF-8"_s);
    auto data = WebCore::SharedBuffer::create(static_cast<const char*>(file.data()), file.size());

    task.didReceiveResponse(response);
    task.didReceiveData(WTFMove(data));
    task.didComplete({ });
}

} // namespace WebKit
