/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MainWindow.h"

#include "ImageButton.h"
#include "TitleBar.h"
#include "ToolkittenUtils.h"
#include "URLBar.h"
#include "WebViewWindow.h"
#include <KeyboardCodes.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKString.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <sstream>
#include <toolkitten/Application.h>
#include <vector>

constexpr int LineHeight = 40;
constexpr int FontSize = 32;

using namespace toolkitten;

MainWindow::MainWindow(const std::vector<std::string>& options)
{
    IntSize size = Application::singleton().windowSize();
    auto windowWidth = size.w;
    auto windowHeight = size.h;
    setSize(size);

    const unsigned titleBarHeight = LineHeight * 1;
    const unsigned urlBarHeight = LineHeight * 1;
    const unsigned webViewFrameYPosition = titleBarHeight + urlBarHeight;

    // Register widgets. DO NOT CHANGE this order.
    m_webviewFrame = createAndAppendChild<Widget>(this,
        { windowWidth, windowHeight - webViewFrameYPosition },
        { 0, webViewFrameYPosition });

    m_titleBar = createAndAppendChild<TitleBar>(this,
        { windowWidth, titleBarHeight },
        { 0, 0 });

    const int closeButtonMargin = 4;
    auto closeButton = createAndAppendChild<::ImageButton>(this,
        { titleBarHeight - closeButtonMargin * 2, titleBarHeight - closeButtonMargin * 2 },
        { (int)(windowWidth - titleBarHeight + closeButtonMargin), closeButtonMargin });
    closeButton->setImageFromResource("icon_cross.png");
    closeButton->setClient({[this](::ImageButton*) {
        closeWebView(activeWebView());
    }});

    const unsigned buttonWidth = 100;
    const unsigned urlBarWidth = windowWidth - buttonWidth * 3;
    const unsigned urlBarXPosition = buttonWidth * 3;

    auto backButton = createAndAppendChild<::ImageButton>(this,
        { buttonWidth, urlBarHeight },
        { 0, titleBarHeight });
    backButton->setImageFromResource("icon_back.png");
    backButton->setClient({ [this](::ImageButton*) {
        activeWebView()->goBack();
    }});

    auto forwardButton = createAndAppendChild<::ImageButton>(this,
        { buttonWidth, urlBarHeight },
        { buttonWidth, titleBarHeight });
    forwardButton->setImageFromResource("icon_forward.png");
    forwardButton->setClient({ [this](::ImageButton*) {
        activeWebView()->goForward();
    }});

    auto reloadButton = createAndAppendChild<::ImageButton>(this,
        { buttonWidth, urlBarHeight },
        { buttonWidth * 2, titleBarHeight });
    reloadButton->setImageFromResource("icon_reload.png");
    reloadButton->setClient({ [this](::ImageButton*) {
        activeWebView()->reload();
    }});

    m_urlBar = createAndAppendChild<::URLBar>(this,
        { urlBarWidth, urlBarHeight },
        { urlBarXPosition, titleBarHeight });
    m_urlBar->setClient({ [this](::URLBar*, const char* url) { 
        activeWebView()->loadURL(url);
    }});

    createNewWebView(nullptr);

    std::string requestedURL = "https://webkit.org";

    parseOptions(options, requestedURL);

    activeWebView()->loadURL(requestedURL.c_str());
}

void MainWindow::parseOptions(const std::vector<std::string>& options, std::string& requestedURL)
{
    auto context = WebContext::singleton();
    WKWebsiteDataStoreRef websiteDataStore = context->websiteDataStore();
    for (auto& option : options) {
        if (!option.compare("--resource-load-statistics-enabled"))
            WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(websiteDataStore, true);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
        else if (!option.compare("--resource-load-statistics-disabled"))
            WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(websiteDataStore, false);
        else
            requestedURL = option;
    }
}

void MainWindow::paintSelf(toolkitten::IntPoint)
{
}

WebViewWindow* MainWindow::createNewWebView(WKPageConfigurationRef configuration)
{
    IntSize size = m_webviewFrame->size();
    IntPoint position = { 0, 0 };

    WebViewWindow::Client windowClient {
        // createNewWindow
        [this](WKPageConfigurationRef configuration) -> WebViewWindow* {
            return createNewWebView(configuration);
        },

        // didUpdateTitle
        [this](WebViewWindow* view) {
            if (view == activeWebView())
                updateTitleBar();
        },
        // didUpdateURL
        [this](WebViewWindow* view) {
            if (view == activeWebView())
                updateURLBar();
        },
        // didUpdateProgress
        [this](WebViewWindow* view) {
            if (view == activeWebView())
                updateTitleBar();
        }
    };

    auto webViewWindow = WebViewWindow::create(std::move(windowClient), configuration);
    webViewWindow->setUserData(this);
    webViewWindow->setPosition(position);
    webViewWindow->setSize({ size.w, size.h });
    webViewWindow->fill(WHITE);

    WebViewWindow* rawWebViewWindow = webViewWindow.get();
    m_webviewFrame->appendChild(std::move(webViewWindow));

    // Acrivate the new window.
    rawWebViewWindow->setActive(true);
    setActiveWebView(rawWebViewWindow);
    return rawWebViewWindow;
}

WebViewWindow* MainWindow::activeWebView()
{
    auto numWebViews = m_webviewFrame->children().size();
    if (m_activeWebViewIndex >= numWebViews)
        m_activeWebViewIndex = numWebViews - 1;

    auto it = m_webviewFrame->children().begin();
    std::advance(it, m_activeWebViewIndex);
    return static_cast<WebViewWindow*>(it->get());
}

void MainWindow::setActiveWebView(WebViewWindow* view)
{
    auto& webViews = m_webviewFrame->children();
    unsigned i = 0;
    for (auto it = webViews.begin(); it != webViews.end(); it++, i++) {
        if (it->get() == view) {
            m_activeWebViewIndex = i;
            static_cast<WebViewWindow*>(it->get())->setActive(true);
        } else
            static_cast<WebViewWindow*>(it->get())->setActive(false);
    }
    updateTitleBar();
    updateURLBar();
}

void MainWindow::switchActiveWebView(int delta)
{
    auto numWebViews = m_webviewFrame->children().size();
    unsigned newIndex = (m_activeWebViewIndex + delta + numWebViews) % numWebViews;
    auto it = m_webviewFrame->children().begin();
    std::advance(it, newIndex);
    setActiveWebView(static_cast<WebViewWindow*>(it->get()));
}

void MainWindow::closeWebView(WebViewWindow* view)
{
    auto& webViews = m_webviewFrame->children();
    unsigned i = 0;
    for (auto it = webViews.begin(); it != webViews.end(); it++, i++) {
        if (it->get() == view) {
            m_webviewFrame->removeChild(view);
            if (m_webviewFrame->children().empty())
                exit(0);

            if (i == m_activeWebViewIndex)
                switchActiveWebView(-1);
            return;
        }
    }
}

bool MainWindow::onKeyDown(int32_t virtualKeyCode)
{
    switch (virtualKeyCode) {
    case VK_L1:
        activeWebView()->goBack();
        return true;
    case VK_L2:
        switchActiveWebView(-1);
        return true;
    case VK_R1:
        activeWebView()->goForward();
        return true;
    case VK_R2:
        switchActiveWebView(1);
        return true;
    case VK_R3:
        activeWebView()->toggleZoomFactor();
        return true;
    case VK_TRIANGLE:
        activeWebView()->reload();
        return true;
    }
    return false;
}

void MainWindow::updateTitleBar()
{
    auto numWebViews = m_webviewFrame->children().size();
    std::stringstream title;
    title << (m_activeWebViewIndex + 1) << "/" << numWebViews << " " << activeWebView()->title();
    m_titleBar->setTitle(title.str());
    m_titleBar->setProgress(activeWebView()->progress());
}

void MainWindow::updateURLBar()
{
    m_urlBar->setText(activeWebView()->url().c_str(), FontSize);
}
