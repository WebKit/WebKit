/*
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


#ifndef _WEB_PAGE_H
#define _WEB_PAGE_H

#include <Handler.h>
#include <Messenger.h>
#include <Rect.h>
#include <String.h>

class BNetworkCookieJar;
class BRegion;
class BUrlContext;
class BView;
class BWebDownload;
class BWebFrame;
class BWebSettings;
class BWebView;

namespace WebCore {
class ChromeClientHaiku;
class ContextMenuClientHaiku;
class DragClientHaiku;
class DumpRenderTreeClient;
class EditorClientHaiku;
class FrameLoaderClientHaiku;
class InspectorClientHaiku;
class ProgressTrackerClientHaiku;

class FrameView;
class Page;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
};

namespace BPrivate {
class WebDownloadPrivate;
};

enum {
	B_FIND_STRING_RESULT			= 'fsrs',
	B_DOWNLOAD_ADDED				= 'dwna',
	B_DOWNLOAD_REMOVED				= 'dwnr',
	B_EDITING_CAPABILITIES_RESULT	= 'cedr',
	B_PAGE_SOURCE_RESULT			= 'psrc'
};

typedef enum {
	B_WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER = 0,
	B_WEBKIT_CACHE_MODEL_WEB_BROWSER
} BWebKitCacheModel;

class __attribute__ ((visibility ("default"))) BWebPage : private BHandler {
public:
	static	void				InitializeOnce();
	static	void				ShutdownOnce();
	static	void				SetCacheModel(BWebKitCacheModel model);

			void				Init();
			void				Shutdown();

			void				SetListener(const BMessenger& listener);
	static	void				SetDownloadListener(const BMessenger& listener);

			BWebFrame*			MainFrame() const;
			BWebSettings*		Settings() const;
			BWebView*			WebView() const;
				// NOTE: Using the BWebView requires locking it's looper!

			void				LoadURL(const char* urlString);
			void				Reload();
			void				GoBack();
			void				GoForward();
			void				StopLoading();

			BString				MainFrameTitle() const;
			BString				MainFrameRequestedURL() const;
			BString				MainFrameURL() const;

            status_t            GetContentsAsMHTML(BDataIO& output);

			void				ChangeZoomFactor(float increment,
									bool textOnly);
			void				FindString(const char* string,
									bool forward = true,
									bool caseSensitive = false,
									bool wrapSelection = true,
									bool startInSelection = false);

            void                SetDeveloperExtrasEnabled(bool enable);
			void				SetStatusMessage(const BString& status);
			void				ResendNotifications();

			void				SendEditingCapabilities();
			void				SendPageSource();

            void				RequestDownload(const BString& url);

private:
	friend class BWebFrame;
	friend class BWebView;
	friend class BPrivate::WebDownloadPrivate;

								BWebPage(BWebView* webView, BUrlContext* context);

	// These calls are private, since they are called from the BWebView only.
	void setVisible(bool visible);
	void draw(const BRect& updateRect);
	void frameResized(float width, float height);
	void focused(bool focused);
	void activated(bool activated);
	void mouseEvent(const BMessage* message, const BPoint& where,
        const BPoint& screenWhere);
	void mouseWheelChanged(const BMessage* message, const BPoint& where,
        const BPoint& screenWhere);
	void keyEvent(const BMessage* message);
	void standardShortcut(const BMessage* message);

    void internalPaint(BView* offscree, WebCore::FrameView*, BRegion*);
    void scroll(int scrollDeltaX, int scrollDeltaY, const BRect& rectToScroll,
        const BRect& clipRect);

private:
    // The following methods are only supposed to be called by the
    // ChromeClientHaiku and FrameLoaderHaiku code! Not from within the window
    // thread! This coud go into a private class.
    friend class WebCore::ChromeClientHaiku;
    friend class WebCore::ContextMenuClientHaiku;
    friend class WebCore::DragClientHaiku;
    friend class WebCore::DumpRenderTreeClient;
    friend class WebCore::EditorClientHaiku;
    friend class WebCore::FrameLoaderClientHaiku;
    friend class WebCore::ProgressTrackerClientHaiku;
    friend class WebCore::InspectorClientHaiku;

    WebCore::Page* page() const;

    WebCore::Page* createNewPage(BRect frame = BRect(),
        bool modalDialog = false, bool resizable = true,
        bool activate = true);

    BUrlContext* GetContext();
    BRect windowFrame();
    BRect windowBounds();
    void setWindowBounds(const BRect& bounds);
    BRect viewBounds();
    void setViewBounds(const BRect& bounds);

    void setToolbarsVisible(bool);
    bool areToolbarsVisible() const { return fToolbarsVisible; }

    void setStatusbarVisible(bool);
    bool isStatusbarVisible() const { return fStatusbarVisible; }

    void setMenubarVisible(bool);
    bool isMenubarVisible() const { return fMenubarVisible; }

    void setResizable(bool);
    void closeWindow();
    void linkHovered(const BString&, const BString&, const BString&);

    friend class BWebDownload;

    static void downloadCreated(BWebDownload* download,
        bool isAsynchronousRequest);

    void paint(BRect rect, bool immediate);

    void setLoadingProgress(float progress);
    void setStatusMessage(const BString& message);
    void setDisplayedStatusMessage(const BString& message, bool force = false);
    void addMessageToConsole(const BString& source, int lineNumber,
        int columnNumber, const BString& message);
    void runJavaScriptAlert(const BString& message);
    bool runJavaScriptConfirm(const BString& message);
    void requestDownload(const WebCore::ResourceRequest& request,
        bool isAsynchronousRequest = true);

private:
	virtual						~BWebPage();
	virtual	void				MessageReceived(BMessage* message);

	void skipToLastMessage(BMessage*& message);

	void handleLoadURL(const BMessage* message);
	void handleReload(const BMessage* message);
	void handleGoBack(const BMessage* message);
	void handleGoForward(const BMessage* message);
	void handleStop(const BMessage* message);
	void handleSetVisible(const BMessage* message);
	void handleFrameResized(const BMessage* message);
	void handleFocused(const BMessage* message);
	void handleActivated(const BMessage* message);
	void handleMouseEvent(const BMessage* message);
	void handleMouseWheelChanged(BMessage* message);
	void handleKeyEvent(BMessage* message);
	void handleChangeZoomFactor(BMessage* message);
	void handleFindString(BMessage* message);
	void handleResendNotifications(BMessage* message);
	void handleSendEditingCapabilities(BMessage* message);
	void handleSendPageSource(BMessage* message);

    status_t dispatchMessage(BMessage& message, BMessage* reply = NULL) const;

private:
    		BMessenger			fListener;
	static	BMessenger			sDownloadListener;
			BWebView*			fWebView;
			BWebFrame*			fMainFrame;
			BWebSettings*		fSettings;
            BUrlContext*        fContext;
			WebCore::Page*		fPage;
            WebCore::DumpRenderTreeClient* fDumpRenderTree;

			float				fLoadingProgress;
			BString				fStatusMessage;
			BString				fDisplayedStatusMessage;

		    bool				fPageVisible;
		    bool				fPageDirty;
		    bool				fLayoutingView;

			bool				fToolbarsVisible;
			bool				fStatusbarVisible;
			bool				fMenubarVisible;
};

#endif // _WEB_PAGE_H
