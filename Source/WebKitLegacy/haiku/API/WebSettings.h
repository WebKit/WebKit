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
#ifndef _WEB_SETTINGS_H_
#define _WEB_SETTINGS_H_

#include <Handler.h>
#include <String.h>


class BBitmap;
class BFont;
class BMessenger;
class BWebPage;

namespace WebCore {
class Settings;
}

namespace BPrivate {
class WebSettingsPrivate;
}


enum BProxyType {
	B_PROXY_TYPE_HTTP = 0,
	B_PROXY_TYPE_SOCKS4 = 1,
	B_PROXY_TYPE_SOCKS4A = 2,
	B_PROXY_TYPE_SOCKS5 = 3,
	B_PROXY_TYPE_SOCKS5_HOSTNAME = 4
};


class __attribute__ ((visibility ("default"))) BWebSettings : public BHandler {
// TODO: Inherit from BReferenceable.
public:
	static	BWebSettings*		Default();

	// This will call all the storage methods below with default paths relative to
	// the given path. An empty path will use the default B_USER_DATA_DIRECTORY.
	static	void				SetPersistentStoragePath(const BString& path = BString());

	static	void				SetIconDatabasePath(const BString& path);
	static	void				ClearIconDatabase();

	// This method triggers sending the provided message when the favicon
	// for the given URL can be found.
	// It is dispatched to the BMessenger provided to SendIconForURL().
	// The icon will be stored as archived BBitmap "icon" in the message, which is
	// otherwise a copy of the provided message
	static  void				SendIconForURL(const BString& url,
									const BMessage& reply,
									const BMessenger& target);

	static	void				SetOfflineStoragePath(const BString& path);
	static	void				SetOfflineStorageDefaultQuota(int64 maximumSize);

	static	void				SetOfflineWebApplicationCachePath(const BString& path);
	static	void				SetOfflineWebApplicationCacheQuota(int64 maximumSize);

			void				SetLocalStoragePath(const BString& path);

			void				SetSerifFont(const BFont& font);
			void				SetSansSerifFont(const BFont& font);
			void				SetFixedFont(const BFont& font);
			void				SetStandardFont(const BFont& font);

			void				SetDefaultStandardFontSize(float size);
			void				SetDefaultFixedFontSize(float size);

			void				SetJavascriptEnabled(bool enable);

	static	void				SetProxyInfo(const BString& host = "",
									uint32 port = 0,
									BProxyType type = B_PROXY_TYPE_HTTP,
									const BString& username = "",
									const BString& password = "");

			void				Apply();

private:
			friend class BWebPage;
			friend class BPrivate::WebSettingsPrivate;

								BWebSettings();
								BWebSettings(WebCore::Settings* settings);
	virtual						~BWebSettings();

private:
	static	void				_PostSetPath(BHandler* handler, uint32 what, const BString& path);
	static	void				_PostSetQuota(BHandler* handler, uint32 what, int64 maximumSize);
			void				_PostFont(uint32 which, const BFont& font);
			void				_PostFontSize(uint32 which, float size);
	static	void				_PostMessage(BHandler* handler, BMessage* message);

	virtual	void				MessageReceived(BMessage* message);

			void				_HandleSetPersistentStoragePath(const BString& path);
			void				_HandleSetIconDatabasePath(const BString& path);
			void				_HandleClearIconDatabase();
			void				_HandleSendIconForURL(BMessage* message);
			void				_HandleSetOfflineStoragePath(const BString& path);
			void				_HandleSetOfflineStorageDefaultQuota(int64 maximumSize);
			void				_HandleSetWebApplicationCachePath(const BString& path);
			void				_HandleSetWebApplicationCacheQuota(int64 maximumSize);
			void				_HandleSetLocalStoragePath(const BString& path);
			void				_HandleSetFont(BMessage* message);
			void				_HandleSetFontSize(BMessage* message);
			void				_HandleSetProxyInfo(BMessage* message);
			void				_HandleSetJavascriptEnabled(bool);
			void				_HandleApply();
private:
			BPrivate::WebSettingsPrivate* fData;
};

#endif // _WEB_SETTINGS_H_
