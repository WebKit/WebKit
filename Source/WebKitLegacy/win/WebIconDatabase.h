/*
 * Copyright (C) 2006-2009, 2013-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebIconDatabase_H
#define WebIconDatabase_H

#include "WebKit.h"

#include <WebCore/IconDatabaseClient.h>
#include <WebCore/IntSize.h>
#include <WebCore/IntSizeHash.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#include <windows.h>

namespace WebCore
{
    class IconDatabase;
}; //namespace WebCore
using namespace WebCore;
using namespace WTF;

class WebIconDatabase : public IWebIconDatabase, public WebCore::IconDatabaseClient
{
public:
    static WebIconDatabase* createInstance();
    static WebIconDatabase* sharedWebIconDatabase();
private:
    WebIconDatabase();
    ~WebIconDatabase();
    void init();
    void startUpIconDatabase();
    void shutDownIconDatabase();
public:

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebIconDatabase
    virtual HRESULT STDMETHODCALLTYPE sharedIconDatabase(_COM_Outptr_opt_ IWebIconDatabase**);
    virtual HRESULT STDMETHODCALLTYPE iconForURL(_In_ BSTR url, _In_ LPSIZE, BOOL cache, __deref_opt_out HBITMAP* image);
    virtual HRESULT STDMETHODCALLTYPE defaultIconWithSize(_In_ LPSIZE, __deref_opt_out HBITMAP* result);
    virtual HRESULT STDMETHODCALLTYPE retainIconForURL(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE releaseIconForURL(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE removeAllIcons();
    virtual HRESULT STDMETHODCALLTYPE delayDatabaseCleanup();
    virtual HRESULT STDMETHODCALLTYPE allowDatabaseCleanup();
    virtual HRESULT STDMETHODCALLTYPE iconURLForURL(_In_ BSTR url, __deref_opt_out BSTR* iconURL);
    virtual HRESULT STDMETHODCALLTYPE isEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setEnabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE hasIconForURL(_In_ BSTR url, _Out_ BOOL* result);

    // IconDatabaseClient
    virtual void didRemoveAllIcons();
    virtual void didImportIconURLForPageURL(const WTF::String&);
    virtual void didImportIconDataForPageURL(const WTF::String&);
    virtual void didChangeIconForPageURL(const WTF::String&);
    virtual void didFinishURLImport();

    static BSTR iconDatabaseDidAddIconNotification();
    static BSTR iconDatabaseDidRemoveAllIconsNotification();
    static BSTR iconDatabaseNotificationUserInfoURLKey();
protected:
    ULONG m_refCount { 0 };
    static WebIconDatabase* m_sharedWebIconDatabase;

    // Keep a set of HBITMAPs around for the default icon, and another
    // to share amongst present site icons
    HBITMAP getOrCreateSharedBitmap(const IntSize&);
    HBITMAP getOrCreateDefaultIconBitmap(const IntSize&);
    HashMap<IntSize, HBITMAP> m_defaultIconMap;
    HashMap<IntSize, HBITMAP> m_sharedIconMap;

    Lock m_notificationMutex;
    Vector<String> m_notificationQueue;
    void scheduleNotificationDelivery();
    bool m_deliveryRequested { false };

    static void deliverNotifications(void*);
};

#endif
