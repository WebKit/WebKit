/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

namespace WebCore {

FrameQtClientDefault::FrameQtClientDefault()
    : FrameQtClient()
    , ResourceHandleClient()
    , m_frame(0)
    , m_assignedMimetype(false)
{
}

FrameQtClientDefault::~FrameQtClientDefault()
{
}

void FrameQtClientDefault::setFrame(const FrameQt* frame)
{
    ASSERT(frame);
    m_frame = const_cast<FrameQt*>(frame);
}

void FrameQtClientDefault::openURL(const KURL& url)
{
    ASSERT(m_frame);

    m_frame->loader()->didOpenURL(url);
    m_assignedMimetype = false;

    if (!m_frame->document())
        m_frame->loader()->createEmptyDocument();

    ASSERT(m_frame->document());

    ResourceRequest request(url);
    RefPtr<ResourceHandle> loader = ResourceHandle::create(request, this, m_frame->document()->docLoader(), false);
}

void FrameQtClientDefault::submitForm(const String& method, const KURL& url, PassRefPtr<FormData> postData)
{
    ASSERT(m_frame);

    m_assignedMimetype = false;

    ResourceRequest request(url);
    request.setHTTPMethod(method);
    request.setHTTPBody(postData);

    RefPtr<ResourceHandle> loader = ResourceHandle::create(request, this, m_frame->document() ? m_frame->document()->docLoader() : 0, false);
}

void FrameQtClientDefault::checkLoaded()
{
    ASSERT(m_frame && m_frame->document() && m_frame->document()->docLoader());

    // Recursively check the frame tree for open requests...
    int num = numPendingOrLoadingRequests(true);
    if (!num)
        loadFinished();
}

void FrameQtClientDefault::runJavaScriptAlert(String const& message)
{
#if PLATFORM(KDE)
    KMessageBox::error(m_frame->view()->qwidget(), message, "JavaScript");
#else
    QMessageBox::warning(m_frame->view()->qwidget(), "JavaScript", message);
#endif
}

bool FrameQtClientDefault::runJavaScriptConfirm(const String& message)
{
#if PLATFORM(KDE)
    return KMessageBox::warningYesNo(m_frame->view()->qwidget(), message,
                                     "JavaScript", KStdGuiItem::ok(), KStdGuiItem::cancel())
                                     == KMessageBox::Yes;
#else
    return QMessageBox::warning(m_frame->view()->qwidget(), "JavaScript", message,
                                QMessageBox::Yes | QMessageBox::No)
                               == QMessageBox::Yes;
#endif
}

bool FrameQtClientDefault::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result)
{
    bool ok;
#if PLATFORM(KDE)
    result = KInputDialog::getText("JavaScript", message, defaultValue, &ok, m_frame->view()->qwidget());
#else
    result = QInputDialog::getText(m_frame->view()->qwidget(), "JavaScript", message,
            QLineEdit::Normal, defaultValue, &ok);
#endif

    return ok;
}

bool FrameQtClientDefault::menubarVisible() const
{
    return false;
}

bool FrameQtClientDefault::toolbarVisible() const
{
    return false;
}

bool FrameQtClientDefault::statusbarVisible() const
{
    return false;
}

bool FrameQtClientDefault::personalbarVisible() const
{
    return false;
}

bool FrameQtClientDefault::locationbarVisible() const
{
    return false;
}

void FrameQtClientDefault::loadFinished() const
{
    // no-op
}

void FrameQtClientDefault::receivedResponse(ResourceHandle*, PlatformResponse)
{
    // no-op
}

void FrameQtClientDefault::didReceiveData(ResourceHandle* job, const char* data, int length)
{
    ResourceHandleInternal* d = job->getInternal();
    ASSERT(d);

    if (!m_assignedMimetype) {
        m_assignedMimetype = true;

        // Assign correct mimetype _before_ calling begin()!
        m_frame->loader()->setResponseMIMEType(d->m_mimetype);
    }

    // TODO: Allow user overrides of the encoding...
    // This calls begin() for us, despite the misleading name
    m_frame->loader()->setEncoding(d->m_charset, false);

    // Feed with new data
    m_frame->loader()->write(data, length);
}

FrameQt* FrameQtClientDefault::traverseNextFrameStayWithin(FrameQt* frame) const
{
    return QtFrame(m_frame->tree()->traverseNext(frame));
}

static int numRequests(Document* document)
{
    if (document)
        return cache()->loader()->numRequests(document->docLoader());

    return 0;
}

int FrameQtClientDefault::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return numRequests(m_frame->document());

    int num = 0;
    for (FrameQt* frame = m_frame; frame != 0; frame = traverseNextFrameStayWithin(frame))
        num += numRequests(frame->document());

    return num;
}

void FrameQtClientDefault::receivedAllData(ResourceHandle* job, PlatformData data)
{
    ASSERT(m_frame);
  
    m_frame->loader()->end();
    m_assignedMimetype = false;
}

}
