/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
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

#include "config.h"
#include "FrameQtClient.h"

#include "Cache.h"
#include "FrameQt.h"
#include "Document.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FrameLoader.h"
#include "ResourceHandle.h"
#include "LoaderFunctions.h"
#include "ResourceHandleInternal.h"

#if PLATFORM(KDE)
#include <kstdguiitem.h>
#include <kmessagebox.h>
#include <kinputdialog.h>
#else
#include <QInputDialog>
#include <QMessageBox>
#endif

#include <qdebug.h>

namespace WebCore {

FrameQtClient::FrameQtClient()
    : ResourceHandleClient()
    , m_frame(0)
{
}

FrameQtClient::~FrameQtClient()
{
}

void FrameQtClient::setFrame(const FrameQt* frame)
{
    ASSERT(frame);
    m_frame = const_cast<FrameQt*>(frame);
}

void FrameQtClient::openURL(const KURL& url)
{
    ASSERT(m_frame);

    m_frame->loader()->didOpenURL(url);

    if (!m_frame->document())
        m_frame->loader()->createEmptyDocument();

    ASSERT(m_frame->document());

    ResourceRequest request(url);
    RefPtr<ResourceHandle> loader = ResourceHandle::create(request, this,
                                                    m_frame->document()->docLoader(), false);
    loader.get()->ref();
}

void FrameQtClient::submitForm(const String& method, const KURL& url, PassRefPtr<FormData> postData)
{
    ASSERT(m_frame);

    ResourceRequest request(url);
    request.setHTTPMethod(method);
    request.setHTTPBody(postData);

    RefPtr<ResourceHandle> loader = ResourceHandle::create(request, this,
                                                           m_frame->document() ? m_frame->document()->docLoader() : 0,
                                                           false);
    loader.get()->ref();
}

void FrameQtClient::didReceiveResponse(ResourceHandle* job, const ResourceResponse& response)
{
    // set mimetype and encoding

    // Assign correct mimetype _before_ calling begin()!
    m_frame->loader()->setResponseMIMEType(response.mimeType());

    // TODO: Allow user overrides of the encoding...
    // This calls begin() for us, despite the misleading name
    if (response.textEncodingName().length()) {
        // qDebug() << "FrameQtClient: setting encoding to" << response.textEncodingName();
        m_frame->loader()->setEncoding(response.textEncodingName(), false);
    } else {
        m_frame->loader()->begin(job->url());
    }
}

void FrameQtClient::didFinishLoading(ResourceHandle* handle)
{
    m_frame->loader()->end();
    m_frame->loader()->checkCompleted();
    handle->deref();
}

void FrameQtClient::didFail(ResourceHandle* handle, const ResourceError&)
{
    m_frame->loader()->end();
    m_frame->loader()->checkCompleted();
    handle->deref();
}

void FrameQtClient::didReceiveData(ResourceHandle* job, const char* data, int length, int)
{
    // Feed with new data
    m_frame->loader()->write(data, length);
}

FrameQt* FrameQtClient::traverseNextFrameStayWithin(FrameQt* frame) const
{
    return QtFrame(m_frame->tree()->traverseNext(frame));
}

static int numRequests(Document* document)
{
    if (document)
        return cache()->loader()->numRequests(document->docLoader());

    return 0;
}

int FrameQtClient::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return numRequests(m_frame->document());

    int num = 0;
    for (FrameQt* frame = m_frame; frame != 0; frame = traverseNextFrameStayWithin(frame))
        num += numRequests(frame->document());

    return num;
}

}
