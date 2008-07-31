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

#include <qwebpage.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <qwebsettings.h>

#include <QtGui>
#include <QDebug>
#if QT_VERSION >= 0x040400 && !defined(QT_NO_PRINTER)
#include <QPrintPreviewDialog>
#endif

#include <QtUiTools/QUiLoader>
#include <QtDebug>


class SearchEdit;

class ClearButton : public QPushButton {
    Q_OBJECT
public:
    ClearButton(QWidget *w)
        : QPushButton(w)
    {
        setMinimumSize(24, 24);
        setFixedSize(24, 24);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    void paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
        QPainter painter(this);
        int height = parentWidget()->geometry().height();
        int width = height; //parentWidget()->geometry().width();

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(hasFocus() ? Qt::darkGray : Qt::lightGray);
        painter.setBrush(isDown() ?
                         QColor(140, 140, 190) :
                         underMouse() ? QColor(220, 220, 255) : QColor(200, 200, 230)
            );
        painter.drawEllipse(4, 4, width - 8, height - 8);
        painter.setPen(Qt::white);
        int border = 8;
        painter.drawLine(border, border, width - border, height - border);
        painter.drawLine(border, height - border, width - border, border);
    }
};

class SearchEdit : public QLineEdit
{
    Q_OBJECT
public:
    SearchEdit(const QString &str, QWidget *parent = 0)
        : QLineEdit(str, parent)
    {
        setMinimumSize(QSize(400, 24));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        setStyleSheet(":enabled { padding-right: 27px }");
        clearButton = new ClearButton(this);
        clearButton->setGeometry(QRect(geometry().right() - 27,
                                       geometry().top() - 2,
                                       geometry().right(), geometry().bottom()));
        clearButton->setVisible(true);
#ifndef QT_NO_CURSOR
        clearButton->setCursor(Qt::ArrowCursor);
#endif
#ifndef QT_NO_TOOLTIP
        clearButton->setToolTip("Clear");
#endif
        connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    }
    ~SearchEdit() { }
protected:
    virtual void paintEvent(QPaintEvent *e) {
        QLineEdit::paintEvent(e);
        if(text().isEmpty())
            clearButton->setVisible(false);
        else
            clearButton->setVisible(true);
    }
    virtual void resizeEvent(QResizeEvent *) {
        clearButton->setParent(this);
        clearButton->setGeometry(QRect(width()-27,
                                       0,
                                       24, 24));
    }
    virtual void moveEvent(QMoveEvent *) {
        clearButton->setParent(this);
        clearButton->setGeometry(QRect(width()-27, 1,
                                       24, 24));
    }

    QPushButton *clearButton;
};

class WebPage : public QWebPage
{
public:
    inline WebPage(QWidget *parent) : QWebPage(parent) {}

    virtual QWebPage *createWindow(QWebPage::WebWindowType);
    virtual QObject* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const QUrl &url = QUrl())
    {
        view = new QWebView(this);
        view->setPage(new WebPage(view));

        progress = new QProgressBar(this);
        progress->setRange(0, 100);
        progress->setMinimumSize(100, 20);
        progress->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        progress->hide();
        statusBar()->addPermanentWidget(progress);

        connect(view, SIGNAL(loadProgress(int)),
                progress, SLOT(show()));
        connect(view, SIGNAL(loadProgress(int)),
                progress, SLOT(setValue(int)));
        connect(view, SIGNAL(loadFinished(bool)),
                progress, SLOT(hide()));

        connect(view, SIGNAL(loadFinished(bool)),
                this, SLOT(loadFinished()));
        connect(view, SIGNAL(titleChanged(const QString&)),
                this, SLOT(setWindowTitle(const QString&)));
        connect(view->page(), SIGNAL(linkHovered(const QString&, const QString&, const QString &)),
                this, SLOT(showLinkHover(const QString&, const QString&)));
        connect(view->page(), SIGNAL(windowCloseRequested()), this, SLOT(deleteLater()));


        setCentralWidget(view);

        QToolBar *bar = addToolBar("Navigation");
        urlEdit = new SearchEdit(url.toString());
        urlEdit->setSizePolicy(QSizePolicy::Expanding, urlEdit->sizePolicy().verticalPolicy());
        connect(urlEdit, SIGNAL(returnPressed()),
                SLOT(changeLocation()));
        bar->addAction(view->pageAction(QWebPage::Back));
        bar->addAction(view->pageAction(QWebPage::Stop));
        bar->addAction(view->pageAction(QWebPage::Forward));
        QAction* reloadAction = view->pageAction(QWebPage::Reload);
        reloadAction->setShortcut(QKeySequence::Refresh);
        bar->addAction(reloadAction);
        bar->addSeparator();
        bar->addAction(view->pageAction(QWebPage::Cut));
        bar->addAction(view->pageAction(QWebPage::Copy));
        bar->addAction(view->pageAction(QWebPage::Paste));
        bar->addSeparator();
        bar->addAction(view->pageAction(QWebPage::Undo));
        bar->addAction(view->pageAction(QWebPage::Redo));

        bar->addSeparator();
        QAction* editAction = bar->addAction(tr("Edit"), this, SLOT(setEditable(bool)));
        editAction->setCheckable(true);
        bar->addAction(tr("Dump"), this, SLOT(dumpHtml()));
#if QT_VERSION >= 0x040400 && !defined(QT_NO_PRINTER)
        bar->addSeparator();
        QAction* printAction = bar->addAction(tr("Print"), this, SLOT(print()));
#endif

        addToolBarBreak();
        bar = addToolBar("Editing");
        bar->addAction(view->pageAction(QWebPage::ToggleBold));
        bar->addAction(view->pageAction(QWebPage::ToggleItalic));
        bar->addAction(view->pageAction(QWebPage::ToggleUnderline));

        addToolBarBreak();
        bar = addToolBar("Location");
        bar->addWidget(new QLabel(tr("Location:")));
        bar->addWidget(urlEdit);

#if QT_VERSION >= 0x040400 && !defined(QT_NO_PRINTER)
        QMenu* fileMenu = menuBar()->addMenu(tr("File"));
        fileMenu->addAction(printAction);
#endif
        QMenu* editMenu = menuBar()->addMenu(tr("Edit"));
        editMenu->addAction(view->pageAction(QWebPage::Undo));
        editMenu->addAction(view->pageAction(QWebPage::Redo));
        editMenu->addSeparator();
        editMenu->addAction(view->pageAction(QWebPage::Cut));
        editMenu->addAction(view->pageAction(QWebPage::Copy));
        editMenu->addAction(view->pageAction(QWebPage::Paste));
        
        QMenu* formatMenu = menuBar()->addMenu(tr("Format"));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleBold));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleItalic));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleUnderline));
        QMenu* writingMenu = formatMenu->addMenu(tr("Writing Direction"));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionDefault));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionLeftToRight));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionRightToLeft));

        if (url.isValid())
            view->load(url);
    }
    inline QWebPage *webPage() const { return view->page(); }
protected slots:
    void changeLocation()
    {
        QUrl url = guessUrlFromString(urlEdit->text());
        urlEdit->setText(url.toString());
        view->load(url);
        view->setFocus(Qt::OtherFocusReason);
    }
    void loadFinished()
    {
        urlEdit->setText(view->url().toString());
    }
    void showLinkHover(const QString &link, const QString &toolTip)
    {
        statusBar()->showMessage(link);
    }
    void setEditable(bool on)
    {
        view->page()->setEditable(on);
    }
    void dumpHtml()
    {
        qDebug() << "HTML: " << view->page()->mainFrame()->toHtml();
    }
    void print()
    {
#if QT_VERSION >= 0x040400 && !defined(QT_NO_PRINTER)
        QPrintPreviewDialog dlg(this);
        connect(&dlg, SIGNAL(paintRequested(QPrinter *)),
                view, SLOT(print(QPrinter *)));
        dlg.exec();
#endif
    }
protected:
 private:
    QUrl guessUrlFromString(const QString &string) {
        QString urlStr = string.trimmed();
        QRegExp test(QLatin1String("^[a-zA-Z]+\\:.*"));

        // Check if it looks like a qualified URL. Try parsing it and see.
        bool hasSchema = test.exactMatch(urlStr);
        if (hasSchema) {
            QUrl url(urlStr, QUrl::TolerantMode);
            if (url.isValid())
                return url;
        }

        // Might be a file.
        if (QFile::exists(urlStr))
            return QUrl::fromLocalFile(urlStr);

        // Might be a shorturl - try to detect the schema.
        if (!hasSchema) {
            int dotIndex = urlStr.indexOf(QLatin1Char('.'));
            if (dotIndex != -1) {
                QString prefix = urlStr.left(dotIndex).toLower();
                QString schema = (prefix == QLatin1String("ftp")) ? prefix : QLatin1String("http");
                QUrl url(schema + QLatin1String("://") + urlStr, QUrl::TolerantMode);
                if (url.isValid())
                    return url;
            }
        }

        // Fall back to QUrl's own tolerant parser.
        return QUrl(string, QUrl::TolerantMode);
    }

    QWebView *view;
    QLineEdit *urlEdit;
    QProgressBar *progress;
};

QWebPage *WebPage::createWindow(QWebPage::WebWindowType)
{
    MainWindow *mw = new MainWindow;
    return mw->webPage();
}

QObject *WebPage::createPlugin(const QString &classId, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    Q_UNUSED(url);
    Q_UNUSED(paramNames);
    Q_UNUSED(paramValues);
    QUiLoader loader;
    return loader.createWidget(classId, view());
}

#include "main.moc"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString url = QString("%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));

    QWebSettings::setMaximumPagesInCache(4);

    app.setApplicationName("QtLauncher");
#if QT_VERSION >= 0x040400
    app.setApplicationVersion("0.1");
#endif

    QWebSettings::setObjectCacheCapacities((16*1024*1024)/8, (16*1024*1024)/8, 16*1024*1024);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

    const QStringList args = app.arguments();
    if (args.count() > 1)
        url = args.at(1);

    QUrl qurl(url);
    if (qurl.scheme().isEmpty())
        qurl = QUrl::fromLocalFile(QFileInfo(url).absoluteFilePath());
    MainWindow window(qurl);
    window.show();

    return app.exec();
}
