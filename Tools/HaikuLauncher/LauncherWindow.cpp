/*
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
 * Copyright (C) 2010 Rene Gollent <rene@gollent.com>
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

#include "LauncherWindow.h"

#include "AuthenticationPanel.h"
#include "LauncherApp.h"
#include "WebPage.h"
#include "WebView.h"
#include "WebViewConstants.h"

#include <interface/StringView.h>
#include <Button.h>
#include <Entry.h>
#include <FilePanel.h>
#include <GridLayoutBuilder.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <StatusBar.h>
#include <StringView.h>
#include <TextControl.h>

#include <stdio.h>

enum {
	OPEN_LOCATION = 'open',
	OPEN_INSPECTOR = 'insp',
	SAVE_PAGE = 'save',
    GO_BACK = 'goba',
    GO_FORWARD = 'gofo',
    STOP = 'stop',
    GOTO_URL = 'goul',
    RELOAD = 'reld',

    TEXT_SIZE_INCREASE = 'tsin',
    TEXT_SIZE_DECREASE = 'tsdc',
    TEXT_SIZE_RESET = 'tsrs',
};

LauncherWindow::LauncherWindow(BRect frame, ToolbarPolicy toolbarPolicy)
    : BWebWindow(frame, "HaikuLauncher",
        B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
        B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
	init(new BWebView("web view"), toolbarPolicy);
}

LauncherWindow::LauncherWindow(BRect frame, BWebView* webView,
		ToolbarPolicy toolbarPolicy)
    : BWebWindow(frame, "HaikuLauncher",
        B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
        B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
	init(webView, toolbarPolicy);
}

LauncherWindow::~LauncherWindow()
{
    delete m_saveFilePanel;
}

void LauncherWindow::DispatchMessage(BMessage* message, BHandler* target)
{
	if (m_url && message->what == B_KEY_DOWN && target == m_url->TextView()) {
		// Handle B_RETURN in the URL text control. This is the easiest
		// way to react *only* when the user presses the return key in the
		// address bar, as opposed to trying to load whatever is in there when
		// the text control just goes out of focus.
	    const char* bytes;
	    if (message->FindString("bytes", &bytes) == B_OK
	    	&& bytes[0] == B_RETURN) {
	    	// Do it in such a way that the user sees the Go-button go down.
	    	m_goButton->SetValue(B_CONTROL_ON);
	    	UpdateIfNeeded();
	    	m_goButton->Invoke();
	    	snooze(1000);
	    	m_goButton->SetValue(B_CONTROL_OFF);
	    }
	}
	BWebWindow::DispatchMessage(message, target);
}

void LauncherWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
    case OPEN_LOCATION:
        if (m_url) {
        	if (m_url->TextView()->IsFocus())
        	    m_url->TextView()->SelectAll();
        	else
        	    m_url->MakeFocus(true);
        }
    	break;
    case OPEN_INSPECTOR: {
        // FIXME: wouldn't the view better be in the same window?
        BRect frame = Frame();
        frame.OffsetBy(20, 20);
        LauncherWindow* inspectorWindow = new LauncherWindow(frame);
        inspectorWindow->Show();

        CurrentWebView()->SetInspectorView(inspectorWindow->CurrentWebView());
        break;
    }
	case SAVE_PAGE: {
		BMessage* message = new BMessage(B_SAVE_REQUESTED);
		message->AddPointer("page", CurrentWebView()->WebPage());
        
        if (m_saveFilePanel == NULL) {
	        m_saveFilePanel = new BFilePanel(B_SAVE_PANEL, NULL, NULL,
                B_DIRECTORY_NODE, false);
        }

        m_saveFilePanel->SetSaveText(CurrentWebView()->WebPage()->MainFrameTitle());
        m_saveFilePanel->SetMessage(message);
		m_saveFilePanel->Show();
		break;
	}

    case RELOAD:
        CurrentWebView()->Reload();
        break;
    case GOTO_URL: {
        BString url;
        if (message->FindString("url", &url) != B_OK)
        	url = m_url->Text();
        CurrentWebView()->LoadURL(url.String());
        break;
    }
    case GO_BACK:
        CurrentWebView()->GoBack();
        break;
    case GO_FORWARD:
        CurrentWebView()->GoForward();
        break;
    case STOP:
        CurrentWebView()->StopLoading();
        break;

    case B_SIMPLE_DATA: {
        // User possibly dropped files on this window.
        // If there is more than one entry_ref, let the app handle it (open one
        // new page per ref). If there is one ref, open it in this window.
        type_code type;
        int32 countFound;
        if (message->GetInfo("refs", &type, &countFound) != B_OK
            || type != B_REF_TYPE) {
            break;
        }
        if (countFound > 1) {
            message->what = B_REFS_RECEIVED;
            be_app->PostMessage(message);
            break;
        }
        entry_ref ref;
        if (message->FindRef("refs", &ref) != B_OK)
            break;
        BEntry entry(&ref, true);
        BPath path;
        if (!entry.Exists() || entry.GetPath(&path) != B_OK)
            break;
        CurrentWebView()->LoadURL(path.Path());
        break;
    }

    case TEXT_SIZE_INCREASE:
        CurrentWebView()->IncreaseZoomFactor(true);
        break;
    case TEXT_SIZE_DECREASE:
        CurrentWebView()->DecreaseZoomFactor(true);
        break;
    case TEXT_SIZE_RESET:
        CurrentWebView()->ResetZoomFactor();
        break;

    default:
        BWebWindow::MessageReceived(message);
        break;
    }
}

bool LauncherWindow::QuitRequested()
{
    BMessage message(WINDOW_CLOSED);
    message.AddRect("window frame", Frame());
    be_app->PostMessage(&message);
    return BWebWindow::QuitRequested();
}

// #pragma mark - Notification API

void LauncherWindow::NavigationRequested(const BString& url, BWebView* view)
{
}

void LauncherWindow::NewWindowRequested(const BString& url, bool primaryAction)
{
    // Always open new windows in the application thread, since
    // creating a BWebView will try to grab the application lock.
    // But our own WebPage may already try to lock us from within
    // the application thread -> dead-lock. Thus we can't wait for
    // a reply here.
    BMessage message(NEW_WINDOW);
    message.AddString("url", url);
    be_app->PostMessage(&message);
}

void LauncherWindow::NewPageCreated(BWebView* view, BRect windowFrame,
	bool modalDialog, bool resizable, bool activate)
{
	if (!windowFrame.IsValid())
		windowFrame = Frame().OffsetByCopy(10, 10);
	LauncherWindow* window = new LauncherWindow(windowFrame, view, HaveToolbar);
	window->Show();
}

void LauncherWindow::LoadNegotiating(const BString& url, BWebView* view)
{
    BString status("Requesting: ");
    status << url;
    StatusChanged(status, view);
}

void LauncherWindow::LoadCommitted(const BString& url, BWebView* view)
{
	// This hook is invoked when the load is commited.
    if (m_url)
        m_url->SetText(url.String());

    BString status("Loading: ");
    status << url;
    StatusChanged(status, view);

    NavigationCapabilitiesChanged(m_BackButton->IsEnabled(),
        m_ForwardButton->IsEnabled(), true, view);
}

void LauncherWindow::LoadProgress(float progress, BWebView* view)
{
    if (m_loadingProgressBar) {
        if (progress < 100 && m_loadingProgressBar->IsHidden())
            m_loadingProgressBar->Show();
        m_loadingProgressBar->SetTo(progress);
    }
}

void LauncherWindow::LoadFailed(const BString& url, BWebView* view)
{
    BString status(url);
    status << " failed.";
    StatusChanged(status, view);
    if (m_loadingProgressBar && !m_loadingProgressBar->IsHidden())
        m_loadingProgressBar->Hide();
}

void LauncherWindow::LoadFinished(const BString& url, BWebView* view)
{
    // Update the URL again to handle cases where LoadCommitted is not called
    // (for example when navigating to anchors in the same page).
    if (m_url)
        m_url->SetText(url.String());

    BString status(url);
    status << " finished.";
    StatusChanged(status, view);
    if (m_loadingProgressBar && !m_loadingProgressBar->IsHidden())
        m_loadingProgressBar->Hide();

    NavigationCapabilitiesChanged(m_BackButton->IsEnabled(),
        m_ForwardButton->IsEnabled(), false, view);
}

void LauncherWindow::SetToolBarsVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::SetStatusBarVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::SetMenuBarVisible(bool flag, BWebView* view)
{
    // TODO
}

void LauncherWindow::TitleChanged(const BString& title, BWebView* view)
{
    updateTitle(title);
}

void LauncherWindow::StatusChanged(const BString& statusText, BWebView* view)
{
    if (m_statusText)
        m_statusText->SetText(statusText.String());
}

void LauncherWindow::NavigationCapabilitiesChanged(bool canGoBackward,
    bool canGoForward, bool canStop, BWebView* view)
{
    if (m_BackButton)
        m_BackButton->SetEnabled(canGoBackward);
    if (m_ForwardButton)
        m_ForwardButton->SetEnabled(canGoForward);
    if (m_StopButton)
        m_StopButton->SetEnabled(canStop);
}


bool
LauncherWindow::AuthenticationChallenge(BString message, BString& inOutUser,
	BString& inOutPassword, bool& inOutRememberCredentials,
	uint32 failureCount, BWebView* view)
{
	AuthenticationPanel* panel = new AuthenticationPanel(Frame());
		// Panel auto-destructs.
	bool success = panel->getAuthentication(message, inOutUser, inOutPassword,
		inOutRememberCredentials, failureCount > 0, inOutUser, inOutPassword,
		&inOutRememberCredentials);
	return success;
}


void LauncherWindow::init(BWebView* webView, ToolbarPolicy toolbarPolicy)
{
	SetCurrentWebView(webView);

    if (toolbarPolicy == HaveToolbar) {
        // Menu
        m_menuBar = new BMenuBar("Main menu");
        BMenu* menu = new BMenu("Window");
        BMessage* newWindowMessage = new BMessage(NEW_WINDOW);
        newWindowMessage->AddString("url", "");
        BMenuItem* newItem = new BMenuItem("New", newWindowMessage, 'N');
        menu->AddItem(newItem);
        newItem->SetTarget(be_app);
        menu->AddItem(new BMenuItem("Open location", new BMessage(OPEN_LOCATION), 'L'));
        menu->AddItem(new BMenuItem("Inspect page", new BMessage(OPEN_INSPECTOR), 'I'));
	    menu->AddItem(new BMenuItem("Save page", new BMessage(SAVE_PAGE), 'S'));
        menu->AddSeparatorItem();
        menu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED), 'W', B_SHIFT_KEY));
        BMenuItem* quitItem = new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q');
        menu->AddItem(quitItem);
        quitItem->SetTarget(be_app);
        m_menuBar->AddItem(menu);

        menu = new BMenu("Text");
        menu->AddItem(new BMenuItem("Increase size", new BMessage(TEXT_SIZE_INCREASE), '+'));
        menu->AddItem(new BMenuItem("Decrease size", new BMessage(TEXT_SIZE_DECREASE), '-'));
        menu->AddItem(new BMenuItem("Reset size", new BMessage(TEXT_SIZE_RESET), '0'));
        m_menuBar->AddItem(menu);

        // Back, Forward & Stop
        m_BackButton = new BButton("Back", new BMessage(GO_BACK));
        m_ForwardButton = new BButton("Forward", new BMessage(GO_FORWARD));
        m_StopButton = new BButton("Stop", new BMessage(STOP));

        // URL
        m_url = new BTextControl("url", "", "", NULL);

        // Go
        m_goButton = new BButton("", "Go", new BMessage(GOTO_URL));

        // Status Bar
        m_statusText = new BStringView("status", "");
        m_statusText->SetAlignment(B_ALIGN_LEFT);
        m_statusText->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
        m_statusText->SetExplicitMinSize(BSize(150, 12));
            // Prevent the window from growing to fit a long status message...
        BFont font(be_plain_font);
        font.SetSize(ceilf(font.Size() * 0.8));
        m_statusText->SetFont(&font, B_FONT_SIZE);

        // Loading progress bar
        m_loadingProgressBar = new BStatusBar("progress");
        m_loadingProgressBar->SetMaxValue(100);
        m_loadingProgressBar->Hide();
        m_loadingProgressBar->SetBarHeight(12);

        const float kInsetSpacing = 5;
        const float kElementSpacing = 7;

        // Layout
        AddChild(BGroupLayoutBuilder(B_VERTICAL)
            .Add(m_menuBar)
            .Add(BGridLayoutBuilder(kElementSpacing, kElementSpacing)
                .Add(m_BackButton, 0, 0)
                .Add(m_ForwardButton, 1, 0)
                .Add(m_StopButton, 2, 0)
                .Add(m_url, 3, 0)
                .Add(m_goButton, 4, 0)
                .SetInsets(kInsetSpacing, kInsetSpacing, kInsetSpacing, kInsetSpacing)
            )
            .Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
            .Add(webView)
            .Add(new BSeparatorView(B_HORIZONTAL, B_PLAIN_BORDER))
            .Add(BGroupLayoutBuilder(B_HORIZONTAL, kElementSpacing)
                .Add(m_statusText)
                .Add(m_loadingProgressBar, 0.2)
                .AddStrut(12 - kElementSpacing)
                .SetInsets(kInsetSpacing, 0, kInsetSpacing, 0)
            )
        );

        m_url->MakeFocus(true);
    } else {
        m_BackButton = 0;
        m_ForwardButton = 0;
        m_StopButton = 0;
        m_goButton = 0;
        m_url = 0;
        m_menuBar = 0;
        m_statusText = 0;
        m_loadingProgressBar = 0;

        AddChild(BGroupLayoutBuilder(B_VERTICAL)
            .Add(webView)
        );
    }

    m_saveFilePanel = 0;

    AddShortcut('R', B_COMMAND_KEY, new BMessage(RELOAD));

    be_app->PostMessage(WINDOW_OPENED);
}

void LauncherWindow::updateTitle(const BString& title)
{
    BString windowTitle = title;
    if (windowTitle.Length() > 0)
        windowTitle << " - ";
    windowTitle << "HaikuLauncher";
    SetTitle(windowTitle.String());
}
