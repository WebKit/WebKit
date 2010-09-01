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

#define PLATFORM(x) 0

#include "WKContext.h"
#include "WKRetainPtr.h"
#include "qgraphicswkview.h"
#include <QtGui>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <stdint.h>

class BrowserView : public QGraphicsView {
    Q_OBJECT

public:
    BrowserView(QWidget* parent = 0);
    virtual ~BrowserView() { delete m_item; }

    void load(const QUrl&);
    QGraphicsWKView* view() const;

protected:
    virtual void resizeEvent(QResizeEvent*);

private:
    QGraphicsWKView* m_item;
    WKRetainPtr<WKContextRef> m_context;
};

class BrowserWindow : public QMainWindow {
    Q_OBJECT

public:
    BrowserWindow();
    ~BrowserWindow();
    void load(const QString& url);

public slots:
    BrowserWindow* newWindow(const QString& url = "about:blank");

protected slots:
    void changeLocation();
    void loadProgress(int progress);
    void titleChanged(const QString&);
    void urlChanged(const QUrl&);

private:
    BrowserView* m_browser;
    QMenuBar* m_menu;
    QLineEdit* m_addressBar;
};
