/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
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

#include <QApplication>
#include <qwebpage.h>
#include <qwebframe.h>

#include <QVBoxLayout>
#include <QDir>
#include <QUrl>

#include <QProgressBar>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QDebug>



class InfoWidget :public QProgressBar {
    Q_OBJECT
public:
    InfoWidget(QWidget *parent)
        : QProgressBar(parent), m_progress(0)
    {
        setMinimum(0);
        setMaximum(100);
    }
    QSize sizeHint() const
    {
        QSize size(100, 20);
        return size;
    }
public slots:
    void startLoad()
    {
        setValue(int(m_progress*100));
        show();
    }
    void changeLoad(double change)
    {
        m_progress = change;
        setValue(int(change*100));
        //update();
    }
    void endLoad()
    {
        QTimer::singleShot(1000, this, SLOT(hide()));
        m_progress = 0;
    }

protected:
    qreal m_progress;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const QUrl &url)
    {
        page = new QWebPage(this);
        InfoWidget *info = new InfoWidget(page);
        info->setGeometry(20, 20, info->sizeHint().width(),
                          info->sizeHint().height());
        connect(page, SIGNAL(loadStarted(QWebFrame*)),
                         info, SLOT(startLoad()));
        connect(page, SIGNAL(loadProgressChanged(double)),
                         info, SLOT(changeLoad(double)));
        connect(page, SIGNAL(loadFinished(QWebFrame*)),
                         info, SLOT(endLoad()));
    
    
        setCentralWidget(page);

        page->open(url);

        info->raise();
    }
private:
    QWebPage *page;
};

#include "main.moc"

int main(int argc, char **argv)
{
    QString url = QString("%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.count() > 1)
        url = args.at(1);

    MainWindow window(url);
    window.show();

    return app.exec();
}
