/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
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

#ifndef ChromeClientQt_H
#define ChromeClientQt_H

#include "ChromeClient.h"
#include "FloatRect.h"
#include "Shared.h"

class QWebPage;

namespace WebCore {

    class FloatRect;
    class Page;
    struct FrameLoadRequest;

    class ChromeClientQt : public ChromeClient,
                           public Shared<ChromeClientQt> {
    public:
        ChromeClientQt(QWebPage* webPage);
        virtual ~ChromeClientQt();
        virtual void chromeDestroyed();

        virtual void ref();
        virtual void deref();

        virtual void setWindowRect(const FloatRect&);
        virtual FloatRect windowRect();

        virtual FloatRect pageRect();

        virtual float scaleFactor();

        virtual void focus();
        virtual void unfocus();

        virtual bool canTakeFocus(FocusDirection);
        virtual void takeFocus(FocusDirection);

        virtual Page* createWindow(const FrameLoadRequest&);
        virtual Page* createModalDialog(const FrameLoadRequest&);
        virtual void show();

        virtual bool canRunModal();
        virtual void runModal();

        virtual void setToolbarsVisible(bool);
        virtual bool toolbarsVisible();

        virtual void setStatusbarVisible(bool);
        virtual bool statusbarVisible();

        virtual void setScrollbarsVisible(bool);
        virtual bool scrollbarsVisible();

        virtual void setMenubarVisible(bool);
        virtual bool menubarVisible();

        virtual void setResizable(bool);

        virtual void addMessageToConsole(const String& message, unsigned int lineNumber,
                                         const String& sourceID);

        virtual bool canRunBeforeUnloadConfirmPanel();
        virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame);

        virtual void closeWindowSoon();

        QWebPage* m_webPage;
    };
}

#endif
