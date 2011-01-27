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

static QWKPage* newPageFunction(QWKPage* page)
{
    BrowserWindow* window = new BrowserWindow(page->context());
    return window->page();
}

QGraphicsWKView::BackingStoreType BrowserWindow::backingStoreTypeForNewWindow = QGraphicsWKView::Simple;

QVector<int> BrowserWindow::m_zoomLevels;

BrowserWindow::BrowserWindow(QWKContext* context)
    : m_currentZoom(100) ,
      m_browser(new BrowserView(backingStoreTypeForNewWindow, context))
{
    setAttribute(Qt::WA_DeleteOnClose);

    connect(m_browser->view(), SIGNAL(loadProgress(int)), SLOT(loadProgress(int)));
    connect(m_browser->view(), SIGNAL(titleChanged(const QString&)), SLOT(titleChanged(const QString&)));
    connect(m_browser->view(), SIGNAL(urlChanged(const QUrl&)), SLOT(urlChanged(const QUrl&)));

    this->setCentralWidget(m_browser);
    m_browser->setFocus(Qt::OtherFocusReason);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Window", this, SLOT(newWindow()), QKeySequence::New);
    fileMenu->addAction("Open File", this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, SLOT(close()));

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(page()->action(QWKPage::Stop));
    viewMenu->addAction(page()->action(QWKPage::Reload));
    viewMenu->addSeparator();
    QAction* zoomIn = viewMenu->addAction("Zoom &In", this, SLOT(zoomIn()));
    QAction* zoomOut = viewMenu->addAction("Zoom &Out", this, SLOT(zoomOut()));
    QAction* resetZoom = viewMenu->addAction("Reset Zoom", this, SLOT(resetZoom()));

    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    QMenu* toolsMenu = menuBar()->addMenu("&Develop");
    toolsMenu->addAction("Change User Agent", this, SLOT(showUserAgentDialog()));

    m_addressBar = new QLineEdit();
    connect(m_addressBar, SIGNAL(returnPressed()), SLOT(changeLocation()));

    QToolBar* bar = addToolBar("Navigation");
    bar->addAction(page()->action(QWKPage::Back));
    bar->addAction(page()->action(QWKPage::Forward));
    bar->addAction(page()->action(QWKPage::Reload));
    bar->addAction(page()->action(QWKPage::Stop));
    bar->addWidget(m_addressBar);

    QShortcut* selectAddressBar = new QShortcut(Qt::CTRL | Qt::Key_L, this);
    connect(selectAddressBar, SIGNAL(activated()), this, SLOT(openLocation()));

    page()->setCreateNewPageFunction(newPageFunction);

    // the zoom values are chosen to be like in Mozilla Firefox 3
    if (!m_zoomLevels.count()) {
        m_zoomLevels << 30 << 50 << 67 << 80 << 90;
        m_zoomLevels << 100;
        m_zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;
    }

    resize(960, 640);
    show();
}

void BrowserWindow::load(const QString& url)
{
    m_addressBar->setText(url);
    m_browser->load(url);
}

QWKPage* BrowserWindow::page()
{
    return m_browser->view()->page();
}

BrowserWindow* BrowserWindow::newWindow(const QString& url)
{
    BrowserWindow* window = new BrowserWindow;
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

void BrowserWindow::titleChanged(const QString& title)
{
    setWindowTitle(title);
}

void BrowserWindow::urlChanged(const QUrl& url)
{
    m_addressBar->setText(url.toString());
}

void BrowserWindow::openFile()
{
#ifndef QT_NO_FILEDIALOG
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
#endif
}

void BrowserWindow::zoomIn()
{
    int i = m_zoomLevels.indexOf(m_currentZoom);
    Q_ASSERT(i >= 0);
    if (i < m_zoomLevels.count() - 1)
        m_currentZoom = m_zoomLevels[i + 1];

    applyZoom();
}

void BrowserWindow::zoomOut()
{
    int i = m_zoomLevels.indexOf(m_currentZoom);
    Q_ASSERT(i >= 0);
    if (i > 0)
        m_currentZoom = m_zoomLevels[i - 1];

    applyZoom();
}

void BrowserWindow::resetZoom()
{
    m_currentZoom = 100;
    applyZoom();
}

void BrowserWindow::showUserAgentDialog()
{
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
}

void BrowserWindow::updateUserAgentList()
{
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
}

void BrowserWindow::applyZoom()
{
    page()->setPageZoomFactor(qreal(m_currentZoom) / 100.0);
}

BrowserWindow::~BrowserWindow()
{
    delete m_addressBar;
    delete m_browser;
}
