/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "WorkQueueItemQt.h"

QWebFrame* findFrameNamed(const QString& frameName, QWebFrame* frame)
{
    if (frame->frameName() == frameName)
        return frame;

    foreach (QWebFrame* childFrame, frame->childFrames())
        if (QWebFrame* f = findFrameNamed(frameName, childFrame))
            return f;

    return 0;
}

bool LoadItem::invoke() const
{
    //qDebug() << ">>>LoadItem::invoke";
    Q_ASSERT(m_webPage);

    QWebFrame* frame = 0;
    const QString t = target();
    if (t.isEmpty())
        frame = m_webPage->mainFrame();
    else
        frame = findFrameNamed(t, m_webPage->mainFrame());

    if (!frame)
        return false;

    frame->load(url());
    return true;
}

bool ReloadItem::invoke() const
{
    //qDebug() << ">>>ReloadItem::invoke";
    Q_ASSERT(m_webPage);
    m_webPage->triggerAction(QWebPage::Reload);
    return true;
}

bool ScriptItem::invoke() const
{
    //qDebug() << ">>>ScriptItem::invoke";
    Q_ASSERT(m_webPage);
    m_webPage->mainFrame()->evaluateJavaScript(script());
    return true;
}

bool BackForwardItem::invoke() const
{
    //qDebug() << ">>>BackForwardItem::invoke";
    Q_ASSERT(m_webPage);
    if (!m_howFar)
        return false;

    if (m_howFar > 0) {
        for (int i = 0; i != m_howFar; ++i)
            m_webPage->triggerAction(QWebPage::Forward);
    } else {
        for (int i = 0; i != m_howFar; --i)
            m_webPage->triggerAction(QWebPage::Back);
    }
    return true;
}
