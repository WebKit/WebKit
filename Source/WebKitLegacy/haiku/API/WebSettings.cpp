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

#include "config.h"
#include "WebSettings.h"

#include "ApplicationCacheStorage.h"
#include "BitmapImage.h"
#include "DatabaseTracker.h"
#include "FontPlatformData.h"
#include "FrameNetworkingContextHaiku.h"
#include "IconDatabase.h"
#include "Image.h"
#include "IntSize.h"
#include "Settings.h"
#include "WebSettingsPrivate.h"
#include <Application.h>
#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Font.h>
#include <Path.h>
#include <stdio.h>
#include <UrlContext.h>

enum {
	HANDLE_SET_PERSISTENT_STORAGE_PATH = 'hspp',
	HANDLE_SET_ICON_DATABASE_PATH = 'hsip',
	HANDLE_CLEAR_ICON_DATABASE = 'hcli',
	HANDLE_SEND_ICON_FOR_URL = 'sifu',
	HANDLE_SET_OFFLINE_STORAGE_PATH = 'hsop',
	HANDLE_SET_OFFLINE_STORAGE_DEFAULT_QUOTA = 'hsoq',
	HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_PATH = 'hsap',
	HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_QUOTA = 'hsaq',
	HANDLE_SET_LOCAL_STORAGE_PATH = 'hslp',
	HANDLE_SET_FONT = 'hsfn',
	HANDLE_SET_FONT_SIZE = 'hsfs',
	HANDLE_SET_PROXY_INFO = 'hspi',
	HANDLE_SET_JAVASCRIPT_ENABLED = 'jsen',
	HANDLE_APPLY = 'hapl'
};

enum {
	SERIF_FONT = 0,
	SANS_SERIF_FONT,
	FIXED_FONT,
	STANDARD_FONT,
};

enum {
	STANDARD_FONT_SIZE = 0,
	FIXED_FONT_SIZE,
};

BWebSettings::BWebSettings()
    : fData(new BPrivate::WebSettingsPrivate())
{
	// This constructor is used only for the default (global) settings.
	if (be_app->Lock()) {
		be_app->AddHandler(this);
		be_app->Unlock();
	}
}

BWebSettings::BWebSettings(WebCore::Settings* settings)
    : fData(new BPrivate::WebSettingsPrivate(settings))
{
	if (be_app->Lock()) {
		be_app->AddHandler(this);
		be_app->Unlock();
	}
}

BWebSettings::~BWebSettings()
{
	if (be_app->Lock()) {
		be_app->RemoveHandler(this);
		be_app->Unlock();
	}
	delete fData;
}

BWebSettings* BWebSettings::Default()
{
	static BWebSettings defaultInstance;
	return &defaultInstance;
}

void BWebSettings::SetIconDatabasePath(const BString& path)
{
	_PostSetPath(Default(), HANDLE_SET_ICON_DATABASE_PATH, path);
}

void BWebSettings::ClearIconDatabase()
{
	Default()->Looper()->PostMessage(HANDLE_CLEAR_ICON_DATABASE, Default());
}

void BWebSettings::SendIconForURL(const BString& url, const BMessage& reply,
		const BMessenger& target)
{
	BMessage message(HANDLE_SEND_ICON_FOR_URL);
	message.AddString("url", url.String());
	message.AddMessage("reply", &reply);
	message.AddMessenger("target", target);
	Default()->Looper()->PostMessage(&message, Default());
}


void BWebSettings::SetPersistentStoragePath(const BString& path)
{
	_PostSetPath(Default(), HANDLE_SET_PERSISTENT_STORAGE_PATH, path);
}

void BWebSettings::SetOfflineStoragePath(const BString& path)
{
	_PostSetPath(Default(), HANDLE_SET_OFFLINE_STORAGE_PATH, path);
}

void BWebSettings::SetOfflineStorageDefaultQuota(int64 maximumSize)
{
	_PostSetQuota(Default(), HANDLE_SET_OFFLINE_STORAGE_DEFAULT_QUOTA, maximumSize);
}

void BWebSettings::SetOfflineWebApplicationCachePath(const BString& path)
{
	_PostSetPath(Default(), HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_PATH, path);
}

void BWebSettings::SetOfflineWebApplicationCacheQuota(int64 maximumSize)
{
	_PostSetQuota(Default(), HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_QUOTA, maximumSize);
}

void BWebSettings::SetLocalStoragePath(const BString& path)
{
	_PostSetPath(this, HANDLE_SET_LOCAL_STORAGE_PATH, path);
}

void BWebSettings::SetSerifFont(const BFont& font)
{
    _PostFont(SERIF_FONT, font);
}

void BWebSettings::SetSansSerifFont(const BFont& font)
{
    _PostFont(SANS_SERIF_FONT, font);
}

void BWebSettings::SetFixedFont(const BFont& font)
{
    _PostFont(FIXED_FONT, font);
}

void BWebSettings::SetStandardFont(const BFont& font)
{
    _PostFont(STANDARD_FONT, font);
}

void BWebSettings::SetDefaultStandardFontSize(float size)
{
    _PostFontSize(STANDARD_FONT_SIZE, size);
}

void BWebSettings::SetDefaultFixedFontSize(float size)
{
    _PostFontSize(FIXED_FONT_SIZE, size);
}

void BWebSettings::SetJavascriptEnabled(bool enabled)
{
	BMessage message(HANDLE_SET_JAVASCRIPT_ENABLED);
	message.AddBool("enable", enabled);
	_PostMessage(Default(), &message);
}

void BWebSettings::SetProxyInfo(const BString& host, uint32 port,
	BProxyType type, const BString& username, const BString& password)
{
	BMessage message(HANDLE_SET_PROXY_INFO);
	message.AddString("host", host);
	message.AddUInt32("port", port);
	message.AddUInt32("type", type);
	message.AddString("username", username);
	message.AddString("password", password);
	_PostMessage(Default(), &message);
}

void BWebSettings::Apply()
{
    Looper()->PostMessage(HANDLE_APPLY, this);
}

// #pragma mark - private

void BWebSettings::_PostSetPath(BHandler* handler, uint32 what, const BString& path)
{
	BMessage message(what);
	message.AddString("path", path.String());
	_PostMessage(handler, &message);
}

void BWebSettings::_PostSetQuota(BHandler* handler, uint32 what, int64 maximumSize)
{
	BMessage message(what);
	message.AddInt64("quota", maximumSize);
	_PostMessage(handler, &message);
}

void BWebSettings::_PostFont(uint32 which, const BFont& font)
{
	BMessage message(HANDLE_SET_FONT);
	message.AddInt32("which", which);

	font_family family;
	font_style style;
	font.GetFamilyAndStyle(&family, &style);
	BString string(family);
// NOTE: WebCore is not interested in the style here, since it manages styles by itself.
// Only the family is of interest to reference a font.
//	string << ' ' << style;
	message.AddString("font", string.String());

	_PostMessage(this, &message);
}

void BWebSettings::_PostFontSize(uint32 which, float size)
{
	BMessage message(HANDLE_SET_FONT_SIZE);
	message.AddInt32("which", which);
	message.AddFloat("size", size);
	_PostMessage(this, &message);
}

void BWebSettings::_PostMessage(BHandler* handler, BMessage* message)
{
	if (find_thread(0) == handler->Looper()->Thread())
	    handler->MessageReceived(message);
	else
	    handler->Looper()->PostMessage(message, handler);
}

// #pragma mark -

void BWebSettings::MessageReceived(BMessage* message)
{
	switch (message->what) {
	case HANDLE_SET_PERSISTENT_STORAGE_PATH: {
		BString path;
		if (message->FindString("path", &path) == B_OK)
		    _HandleSetPersistentStoragePath(path);
		break;
	}
	case HANDLE_SET_ICON_DATABASE_PATH: {
		BString path;
		if (message->FindString("path", &path) == B_OK)
			_HandleSetIconDatabasePath(path);
		break;
	}
    case HANDLE_CLEAR_ICON_DATABASE:
		_HandleClearIconDatabase();
		break;
    case HANDLE_SEND_ICON_FOR_URL: {
		_HandleSendIconForURL(message);
		break;
    }
	case HANDLE_SET_OFFLINE_STORAGE_PATH: {
		BString path;
		if (message->FindString("path", &path) == B_OK)
		    _HandleSetOfflineStoragePath(path);
		break;
	}
	case HANDLE_SET_OFFLINE_STORAGE_DEFAULT_QUOTA: {
		int64 maximumSize;
		if (message->FindInt64("quota", &maximumSize) == B_OK)
		    _HandleSetOfflineStorageDefaultQuota(maximumSize);
		break;
	}
	case HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_PATH: {
		BString path;
		if (message->FindString("path", &path) == B_OK)
		    _HandleSetWebApplicationCachePath(path);
		break;
	}
	case HANDLE_SET_OFFLINE_WEB_APPLICATION_CACHE_QUOTA: {
		int64 maximumSize;
		if (message->FindInt64("quota", &maximumSize) == B_OK)
		    _HandleSetWebApplicationCacheQuota(maximumSize);
		break;
	}
	case HANDLE_SET_LOCAL_STORAGE_PATH: {
		BString path;
		if (message->FindString("path", &path) == B_OK)
		    _HandleSetLocalStoragePath(path);
		break;
	}
	case HANDLE_SET_FONT:
		_HandleSetFont(message);
		break;
	case HANDLE_SET_FONT_SIZE:
		_HandleSetFontSize(message);
		break;

	case HANDLE_SET_JAVASCRIPT_ENABLED:
		_HandleSetJavascriptEnabled(message->FindBool("enable"));
		break;

	case HANDLE_SET_PROXY_INFO:
		_HandleSetProxyInfo(message);
		break;

	case HANDLE_APPLY:
		_HandleApply();
		break;
	default:
		BHandler::MessageReceived(message);
	}
}

void BWebSettings::_HandleSetPersistentStoragePath(const BString& path)
{
    BPath storagePath;

    if (!path.Length())
        find_directory(B_USER_DATA_DIRECTORY, &storagePath);
    else
        storagePath.SetTo(path.String());

	create_directory(storagePath.Path(), 0777);

	_HandleSetIconDatabasePath(storagePath.Path());
    _HandleSetWebApplicationCachePath(storagePath.Path());
    BPath dataBasePath(storagePath);
    dataBasePath.Append("Databases");
    _HandleSetOfflineStoragePath(dataBasePath.Path());
    BPath localStoragePath(storagePath);
    dataBasePath.Append("LocalStorage");
    Default()->_HandleSetLocalStoragePath(localStoragePath.Path());

    Default()->fData->localStorageEnabled = true;
    Default()->fData->databasesEnabled = true;
    Default()->fData->offlineWebApplicationCacheEnabled = true;
    Default()->fData->apply();
}

void BWebSettings::_HandleSetOfflineStoragePath(const BString& path)
{
}

void BWebSettings::_HandleSetOfflineStorageDefaultQuota(int64 maximumSize)
{
    fData->offlineStorageDefaultQuota = maximumSize;
}

void BWebSettings::_HandleSetWebApplicationCachePath(const BString& path)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    WebCore::cacheStorage().setCacheDirectory(path);
#endif
}

void BWebSettings::_HandleSetIconDatabasePath(const BString& path)
{
	WebKit::IconDatabase::delayDatabaseCleanup();

	if (path.Length()) {
		WebKit::iconDatabase().setEnabled(true);
		BEntry entry(path.String());
		if (entry.IsDirectory())
			WebKit::iconDatabase().open(path, WebKit::IconDatabase::defaultDatabaseFilename());
	} else {
		WebKit::iconDatabase().setEnabled(false);
		WebKit::iconDatabase().close();
	}
}

void BWebSettings::_HandleClearIconDatabase()
{
	if (WebKit::iconDatabase().isEnabled() && WebKit::iconDatabase().isOpen())
        WebKit::iconDatabase().removeAllIcons();
}

void BWebSettings::_HandleSendIconForURL(BMessage* message)
{
	BString url;
	BMessage reply;
	BMessenger target;
	if (message->FindString("url", &url) != B_OK
			|| message->FindMessage("reply", &reply) != B_OK
			|| message->FindMessenger("target", &target) != B_OK) {
		return;
	}

	reply.RemoveName("url");
	reply.RemoveName("icon");
	reply.AddString("url", url);

	std::pair<WebCore::PlatformImagePtr, WebKit::IconDatabase::IsKnownIcon> icon
		= WebKit::iconDatabase().synchronousIconForPageURL(url, WebCore::IntSize(16, 16));
	BMessage iconArchive;
	if (icon.second == WebKit::IconDatabase::IsKnownIcon::Yes
			&& icon.first != NULL && icon.first->Archive(&iconArchive) == B_OK)
		reply.AddMessage("icon", &iconArchive);

	target.SendMessage(&reply);
}


void BWebSettings::_HandleSetWebApplicationCacheQuota(int64 maximumSize)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    WebCore::cacheStorage().setMaximumSize(maximumSize);
#endif
}

void BWebSettings::_HandleSetLocalStoragePath(const BString& path)
{
    if (!fData->settings)
        return;

    fData->localStoragePath = path;
    fData->apply();
}

void BWebSettings::_HandleSetFont(BMessage* message)
{
	int32 which;
	if (message->FindInt32("which", &which) != B_OK)
		return;
	BString font;
	if (message->FindString("font", &font) != B_OK)
		return;
	switch (which) {
	case SERIF_FONT:
        fData->serifFontFamily = font;
        fData->serifFontFamilySet = true;
        WebCore::FontPlatformData::SetFallBackSerifFont(font);
        break;
	case SANS_SERIF_FONT:
        fData->sansSerifFontFamily = font;
        fData->sansSerifFontFamilySet = true;
        WebCore::FontPlatformData::SetFallBackSansSerifFont(font);
        break;
	case FIXED_FONT:
        fData->fixedFontFamily = font;
        fData->fixedFontFamilySet = true;
        WebCore::FontPlatformData::SetFallBackFixedFont(font);
        break;
	case STANDARD_FONT:
        fData->standardFontFamily = font;
        fData->standardFontFamilySet = true;
        WebCore::FontPlatformData::SetFallBackStandardFont(font);
        break;
	}
}

void BWebSettings::_HandleSetFontSize(BMessage* message)
{
	int32 which;
	if (message->FindInt32("which", &which) != B_OK)
		return;
	float size;
	if (message->FindFloat("size", &size) != B_OK)
		return;
	switch (which) {
	case STANDARD_FONT_SIZE:
        fData->defaultFontSize = size;
        fData->defaultFontSizeSet = true;
        break;
	case FIXED_FONT_SIZE:
        fData->defaultFixedFontSize = size;
        fData->defaultFixedFontSizeSet = true;
        break;
	}
}


void BWebSettings::_HandleSetJavascriptEnabled(bool enable)
{
	fData->javascriptEnabled = enable;
}


void BWebSettings::_HandleSetProxyInfo(BMessage* message)
{
	BString host;
	uint32 port;
	BProxyType type;
	BString username;
	BString password;
	if (message->FindString("host", &host) != B_OK
		|| message->FindUInt32("port", &port) != B_OK
		|| message->FindUInt32("type", reinterpret_cast<uint32*>(&type)) != B_OK
		|| message->FindString("username", &username) != B_OK
		|| message->FindString("password", &password) != B_OK)
		return;

    // TODO handle SOCKS proxy and authentication
    // TODO there could be a cleaner way of accessing the default context from here.
    RefPtr<WebCore::FrameNetworkingContextHaiku> context
        = WebCore::FrameNetworkingContextHaiku::create(nullptr, nullptr);
    context->context()->SetProxy(host, port);
}

void BWebSettings::_HandleApply()
{
	fData->apply();
}
