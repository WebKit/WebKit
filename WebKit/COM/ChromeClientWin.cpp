/*
 * Copyright (C) 2006 Marvin Decker <marv.decker@gmail.com>
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

#include "config.h"
#include "ChromeClientWin.h"

#pragma warning(push, 0)
#include "FloatRect.h"
#pragma warning(pop)

#define notImplemented() {}

ChromeClientWin::~ChromeClientWin()
{
}

void ChromeClientWin::chromeDestroyed()
{
}

void ChromeClientWin::setWindowRect(const WebCore::FloatRect&)
{
}

WebCore::FloatRect ChromeClientWin::windowRect()
{
    notImplemented();
    return WebCore::FloatRect();
}

WebCore::FloatRect ChromeClientWin::pageRect()
{
    notImplemented();
    return WebCore::FloatRect();
}

float ChromeClientWin::scaleFactor()
{
    notImplemented();
    return 0.0;
}

void ChromeClientWin::focus()
{
    notImplemented();
}

void ChromeClientWin::unfocus()
{
    notImplemented();
}

WebCore::Page* ChromeClientWin::createWindow(const WebCore::FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

WebCore::Page* ChromeClientWin::createModalDialog(const WebCore::FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

void ChromeClientWin::show()
{
    notImplemented();
}

bool ChromeClientWin::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientWin::runModal()
{
    notImplemented();
}

void ChromeClientWin::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWin::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWin::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWin::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWin::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWin::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWin::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWin::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWin::setResizable(bool)
{
    notImplemented();
}

void ChromeClientWin::addMessageToConsole(const WebCore::String&,
                                          unsigned int,
                                          const WebCore::String&)
{
    notImplemented();
}

bool ChromeClientWin::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClientWin::runBeforeUnloadConfirmPanel(const WebCore::String&,
                                                  WebCore::Frame*)
{
    notImplemented();
    return false;
}

void ChromeClientWin::closeWindowSoon()
{
    notImplemented();
}
