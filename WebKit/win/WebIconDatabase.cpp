/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "WebKitDLL.h"
#include "WebIconDatabase.h"

#include "COMPtr.h"
#include "WebPreferences.h"
#pragma warning(push, 0)
#include <WebCore/IconDatabase.h>
#include <WebCore/Image.h>
#include <WebCore/PlatformString.h>
#pragma warning(pop)
#include "shlobj.h"

using namespace WebCore;
using namespace WTF;

// WebIconDatabase ----------------------------------------------------------------

WebIconDatabase* WebIconDatabase::m_sharedWebIconDatabase = 0;

WebIconDatabase::WebIconDatabase()
: m_refCount(0)
{
    gClassCount++;
}

WebIconDatabase::~WebIconDatabase()
{
    gClassCount--;
}

// FIXME - <rdar://problem/4721579>
// This is code ripped directly from FileUtilities.cpp - it may be extremely useful
// to have it in a centralized location in WebKit.  But also, getting the icon database
// path should use the WebPreferences system before it falls back to some reasonable default
HRESULT userIconDatabasePath(String& path)
{
    // get the path to the user's non-roaming application data folder (Example: C:\Documents and Settings\{username}\Local Settings\Application Data\)
    TCHAR appDataPath[MAX_PATH];
    HRESULT hr = SHGetFolderPath(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataPath);
    if (FAILED(hr))
        return hr;

    // make the Apple Computer and WebKit subfolder
    path = String(appDataPath) + "\\Apple Computer\\";

    WebCore::String appName = "WebKit";
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFStringRef bundleExecutable = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleExecutableKey);
        if (bundleExecutable)
            appName = bundleExecutable;
    }
    path += appName;

    if (!CreateDirectory(path.charactersWithNullTermination(), 0)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return (HRESULT_FROM_WIN32(err));
    }

    return S_OK;
}

void WebIconDatabase::init()
{
    WebPreferences* standardPrefs = WebPreferences::sharedStandardPreferences();
    BOOL enabled = FALSE;
    if (FAILED(standardPrefs->iconDatabaseEnabled(&enabled))) {
        enabled = FALSE;
        LOG_ERROR("Unable to get icon database enabled preference");
    }
    iconDatabase()->setEnabled(!!enabled);

    BSTR prefDatabasePath = 0;
    if (FAILED(standardPrefs->iconDatabaseLocation(&prefDatabasePath)))
        LOG_ERROR("Unable to get icon database location preference");

    String databasePath(prefDatabasePath, SysStringLen(prefDatabasePath));
    SysFreeString(prefDatabasePath);
    if (databasePath.isEmpty())
        if (FAILED(userIconDatabasePath(databasePath)))
            LOG_ERROR("Failed to construct default icon database path");

    if (!iconDatabase()->open(databasePath))
            LOG_ERROR("Failed to open icon database path");
}

WebIconDatabase* WebIconDatabase::createInstance()
{
    WebIconDatabase* instance = new WebIconDatabase();
    instance->AddRef();
    return instance;
}

WebIconDatabase* WebIconDatabase::sharedWebIconDatabase()
{
    if (m_sharedWebIconDatabase) {
        m_sharedWebIconDatabase->AddRef();
        return m_sharedWebIconDatabase;
    }
    m_sharedWebIconDatabase = createInstance();
    m_sharedWebIconDatabase->init();
    return m_sharedWebIconDatabase;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebIconDatabase::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebIconDatabase*>(this);
    else if (IsEqualGUID(riid, IID_IWebIconDatabase))
        *ppvObject = static_cast<IWebIconDatabase*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebIconDatabase::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebIconDatabase::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebIconDatabase --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebIconDatabase::sharedIconDatabase(
        /* [retval][out] */ IWebIconDatabase** result)
{
    *result = sharedWebIconDatabase();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::iconForURL(
        /* [in] */ BSTR url,
        /* [optional][in] */ LPSIZE size,
        /* [optional][in] */ BOOL /*cache*/,
        /* [retval][out] */ OLE_HANDLE* bitmap)
{
    IntSize intSize(*size);

    Image* icon = iconDatabase()->iconForPageURL(String(url, SysStringLen(url)), intSize);

    // Make sure we check for the case of an "empty image"
    if (icon && icon->width()) {
        *bitmap = (OLE_HANDLE)(ULONG64)getOrCreateSharedBitmap(size);
        if (!icon->getHBITMAP((HBITMAP)(ULONG64)*bitmap)) {
            LOG_ERROR("Failed to draw Image to HBITMAP");
            *bitmap = 0;
            return E_FAIL;
        }
        return S_OK;
    }

    return defaultIconWithSize(size, bitmap);
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::defaultIconWithSize(
        /* [in] */ LPSIZE size,
        /* [retval][out] */ OLE_HANDLE* result)
{
    *result = (OLE_HANDLE)(ULONG64)getOrCreateDefaultIconBitmap(size);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::retainIconForURL(
        /* [in] */ BSTR url)
{
    iconDatabase()->retainIconForPageURL(String(url, SysStringLen(url)));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::releaseIconForURL(
        /* [in] */ BSTR url)
{
    iconDatabase()->releaseIconForPageURL(String(url, SysStringLen(url)));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::removeAllIcons(void)
{
    iconDatabase()->removeAllIcons();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::delayDatabaseCleanup(void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebIconDatabase::allowDatabaseCleanup(void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HBITMAP createDIB(LPSIZE size)
{
    HBITMAP result;

    BITMAPINFO bmInfo = {0};
    bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmInfo.bmiHeader.biWidth = size->cx;
    bmInfo.bmiHeader.biHeight = size->cy;
    bmInfo.bmiHeader.biPlanes = 1;
    bmInfo.bmiHeader.biBitCount = 32;
    bmInfo.bmiHeader.biCompression = BI_RGB;

    HDC dc = GetDC(0);
    result = CreateDIBSection(dc, &bmInfo, DIB_RGB_COLORS, 0, 0, 0);
    ReleaseDC(0, dc);

    return result;
}

HBITMAP WebIconDatabase::getOrCreateSharedBitmap(LPSIZE size)
{
    HBITMAP result = m_sharedIconMap.get(*size);
    if (result)
        return result;
    result = createDIB(size);
    m_sharedIconMap.set(*size, result);
    return result;
}

HBITMAP WebIconDatabase::getOrCreateDefaultIconBitmap(LPSIZE size)
{
    HBITMAP result = m_defaultIconMap.get(*size);
    if (result)
        return result;

    result = createDIB(size);
    static Image* defaultIconImage = 0;
    if (!defaultIconImage) {
        defaultIconImage = Image::loadPlatformResource("urlIcon");
    }
    m_defaultIconMap.set(*size, result);
    if (!defaultIconImage->getHBITMAP(result)) {
        LOG_ERROR("Failed to draw Image to HBITMAP");
        return 0;
    }
    return result;
}
