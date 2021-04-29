/*
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
#ifndef _WEB_VIEW_H_
#define _WEB_VIEW_H_


#include <String.h>
#include <View.h>
#include <UrlContext.h>

#include <memory>

class BWebPage;

namespace WebCore {
    class ChromeClientHaiku;
    class DumpRenderTreeClient;
    class GraphicsLayer;
    class AcceleratedCompositingContext;
}


class __attribute__ ((visibility ("default"))) BWebView : public BView {
public:
	class UserData {
	public:
		virtual					~UserData();
	};

public:
								BWebView(const char* name, BPrivate::Network::BUrlContext* context = nullptr);
	virtual						~BWebView();

	// The BWebView needs to be deleted by the BWebPage instance running
	// on the application thread in order to prevent possible race conditions.
	// Call Shutdown() to initiate the deletion.
			void				Shutdown();

	// BView hooks
	virtual	void				AttachedToWindow();
	virtual	void				DetachedFromWindow();

	virtual	void				Show();
	virtual	void				Hide();

	virtual	void				Draw(BRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				GetPreferredSize(float* width, float* height);
	virtual	void				MessageReceived(BMessage* message);

	virtual	void				MakeFocus(bool focused = true);
	virtual	void				WindowActivated(bool activated);

	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);

	virtual	void				KeyDown(const char* bytes, int32 numBytes);
	virtual	void				KeyUp(const char* bytes, int32 numBytes);

	virtual	void				Pulse();

	// BWebPage API exposure
			BWebPage*			WebPage() const { return fWebPage; }

			BString				MainFrameTitle() const;
			BString				MainFrameRequestedURL() const;
			BString				MainFrameURL() const;

			void				LoadURL(const char* urlString,
									bool aquireFocus = true);
			void				Reload();
			void				GoBack();
			void				GoForward();
			void				StopLoading();

			void				IncreaseZoomFactor(bool textOnly);
			void				DecreaseZoomFactor(bool textOnly);
			void				ResetZoomFactor();

			void				FindString(const char* string,
									bool forward = true,
									bool caseSensitive = false,
									bool wrapSelection = true,
									bool startInSelection = false);

	// BWebview API
			void				SetAutoHidePointer(bool doIt);

			void				SetUserData(UserData* cookie);
			UserData*			GetUserData() const;

            void                SetInspectorView(BWebView* inspector);
            BWebView*           GetInspectorView();

            void                SetRootLayer(WebCore::GraphicsLayer* layer);
private:
	friend class BWebPage;
    friend class WebCore::DumpRenderTreeClient;
    friend class WebCore::ChromeClientHaiku;
    friend class WebCore::AcceleratedCompositingContext;

            inline BBitmap*     OffscreenBitmap() const
                                    { return fOffscreenBitmap; }
            inline  BView*      OffscreenView() const
                                    { return fOffscreenView; }
			void				SetOffscreenViewClean(BRect cleanRect,
									bool immediate);

            bool                IsComposited();

private:
            void                _ResizeOffscreenView(int width, int height);
			void				_DispatchMouseEvent(const BPoint& where,
									uint32 sanityWhat);
			void				_DispatchKeyEvent(uint32 sanityWhat);
private:
			uint32				fLastMouseButtons;
			bigtime_t			fLastMouseMovedTime;
			BPoint				fLastMousePos;
			bool				fAutoHidePointer;

            BBitmap*            fOffscreenBitmap;
            BView*              fOffscreenView;

			BWebPage*			fWebPage;

			UserData*			fUserData;
            BWebView*           fInspectorView;
};

#endif // _WEB_VIEW_H_
