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
#ifndef _WEB_WINDOW_H_
#define _WEB_WINDOW_H_


#include <Window.h>

class BWebView;


class __attribute__ ((visibility ("default"))) BWebWindow : public BWindow {
public:
								BWebWindow(BRect frame, const char* title,
									window_look look, window_feel feel,
									uint32 flags,
									uint32 workspace = B_CURRENT_WORKSPACE);
	virtual						~BWebWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

	// You can have as many BWebViews in your derived window as you want,
	// but for some situations, the BWebWindow needs to know the current one,
	// so always make it known here.
	virtual	void				SetCurrentWebView(BWebView* view);
			BWebView*			CurrentWebView() const;

	// Derived windows can implement this notification API.
	virtual	void				NavigationRequested(const BString& url,
									BWebView* view);
	virtual	void				NewWindowRequested(const BString& url,
									bool primaryAction);
	virtual	void				CloseWindowRequested(BWebView* view);
	virtual	void				NewPageCreated(BWebView* view,
									BRect windowFrame, bool modalDialog,
									bool resizable, bool activate);
	virtual	void				LoadNegotiating(const BString& url,
									BWebView* view);
	virtual	void				LoadCommitted(const BString& url,
									BWebView* view);
	virtual	void				LoadProgress(float progress, BWebView* view);
	virtual	void				LoadFailed(const BString& url, BWebView* view);
	virtual	void				LoadFinished(const BString& url,
									BWebView* view);
	virtual	void				MainDocumentError(const BString& failingURL,
									const BString& localizedDescription,
									BWebView* view);
	virtual	void				TitleChanged(const BString& title,
									BWebView* view);
	virtual	void				IconReceived(const BBitmap* icon,
									BWebView* view);
	virtual	void				ResizeRequested(float width, float height,
									BWebView* view);
	virtual	void				SetToolBarsVisible(bool flag, BWebView* view);
	virtual	void				SetStatusBarVisible(bool flag, BWebView* view);
	virtual	void				SetMenuBarVisible(bool flag, BWebView* view);
	virtual	void				SetResizable(bool flag, BWebView* view);
	virtual	void				StatusChanged(const BString& status,
									BWebView* view);
	virtual	void				NavigationCapabilitiesChanged(
									bool canGoBackward, bool canGoForward,
									bool canStop, BWebView* view);
	virtual	void				UpdateGlobalHistory(const BString& url);
	virtual	bool				AuthenticationChallenge(BString message,
									BString& inOutUser, BString& inOutPassword,
									bool& inOutRememberCredentials,
									uint32 failureCount, BWebView* view);

private:
			void				_FetchIconForURL(const BString& url,
									const BMessage& message);
			BWebView*			_WebViewForMessage(
									const BMessage* message) const;

private:
			BWebView*			fWebView;
};

#endif // _WEB_WINDOW_H_

