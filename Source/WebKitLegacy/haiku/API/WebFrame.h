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
#ifndef _WEB_FRAME_H_
#define _WEB_FRAME_H_


#include <Point.h>
#include <String.h>

#include "WebCore/FindOptions.h"
#include <JavaScriptCore/APICast.h>

class BMessenger;
class BWebPage;

namespace WebCore {
class AcceleratedCompositingContext;
class ChromeClientHaiku;
class DumpRenderTreeClient;
class Frame;
class FrameLoaderClientHaiku;
class HTMLFrameOwnerElement;
}

namespace WTF {
class URL;
}

class WebFramePrivate;


class __attribute__ ((visibility ("default"))) BWebFrame {
public:
			void				SetListener(const BMessenger& listener);

			void				LoadURL(BString url);

			void				StopLoading();
			void				Reload();

			BString				RequestedURL() const;
			BString				URL() const;

			bool				CanCopy() const;
			bool				CanCut() const;
			bool				CanPaste() const;

			void				Copy();
			void				Cut();
			void				Paste();

			bool				CanUndo() const;
			bool				CanRedo() const;

			void				Undo();
			void				Redo();

			bool				AllowsScrolling() const;
			void				SetAllowsScrolling(bool enable);
            BPoint              ScrollPosition();

			BString				FrameSource() const;
			void				SetFrameSource(const BString& source);

			void				SetTransparent(bool transparent);
			bool				IsTransparent() const;

			BString				InnerText() const;
			BString				AsMarkup() const;
			BString				ExternalRepresentation() const;

			bool				FindString(const BString& string,
									WebCore::FindOptions options);

			bool				CanIncreaseZoomFactor() const;
			bool				CanDecreaseZoomFactor() const;

			void				IncreaseZoomFactor(bool textOnly);
			void				DecreaseZoomFactor(bool textOnly);

			void				ResetZoomFactor();

			void				SetEditable(bool editable);
			bool				IsEditable() const;

			void				SetTitle(const BString& title);
			const BString&		Title() const;

            const char*         Name() const;

			JSGlobalContextRef	GlobalContext() const;
private:
	friend class BWebView;
	friend class BWebPage;

	friend class WebCore::ChromeClientHaiku;
	friend class WebCore::DumpRenderTreeClient;
	friend class WebCore::FrameLoaderClientHaiku;
	friend class WebCore::AcceleratedCompositingContext;

								BWebFrame(BWebPage* webPage,
									BWebFrame* parentFrame,
									WebFramePrivate* data);
								~BWebFrame();

			void				LoadURL(WTF::URL);
			WebCore::Frame*		Frame() const;

            BWebFrame*          AddChild(BWebPage* page, BString name,
                                    WebCore::HTMLFrameOwnerElement* ownerElement);

private:
			float				fZoomFactor;
			bool				fIsEditable;
			BString				fTitle;
mutable		BString				fName;

			WebFramePrivate*	fData;
};

#endif // _WEB_FRAME_H_
