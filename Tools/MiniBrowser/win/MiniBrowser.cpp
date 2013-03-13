/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "stdafx.h"

#include "BrowserWindow.h"
#include "MiniBrowser.h"
#include <assert.h>

MiniBrowser::MiniBrowser()
    : m_instance(0)
{
}

MiniBrowser& MiniBrowser::shared()
{
    static MiniBrowser miniBrowser;

    return miniBrowser;
}

void MiniBrowser::initialize(HINSTANCE instance)
{
    assert(!m_instance);

    m_instance = instance;
}

void MiniBrowser::createNewWindow()
{
    static const wchar_t* kDefaultURLString = L"http://webkit.org/";

    BrowserWindow* browserWindow = BrowserWindow::create();
    browserWindow->createWindow(0, 0, 800, 600);
    browserWindow->showWindow();
    browserWindow->goToURL(kDefaultURLString);
}

void MiniBrowser::registerWindow(BrowserWindow* window)
{
    m_browserWindows.insert(window);
}

void MiniBrowser::unregisterWindow(BrowserWindow* window)
{
    m_browserWindows.erase(window);

    if (m_browserWindows.empty())
        ::PostQuitMessage(0);
}

bool MiniBrowser::handleMessage(const MSG* message)
{
    for (std::set<BrowserWindow*>::const_iterator it = m_browserWindows.begin(), end = m_browserWindows.end(); it != end; ++it) {
        BrowserWindow* browserWindow = *it;

        if (browserWindow->handleMessage(message))
            return true;
    }

    return false;
}
