/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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
#include "WebKitPart.h"

#include "FrameLoader.h"
#include "FrameView.h"
#include "ChromeClientQt.h"
#include "ContextMenuClientQt.h"
#include "DragClientQt.h"
#include "EditorClientQt.h"
#include "KURL.h"

#include <QDebug>

#include "Page.h"
#include "FrameQt.h"
#include "WebKitFactory.h"
#include "WebKitPartClient.h"
#include "WebKitPartBrowserExtension.h"

using namespace WebCore;

WebKitPart::WebKitPart(QWidget* parentWidget, QObject* parentObject, GUIProfile prof)
    : KParts::ReadOnlyPart(parentObject)
    , m_frame(0)
    , m_frameView(0)
    , m_client(0)
{
    setInstance(WebKitFactory::instance(), prof == BrowserViewGUI && !parentPart());
    initView(parentWidget, prof);

    m_extension = new WebKitPartBrowserExtension(this);
}

WebKitPart::~WebKitPart()
{
    if (m_frame)
        delete m_frame->page();

    delete m_client;
    delete m_extension;
}

bool WebKitPart::openFile()
{
    return true;
}

bool WebKitPart::openUrl(const KUrl& url)
{
    if (!m_client)
        return false;

    emit started(0);
    m_client->openURL(KURL(url.toEncoded()));
    return true;
}

bool WebKitPart::closeUrl()
{
    return m_frame->loader()->closeURL();
}

WebKitPart* WebKitPart::parentPart()
{
    return qobject_cast<WebKitPart*>(parent());
}

Frame* WebKitPart::frame()
{
    return m_frame.get();
}

void WebKitPart::initView(QWidget* parentWidget, GUIProfile prof)
{
    if (prof == DefaultGUI)
        setXMLFile("WebKitPart.rc");
    else if (prof == BrowserViewGUI)
        setXMLFile("WebKitPartBrowser.rc");

    m_client = new WebKitPartClient(this);
 
    // Initialize WebCore in Qt platform mode...
    Page* page = new Page(new ChromeClientQt(), new ContextMenuClientQt(), new EditorClientQt(), new DragClientQt());
    Frame* frame = new FrameQt(page, 0, m_client);

    m_frame = frame;
    frame->deref(); // Frames are created with a refcount of 1.  Release this ref, since we've assigned it to a RefPtr

    page->setMainFrame(frame);

    FrameView* frameView = new FrameView(frame);
    m_frameView = frameView;
    frameView->deref(); // FrameViews are created with a refcount of 1.  Release this ref, since we've assigned it to a RefPtr

    m_frame->setView(frameView);
    m_frameView->setParentWidget(parentWidget);

    // Initialize KParts widget...
    setWidget(m_frame->view()->qwidget());
}

#include "WebKitPart.moc"
