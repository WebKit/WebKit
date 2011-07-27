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

#include "BrowserWindow.h"

#include "UrlLoader.h"
#include "qdesktopwebview.h"
#include "qtouchwebview.h"
#include "qtouchwebpage.h"

#if 0
// FIXME
static QWKPage* newPageFunction(QWKPage* page)
{
    BrowserWindow* window = new BrowserWindow();
    return window->page();
}
#endif

BrowserWindow::BrowserWindow(WindowOptions* options)
    : m_urlLoader(0)
    , m_browser(0)
{
    if (options)
        m_windowOptions = *options;
    else {
        WindowOptions tmpOptions;
        m_windowOptions = tmpOptions;
    }

    m_browser = new BrowserView(m_windowOptions.useTouchWebView);

    setAttribute(Qt::WA_DeleteOnClose);

    connect(webView(), SIGNAL(loadProgress(int)), SLOT(loadProgress(int)));
    connect(webView(), SIGNAL(titleChanged(QString)), SLOT(setWindowTitle(QString)));
    connect(webView(), SIGNAL(urlChanged(QUrl)), SLOT(urlChanged(QUrl)));

    if (m_windowOptions.printLoadedUrls)
        connect(webView(), SIGNAL(urlChanged(QUrl)), this, SLOT(printURL(QUrl)));

    if (QDesktopWebView* const desktopWebView = m_browser->desktopWebView())
        connect(desktopWebView, SIGNAL(statusBarMessageChanged(QString)), statusBar(), SLOT(showMessage(QString)));

    this->setCentralWidget(m_browser);
    m_browser->setFocus(Qt::OtherFocusReason);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Window", this, SLOT(newWindow()), QKeySequence::New);
    fileMenu->addAction("Open File", this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, SLOT(close()));

#if 0
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* undo = page()->action(QWKPage::Undo);
    undo->setShortcut(QKeySequence(QKeySequence::Undo));
    editMenu->addAction(undo);
    QAction* redo = page()->action(QWKPage::Redo);
    redo->setShortcut(QKeySequence(QKeySequence::Redo));
    editMenu->addAction(redo);
    editMenu->addSeparator();

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(page()->action(QWKPage::Stop));
    viewMenu->addAction(page()->action(QWKPage::Reload));
    viewMenu->addSeparator();
    viewMenu->addAction("Take Screen Shot...", this, SLOT(screenshot()));
#endif

    QMenu* windowMenu = menuBar()->addMenu("&Window");
    QAction* toggleFullScreen = windowMenu->addAction("Toggle FullScreen", this, SIGNAL(enteredFullScreenMode(bool)));
    toggleFullScreen->setShortcut(Qt::Key_F11);
    toggleFullScreen->setCheckable(true);
    toggleFullScreen->setChecked(false);
    // When exit fullscreen mode by clicking on the exit area (bottom right corner) we must
    // uncheck the Toggle FullScreen action.
    toggleFullScreen->connect(this, SIGNAL(enteredFullScreenMode(bool)), SLOT(setChecked(bool)));
    connect(this, SIGNAL(enteredFullScreenMode(bool)), this, SLOT(toggleFullScreenMode(bool)));

    QMenu* toolsMenu = menuBar()->addMenu("&Develop");
    toolsMenu->addAction("Change User Agent", this, SLOT(showUserAgentDialog()));
    toolsMenu->addSeparator();
    toolsMenu->addAction("Load URLs from file", this, SLOT(loadURLListFromFile()));

    m_addressBar = new QLineEdit();
    connect(m_addressBar, SIGNAL(returnPressed()), SLOT(changeLocation()));

    QToolBar* bar = addToolBar("Navigation");
    bar->addAction(m_browser->navigationAction(QtWebKit::Back));
    bar->addAction(m_browser->navigationAction(QtWebKit::Forward));
    bar->addAction(m_browser->navigationAction(QtWebKit::Reload));
    bar->addAction(m_browser->navigationAction(QtWebKit::Stop));
    bar->addWidget(m_addressBar);

    QShortcut* selectAddressBar = new QShortcut(Qt::CTRL | Qt::Key_L, this);
    connect(selectAddressBar, SIGNAL(activated()), this, SLOT(openLocation()));

#if 0
    // FIXME
    page()->setCreateNewPageFunction(newPageFunction);
#endif

    if (m_windowOptions.startMaximized)
        setWindowState(windowState() | Qt::WindowMaximized);
    else
        resize(800, 600);
    show();
}

void BrowserWindow::load(const QString& url)
{
    m_addressBar->setText(url);
    m_browser->load(url);
}

QObject* BrowserWindow::webView() const
{
    return m_browser->touchWebView() ? static_cast<QObject*>(m_browser->touchWebView()->page()) : static_cast<QObject*>(m_browser->desktopWebView());
}

BrowserWindow* BrowserWindow::newWindow(const QString& url)
{
    BrowserWindow* window = new BrowserWindow();
    window->load(url);
    return window;
}

void BrowserWindow::openLocation()
{
    m_addressBar->selectAll();
    m_addressBar->setFocus();
}

void BrowserWindow::changeLocation()
{
    QString string = m_addressBar->text();
    m_browser->load(string);
}

void BrowserWindow::loadProgress(int progress)
{
    QColor backgroundColor = QApplication::palette().color(QPalette::Base);
    QColor progressColor = QColor(120, 180, 240);
    QPalette pallete = m_addressBar->palette();

    if (progress <= 0 || progress >= 100)
        pallete.setBrush(QPalette::Base, backgroundColor);
    else {
        QLinearGradient gradient(0, 0, width(), 0);
        gradient.setColorAt(0, progressColor);
        gradient.setColorAt(((double) progress) / 100, progressColor);
        if (progress != 100)
            gradient.setColorAt((double) progress / 100 + 0.001, backgroundColor);
        pallete.setBrush(QPalette::Base, gradient);
    }
    m_addressBar->setPalette(pallete);
}

void BrowserWindow::urlChanged(const QUrl& url)
{
    m_addressBar->setText(url.toString());
    m_browser->setFocus();
    m_browser->view()->setFocus(true);
}

void BrowserWindow::openFile()
{
    static const QString filter("HTML Files (*.htm *.html *.xhtml);;Text Files (*.txt);;Image Files (*.gif *.jpg *.png);;SVG Files (*.svg);;All Files (*)");

    QFileDialog fileDialog(this, tr("Open"), QString(), filter);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    fileDialog.setOptions(QFileDialog::ReadOnly);

    if (fileDialog.exec()) {
        QString selectedFile = fileDialog.selectedFiles()[0];
        if (!selectedFile.isEmpty())
            load(selectedFile);
    }
}

void BrowserWindow::screenshot()
{
    QPixmap pixmap = QPixmap::grabWidget(m_browser);
    QLabel* label = 0;
    label = new QLabel;
    label->setAttribute(Qt::WA_DeleteOnClose);
    label->setWindowTitle("Screenshot - Preview");
    label->setPixmap(pixmap);
    label->show();

    QString fileName = QFileDialog::getSaveFileName(label, "Screenshot", QString(), QString("PNG File (.png)"));
    if (!fileName.isEmpty()) {
        QRegExp rx("*.png");
        rx.setCaseSensitivity(Qt::CaseInsensitive);
        rx.setPatternSyntax(QRegExp::Wildcard);

        if (!rx.exactMatch(fileName))
            fileName += ".png";

        pixmap.save(fileName, "png");
        if (label)
            label->setWindowTitle(QString("Screenshot - Saved at %1").arg(fileName));
    }
}

void BrowserWindow::toggleFullScreenMode(bool enable)
{
    bool alreadyEnabled = windowState() & Qt::WindowFullScreen;
    if (enable ^ alreadyEnabled)
        setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void BrowserWindow::showUserAgentDialog()
{
#if 0
    // FIXME
    updateUserAgentList();

    QDialog dialog(this);
    dialog.setWindowTitle("Change User Agent");
    dialog.resize(size().width() * 0.7, dialog.size().height());
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    dialog.setLayout(layout);

    QComboBox* combo = new QComboBox(&dialog);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    combo->setEditable(true);
    combo->insertItems(0, m_userAgentList);
    layout->addWidget(combo);

    int index = combo->findText(page()->customUserAgent());
    combo->setCurrentIndex(index);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                                      , Qt::Horizontal, &dialog);
    connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttonBox);

    if (dialog.exec() && !combo->currentText().isEmpty())
        page()->setCustomUserAgent(combo->currentText());
#endif
}

void BrowserWindow::loadURLListFromFile()
{
    QString selectedFile;
    selectedFile = QFileDialog::getOpenFileName(this, tr("Load URL list from file")
                                                       , QString(), tr("Text Files (*.txt);;All Files (*)"));
    if (selectedFile.isEmpty())
       return;

    m_urlLoader = new UrlLoader(this, selectedFile, 0, 0);
    m_urlLoader->loadNext();
}

void BrowserWindow::printURL(const QUrl& url)
{
    QTextStream output(stdout);
    output << "Loaded: " << url.toString() << endl;
}

void BrowserWindow::updateUserAgentList()
{
#if 0
    // FIXME
    QFile file(":/useragentlist.txt");

    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            QString agent = file.readLine().trimmed();
            if (!m_userAgentList.contains(agent))
                m_userAgentList << agent;
        }
        file.close();
    }

    Q_ASSERT(!m_userAgentList.isEmpty());
    QWKPage* wkPage = page();
    if (!(wkPage->customUserAgent().isEmpty() || m_userAgentList.contains(wkPage->customUserAgent())))
        m_userAgentList << wkPage->customUserAgent();
#endif
}

BrowserWindow::~BrowserWindow()
{
    delete m_urlLoader;
    delete m_addressBar;
    delete m_browser;
}
