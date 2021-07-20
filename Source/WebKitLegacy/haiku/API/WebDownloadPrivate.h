/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDownload_h
#define WebDownload_h

#include "WebCore/ResourceHandle.h"
#include "WebCore/ResourceHandleClient.h"
#include <File.h>
#include <Messenger.h>
#include <Path.h>
#include <String.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class NetworkingContext;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

using WebCore::ResourceError;
using WebCore::ResourceHandle;
using WebCore::ResourceRequest;
using WebCore::ResourceResponse;

class BWebDownload;
class BWebPage;

namespace BPrivate {

class WebDownloadPrivate : public WebCore::ResourceHandleClient {
	WTF_MAKE_NONCOPYABLE(WebDownloadPrivate);
public:
    WebDownloadPrivate(const ResourceRequest&, WebCore::NetworkingContext*);

    // ResourceHandleClient implementation
    virtual void didReceiveResponseAsync(ResourceHandle*, ResourceResponse&&, WTF::CompletionHandler<void()>&&) override;
    virtual void didReceiveData(ResourceHandle*, const uint8_t*, unsigned, int) override;
    virtual void didFinishLoading(ResourceHandle*, const WebCore::NetworkLoadMetrics&) override;
    virtual void didFail(ResourceHandle*, const ResourceError&) override;
    virtual void wasBlocked(ResourceHandle*) override;
    virtual void cannotShowURL(ResourceHandle*) override;
	void willSendRequestAsync(ResourceHandle*, ResourceRequest&&, ResourceResponse&&, CompletionHandler<void(ResourceRequest&&)>&&) override {}

    void setDownload(BWebDownload*);
    void start(const BPath& path);
    void hasMovedTo(const BPath& path);
    void cancel();
    void setProgressListener(const BMessenger&);

    const BString& url() const { return m_url; }
    const BString& filename() const { return m_filename; }
    const BPath& path() const { return m_path; }
    off_t currentSize() const { return m_currentSize; }
    off_t expectedSize() const { return m_expectedSize; }

private:
	void handleFinished(WebCore::ResourceHandle* handle, uint32 status);
	void createFile();
	void findAvailableFilename();

private:
    BWebDownload* m_webDownload;

    RefPtr<ResourceHandle> m_resourceHandle;
    off_t m_currentSize;
    off_t m_expectedSize;
    BString m_url;
    BPath m_path;
    BString m_filename;
    BString m_mimeType;
    int m_mimeTypeGuessTries;
    BFile m_file;
    bigtime_t m_lastProgressReportTime;

    BMessenger m_progressListener;
};

} // namespace BPrivate

#endif // WebDownload_h
