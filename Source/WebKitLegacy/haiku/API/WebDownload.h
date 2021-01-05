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
#ifndef _WEB_DOWNLOAD_H_
#define _WEB_DOWNLOAD_H_

#include <Handler.h>


namespace BPrivate {
class WebDownloadPrivate;
}

class BMessenger;
class BPath;
class BString;
class BWebPage;


enum {
	B_DOWNLOAD_STARTED = 'dwns',
    B_DOWNLOAD_PROGRESS = 'dwnp'
};

enum {
	B_DOWNLOAD_FINISHED = 0,
	B_DOWNLOAD_FAILED,
	B_DOWNLOAD_BLOCKED,
	B_DOWNLOAD_CANNOT_SHOW_URL
};


class __attribute__ ((visibility ("default"))) BWebDownload : public BHandler {
// TODO: Inherit from BReferenceable.
public:
			void				Start(const BPath& path);
			void				Cancel();

			void				HasMovedTo(const BPath& path);

			void				SetProgressListener(const BMessenger& listener);

			const BString&		URL() const;
			const BPath&		Path() const;
			const BString&		Filename() const;

			off_t				CurrentSize() const;
			off_t				ExpectedSize() const;

private:
			friend class BWebPage;
			friend class BPrivate::WebDownloadPrivate;

								BWebDownload(BPrivate::WebDownloadPrivate* data);
								~BWebDownload();

private:
	virtual	void				MessageReceived(BMessage* message);

			void				_HandleCancel();

private:
			BPrivate::WebDownloadPrivate* fData;
};

#endif // _WEB_DOWNLOAD_H_
