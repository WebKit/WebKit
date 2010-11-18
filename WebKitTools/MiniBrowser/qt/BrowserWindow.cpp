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

#include "WKPageNamespace.h"

BrowserWindow::BrowserWindow(QGraphicsWKView::BackingStoreType type)
    : m_backingStoreType(type)
{
    setAttribute(Qt::WA_DeleteOnClose);

    m_menu = new QMenuBar();
    m_browser = new BrowserView(m_backingStoreType);
    m_addressBar = new QLineEdit();

    m_menu->addAction("New Window", this, SLOT(newWindow()));
    m_menu->addAction("Change User Agent", this, SLOT(showUserAgentDialog()));

    m_menu->addSeparator();
    m_menu->addAction("Quit", this, SLOT(close()));

    m_browser->setFocus(Qt::OtherFocusReason);

    connect(m_addressBar, SIGNAL(returnPressed()), SLOT(changeLocation()));
    connect(m_browser->view(), SIGNAL(loadProgress(int)), SLOT(loadProgress(int)));
    connect(m_browser->view(), SIGNAL(titleChanged(const QString&)), SLOT(titleChanged(const QString&)));
    connect(m_browser->view(), SIGNAL(urlChanged(const QUrl&)), SLOT(urlChanged(const QUrl&)));

    QToolBar* bar = addToolBar("Navigation");
    bar->addAction(m_browser->view()->page()->action(QWKPage::Back));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Forward));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Reload));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Stop));
    bar->addWidget(m_addressBar);

    this->setMenuBar(m_menu);
    this->setCentralWidget(m_browser);

    m_browser->setFocus(Qt::OtherFocusReason);

    QShortcut* selectAddressBar = new QShortcut(Qt::CTRL | Qt::Key_L, this);
    connect(selectAddressBar, SIGNAL(activated()), this, SLOT(openLocation()));

    resize(960, 640);
    show();
}

void BrowserWindow::load(const QString& url)
{
    m_addressBar->setText(url);
    m_browser->load(url);
}

BrowserWindow* BrowserWindow::newWindow(const QString& url)
{
    BrowserWindow* window = new BrowserWindow(m_backingStoreType);
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

void BrowserWindow::updateUserAgentList()
{
    QWKPage* page = m_browser->view()->page();

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

    if (!(page->customUserAgent().isEmpty() || m_userAgentList.contains(page->customUserAgent())))
        m_userAgentList << page->customUserAgent();
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

    int index = combo->findText(m_browser->view()->page()->customUserAgent());
    combo->setCurrentIndex(index);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                                      , Qt::Horizontal, &dialog);
    connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttonBox);

    if (dialog.exec() && !combo->currentText().isEmpty())
        m_browser->view()->page()->setCustomUserAgent(combo->currentText());
}

BrowserWindow::~BrowserWindow()
{
    delete m_addressBar;
    delete m_browser;
    delete m_menu;
}
