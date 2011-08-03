/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
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

#ifndef BrowserWindow_h
#define BrowserWindow_h

#include "BrowserView.h"

#include "MiniBrowserApplication.h"
#include <QStringList>
#include <QtGui>

class UrlLoader;

class BrowserWindow : public QMainWindow {
    Q_OBJECT

public:
    BrowserWindow(WindowOptions* = 0);
    ~BrowserWindow();
    void load(const QString& url);

    QObject* webView() const;

public slots:
    BrowserWindow* newWindow(const QString& url = "about:blank");
    void openLocation();

signals:
    void enteredFullScreenMode(bool on);

protected slots:
    void changeLocation();
    void onLoadProgressChanged(int progress);
    void urlChanged(const QUrl&);
    void openFile();

    void screenshot();

    void toggleFullScreenMode(bool enable);

    void showUserAgentDialog();

    void loadURLListFromFile();

    void printURL(const QUrl&);

private:
    void updateUserAgentList();

    UrlLoader* m_urlLoader;
    WindowOptions m_windowOptions;
    BrowserView* m_browser;
    QLineEdit* m_addressBar;
    QStringList m_userAgentList;
};

#endif
