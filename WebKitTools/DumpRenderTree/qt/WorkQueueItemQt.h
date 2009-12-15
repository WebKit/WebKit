/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef WorkQueueItemQt_h
#define WorkQueueItemQt_h

#include <QPointer>
#include <QString>
#include <qwebframe.h>
#include <qwebpage.h>

class WorkQueueItem {
public:
    WorkQueueItem(QWebPage *page) : m_webPage(page) {}
    virtual ~WorkQueueItem() { }
    virtual bool invoke() const = 0;

protected:
    QPointer<QWebPage> m_webPage;
};

class LoadItem : public WorkQueueItem {
public:
    LoadItem(const QString &url, const QString &target, QWebPage *page)
        : WorkQueueItem(page)
        , m_url(url)
        , m_target(target)
    {
    }

    QString url() const { return m_url; }
    QString target() const { return m_target; }

    virtual bool invoke() const;

private:
    QString m_url;
    QString m_target;
};

class ReloadItem : public WorkQueueItem {
public:
    ReloadItem(QWebPage *page)
        : WorkQueueItem(page)
    {
    }
    virtual bool invoke() const;
};

class ScriptItem : public WorkQueueItem {
public:
    ScriptItem(const QString &script, QWebPage *page)
        : WorkQueueItem(page)
        , m_script(script)
    {
    }

    QString script() const { return m_script; }

    virtual bool invoke() const;

private:
    QString m_script;
};

class LoadingScriptItem : public ScriptItem {
public:
    LoadingScriptItem(const QString& script, QWebPage* page)
        : ScriptItem(script, page)
    {
    }

    virtual bool invoke() const { return ScriptItem::invoke(); }
};

class NonLoadingScriptItem : public ScriptItem {
public:
    NonLoadingScriptItem(const QString& script, QWebPage* page)
        : ScriptItem(script, page)
    {
    }

    virtual bool invoke() const { ScriptItem::invoke(); return false; }
};


class BackForwardItem : public WorkQueueItem {
public:
    virtual bool invoke() const;

protected:
    BackForwardItem(int howFar, QWebPage *page)
        : WorkQueueItem(page)
        , m_howFar(howFar)
    {
    }

    int m_howFar;
};

class BackItem : public BackForwardItem {
public:
    BackItem(unsigned howFar, QWebPage *page)
        : BackForwardItem(-howFar, page)
    {
    }
};

class ForwardItem : public BackForwardItem {
public:
    ForwardItem(unsigned howFar, QWebPage *page)
        : BackForwardItem(howFar, page)
    {
    }
};

#endif // !defined(WorkQueueItemQt_h)
