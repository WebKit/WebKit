/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Brent Fulgham <bfulgham@webkit.org>
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
#include "Download.h"

#include "DataReference.h"
#include <WebCore/ErrorsGtk.h>
#include <WebCore/NotImplemented.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

class DownloadClient : public ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(DownloadClient);
public:
    DownloadClient(Download* download)
        : m_download(download)
    {
    }

    void downloadFailed(const ResourceError& error)
    {
        m_download->didFail(error, CoreIPC::DataReference());
    }

    void didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
    {
        m_response = adoptGRef(response.toSoupMessage());
        m_download->didReceiveResponse(response);

        if (response.httpStatusCode() >= 400) {
            downloadFailed(downloadNetworkError(ResourceError(errorDomainDownload, response.httpStatusCode(),
                                                              response.url().string(), response.httpStatusText())));
            return;
        }

        String suggestedFilename = response.suggestedFilename();
        if (suggestedFilename.isEmpty()) {
            KURL url = response.url();
            url.setQuery(String());
            url.removeFragmentIdentifier();
            suggestedFilename = decodeURLEscapeSequences(url.lastPathComponent());
        }

        bool overwrite;
        String destinationURI = m_download->decideDestinationWithSuggestedFilename(suggestedFilename.utf8().data(), overwrite);
        if (destinationURI.isEmpty()) {
            GOwnPtr<char> errorMessage(g_strdup_printf(_("Cannot determine destination URI for download with suggested filename %s"),
                                                       suggestedFilename.utf8().data()));
            downloadFailed(downloadDestinationError(response, errorMessage.get()));
            return;
        }

        GRefPtr<GFile> file = adoptGRef(g_file_new_for_uri(destinationURI.utf8().data()));
        GOwnPtr<GError> error;
        m_outputStream = adoptGRef(g_file_replace(file.get(), 0, TRUE, G_FILE_CREATE_NONE, 0, &error.outPtr()));
        if (!m_outputStream) {
            downloadFailed(downloadDestinationError(response, error->message));
            return;
        }

        m_download->didCreateDestination(destinationURI);
    }

    void didReceiveData(ResourceHandle*, const char* data, int length, int /*encodedDataLength*/)
    {
        gsize bytesWritten;
        GOwnPtr<GError> error;
        g_output_stream_write_all(G_OUTPUT_STREAM(m_outputStream.get()), data, length, &bytesWritten, 0, &error.outPtr());
        if (error) {
            downloadFailed(downloadDestinationError(ResourceResponse(m_response.get()), error->message));
            return;
        }
        m_download->didReceiveData(bytesWritten);
    }

    void didFinishLoading(ResourceHandle*, double)
    {
        m_outputStream = 0;
        m_download->didFinish();
    }

    void didFail(ResourceHandle*, const ResourceError& error)
    {
        downloadFailed(downloadNetworkError(error));
    }

    void wasBlocked(ResourceHandle*)
    {
        notImplemented();
    }

    void cannotShowURL(ResourceHandle*)
    {
        notImplemented();
    }

    Download* m_download;
    GRefPtr<GFileOutputStream> m_outputStream;
    GRefPtr<SoupMessage> m_response;
};

void Download::start(WebPage* initiatingWebPage)
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = adoptPtr(new DownloadClient(this));
    m_resourceHandle = ResourceHandle::create(0, m_request, m_downloadClient.get(), false, false);
    didStart();
}

void Download::startWithHandle(WebPage* initiatingPage, ResourceHandle* resourceHandle, const ResourceResponse&)
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = adoptPtr(new DownloadClient(this));
    resourceHandle->setClient(m_downloadClient.get());
    m_resourceHandle = resourceHandle;
    didStart();
}

void Download::cancel()
{
    if (!m_resourceHandle)
        return;
    m_resourceHandle->cancel();
    didCancel(CoreIPC::DataReference());
    m_resourceHandle = 0;
}

void Download::platformInvalidate()
{
    if (m_resourceHandle) {
        m_resourceHandle->setClient(0);
        m_resourceHandle->cancel();
        m_resourceHandle = 0;
    }
    m_downloadClient.release();
}

void Download::didDecideDestination(const String& destination, bool allowOverwrite)
{
    notImplemented();
}

void Download::platformDidFinish()
{
    m_resourceHandle = 0;
}

void Download::receivedCredential(const AuthenticationChallenge& authenticationChallenge, const Credential& credential)
{
    notImplemented();
}

void Download::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

void Download::receivedCancellation(const AuthenticationChallenge& authenticationChallenge)
{
    notImplemented();
}

} // namespace WebKit
