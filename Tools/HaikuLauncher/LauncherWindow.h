/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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

#ifndef LauncherWindow_h
#define LauncherWindow_h

#include "WebWindow.h"
#include <Messenger.h>
#include <String.h>

class BButton;
class BCheckBox;
class BFilePanel;
class BLayoutItem;
class BMenuBar;
class BStatusBar;
class BStringView;
class BTextControl;
class BWebView;

enum ToolbarPolicy {
    HaveToolbar,
    DoNotHaveToolbar
};

enum {
    NEW_WINDOW = 'nwnd',
    WINDOW_OPENED = 'wndo',
    WINDOW_CLOSED = 'wndc',
};

class LauncherWindow : public BWebWindow {
public:
    LauncherWindow(BRect frame, ToolbarPolicy = HaveToolbar);
    LauncherWindow(BRect frame, BWebView* view, ToolbarPolicy = HaveToolbar);
    virtual ~LauncherWindow();

	virtual void DispatchMessage(BMessage* message, BHandler* target);
    virtual void MessageReceived(BMessage* message);
    virtual bool QuitRequested();

private:
    // WebPage notification API implementations
    virtual void NavigationRequested(const BString& url, BWebView* view) override;
    virtual void NewWindowRequested(const BString& url, bool primaryAction) override;
	virtual void NewPageCreated(BWebView* view, BRect windowFrame,
		bool modalDialog, bool resizable, bool activate) override;
    virtual void LoadNegotiating(const BString& url, BWebView* view) override;
    virtual void LoadCommitted(const BString& url, BWebView* view) override;
    virtual void LoadProgress(float progress, BWebView* view) override;
    virtual void LoadFailed(const BString& url, BWebView* view) override;
    virtual void LoadFinished(const BString& url, BWebView* view) override;
    virtual void TitleChanged(const BString& title, BWebView* view) override;
    virtual void SetToolBarsVisible(bool flag, BWebView* view) override;
    virtual void SetStatusBarVisible(bool flag, BWebView* view) override;
    virtual void SetMenuBarVisible(bool flag, BWebView* view) override;
    virtual void StatusChanged(const BString& status, BWebView* view) override;
    virtual void NavigationCapabilitiesChanged(bool canGoBackward,
        bool canGoForward, bool canStop, BWebView* view) override;
	virtual	bool				AuthenticationChallenge(BString message,
									BString& inOutUser, BString& inOutPassword,
									bool& inOutRememberCredentials,
									uint32 failureCount, BWebView* view);

    void init(BWebView* view, ToolbarPolicy);
    void updateTitle(const BString& title);

private:
    BMenuBar* m_menuBar;
    BButton* m_BackButton;
    BButton* m_ForwardButton;
    BButton* m_StopButton;
    BButton* m_goButton;
    BTextControl* m_url;
    BStringView* m_statusText;
    BStatusBar* m_loadingProgressBar;
    BFilePanel* m_saveFilePanel;
};

#endif // LauncherWindow_h

