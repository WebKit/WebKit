/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ChromeClientQt.h"

#include "Frame.h"
#include "FrameView.h"
#include "FrameLoadRequest.h"

#include <QWidget>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore
{

static QWidget* rootWindowForFrame(const Frame* frame)
{
    FrameView* frameView = (frame ? frame->view() : 0);
    if (!frameView)
        return 0;

    return frameView->qwidget();
}
    
ChromeClientQt::ChromeClientQt()
{
    
}


ChromeClientQt::~ChromeClientQt()
{
    
}

void ChromeClientQt::ref()
{
    Shared<ChromeClientQt>::ref();
}

void ChromeClientQt::deref()
{
    Shared<ChromeClientQt>::deref();
}

void ChromeClientQt::setWindowRect(const FloatRect&)
{
    notImplemented();
#if 0
    QWidget* widget = rootWindowForFrame(mainFrame());
    if (widget)
        widget->setGeometry(QRect(qRound(r.x()), qRound(r.y()),
                                  qRound(r.width()), qRound(r.height())));
#endif
}


FloatRect ChromeClientQt::windowRect()
{
#if 0
    QWidget* widget = rootWindowForFrame(mainFrame());
    if (!widget)
        return FloatRect();

    return IntRect(widget->geometry());
#endif
    notImplemented();
    return IntRect(0, 0, 100, 100);
}


FloatRect ChromeClientQt::pageRect()
{
    notImplemented();
    return FloatRect(0, 0, 100, 100);
}


float ChromeClientQt::scaleFactor()
{
    notImplemented();
    return 1;
}


void ChromeClientQt::focus()
{
    notImplemented();
}


void ChromeClientQt::unfocus()
{
    notImplemented();
}

bool ChromeClientQt::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void ChromeClientQt::takeFocus(FocusDirection)
{
    notImplemented();
}


Page* ChromeClientQt::createWindow(const FrameLoadRequest&)
{
    notImplemented();
    return 0;
}


Page* ChromeClientQt::createModalDialog(const FrameLoadRequest&)
{
    return 0;
}


void ChromeClientQt::show()
{
    notImplemented();
}


bool ChromeClientQt::canRunModal()
{
    notImplemented();
    return false;
}


void ChromeClientQt::runModal()
{
    notImplemented();
}


void ChromeClientQt::setToolbarsVisible(bool)
{
    notImplemented();
}


bool ChromeClientQt::toolbarsVisible()
{
    notImplemented();
    return false;
}


void ChromeClientQt::setStatusbarVisible(bool)
{
    notImplemented();
}


bool ChromeClientQt::statusbarVisible()
{
    notImplemented();
    return false;
}


void ChromeClientQt::setScrollbarsVisible(bool)
{
    notImplemented();
}


bool ChromeClientQt::scrollbarsVisible()
{
    notImplemented();
    return true;
}


void ChromeClientQt::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientQt::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientQt::setResizable(bool)
{
    notImplemented();
}

void ChromeClientQt::addMessageToConsole(const String& message, unsigned int lineNumber,
                                         const String& sourceID)
{
}

void ChromeClientQt::chromeDestroyed()
{
}

bool ChromeClientQt::canRunBeforeUnloadConfirmPanel()
{
}

bool ChromeClientQt::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
}

void ChromeClientQt::closeWindowSoon()
{
}

}


