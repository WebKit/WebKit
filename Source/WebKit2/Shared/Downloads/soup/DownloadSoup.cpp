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
#include "DownloadSoupErrors.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandleInternal.h>
#include <gio/gio.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include <glib/gi18n-lib.h>
#endif

using namespace WebCore;

namespace WebKit {

class DownloadClient : public ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(DownloadClient);
public:
    DownloadClient(Download* download)
        : m_download(download)
        , m_handleResponseLaterID(0)
    {
    }

    ~DownloadClient()
    {
        if (m_handleResponseLaterID)
            g_source_remove(m_handleResponseLaterID);
    }

    void deleteIntermediateFileInNeeded()
    {
        if (!m_intermediateFile)
            return;
        g_file_delete(m_intermediateFile.get(), nullptr, nullptr);
    }

    void downloadFailed(const ResourceError& error)
    {
        deleteIntermediateFileInNeeded();
        m_download->didFail(error, IPC::DataReference());
    }

    void didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
    {
        m_response = response;
        m_download->didReceiveResponse(response);

        if (response.httpStatusCode() >= 400) {
            downloadFailed(platformDownloadNetworkError(response.httpStatusCode(), response.url().string(), response.httpStatusText()));
            return;
        }

        String suggestedFilename = response.suggestedFilename();
        if (suggestedFilename.isEmpty()) {
            URL url = response.url();
            url.setQuery(String());
            url.removeFragmentIdentifier();
            suggestedFilename = decodeURLEscapeSequences(url.lastPathComponent());
        }

        bool overwrite;
        m_destinationURI = m_download->decideDestinationWithSuggestedFilename(suggestedFilename, overwrite);
        if (m_destinationURI.isEmpty()) {
#if PLATFORM(GTK)
            GUniquePtr<char> buffer(g_strdup_printf(_("Cannot determine destination URI for download with suggested filename %s"), suggestedFilename.utf8().data()));
            String errorMessage = String::fromUTF8(buffer.get());
#else
            String errorMessage = makeString("Cannot determine destination URI for download with suggested filename ", suggestedFilename);
#endif
            downloadFailed(platformDownloadDestinationError(response, errorMessage));
            return;
        }

        String intermediateURI = m_destinationURI + ".wkdownload";
        m_intermediateFile = adoptGRef(g_file_new_for_uri(intermediateURI.utf8().data()));
        GOwnPtr<GError> error;
        m_outputStream = adoptGRef(g_file_replace(m_intermediateFile.get(), 0, TRUE, G_FILE_CREATE_NONE, 0, &error.outPtr()));
        if (!m_outputStream) {
            downloadFailed(platformDownloadDestinationError(response, error->message));
            return;
        }

        m_download->didCreateDestination(m_destinationURI);
    }

    void didReceiveData(ResourceHandle*, const char* data, unsigned length, int /*encodedDataLength*/)
    {
        if (m_handleResponseLaterID) {
            g_source_remove(m_handleResponseLaterID);
            handleResponse();
        }

        gsize bytesWritten;
        GOwnPtr<GError> error;
        g_output_stream_write_all(G_OUTPUT_STREAM(m_outputStream.get()), data, length, &bytesWritten, 0, &error.outPtr());
        if (error) {
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }
        m_download->didReceiveData(bytesWritten);
    }

    void didFinishLoading(ResourceHandle*, double)
    {
        m_outputStream = 0;

        ASSERT(m_intermediateFile);
        GRefPtr<GFile> destinationFile = adoptGRef(g_file_new_for_uri(m_destinationURI.utf8().data()));
        GOwnPtr<GError> error;
        if (!g_file_move(m_intermediateFile.get(), destinationFile.get(), G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &error.outPtr())) {
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }

        GRefPtr<GFileInfo> info = adoptGRef(g_file_info_new());
        CString uri = m_response.url().string().utf8();
        g_file_info_set_attribute_string(info.get(), "metadata::download-uri", uri.data());
        g_file_info_set_attribute_string(info.get(), "xattr::xdg.origin.url", uri.data());
        g_file_set_attributes_async(destinationFile.get(), info.get(), G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, nullptr, nullptr, nullptr);

        m_download->didFinish();
    }

    void didFail(ResourceHandle*, const ResourceError& error)
    {
        downloadFailed(platformDownloadNetworkError(error.errorCode(), error.failingURL(), error.localizedDescription()));
    }

    void wasBlocked(ResourceHandle*)
    {
        notImplemented();
    }

    void cannotShowURL(ResourceHandle*)
    {
        notImplemented();
    }

    void cancel(ResourceHandle* handle)
    {
        handle->cancel();
        deleteIntermediateFileInNeeded();
        m_download->didCancel(IPC::DataReference());
    }

    void handleResponse()
    {
        m_handleResponseLaterID = 0;
        didReceiveResponse(0, m_delayedResponse);
    }

    static gboolean handleResponseLaterCallback(DownloadClient* downloadClient)
    {
        downloadClient->handleResponse();
        return FALSE;
    }

    void handleResponseLater(const ResourceResponse& response)
    {
        ASSERT(m_response.isNull());
        ASSERT(!m_handleResponseLaterID);

        m_delayedResponse = response;

        // Call didReceiveResponse in an idle to make sure the download is added
        // to the DownloadManager downloads map.
        m_handleResponseLaterID = g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(handleResponseLaterCallback), this, 0);
    }

    Download* m_download;
    GRefPtr<GFileOutputStream> m_outputStream;
    ResourceResponse m_response;
    String m_destinationURI;
    GRefPtr<GFile> m_intermediateFile;
    ResourceResponse m_delayedResponse;
    unsigned m_handleResponseLaterID;
};

void Download::start()
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = adoptPtr(new DownloadClient(this));
    m_resourceHandle = ResourceHandle::create(0, m_request, m_downloadClient.get(), false, false);
    didStart();
}

void Download::startWithHandle(ResourceHandle* resourceHandle, const ResourceResponse& response)
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = adoptPtr(new DownloadClient(this));
    resourceHandle->setClient(m_downloadClient.get());
    m_resourceHandle = resourceHandle;
    didStart();
    static_cast<DownloadClient*>(m_downloadClient.get())->handleResponseLater(response);
}

void Download::cancel()
{
    if (!m_resourceHandle)
        return;
    static_cast<DownloadClient*>(m_downloadClient.get())->cancel(m_resourceHandle.get());
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

void Download::didDecideDestination(const String& /*destination*/, bool /*allowOverwrite*/)
{
    notImplemented();
}

void Download::platformDidFinish()
{
    m_resourceHandle = 0;
}

void Download::receivedCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void Download::receivedRequestToContinueWithoutCredential(const AuthenticationChallenge&)
{
    notImplemented();
}

void Download::receivedCancellation(const AuthenticationChallenge&)
{
    notImplemented();
}

void Download::continueWithoutCredential(const AuthenticationChallenge &)
{
    notImplemented();
}

void Download::useCredential(const AuthenticationChallenge&, const Credential&)
{
    notImplemented();
}

void Download::cancelAuthenticationChallenge(const AuthenticationChallenge&)
{
    notImplemented();
}

} // namespace WebKit
