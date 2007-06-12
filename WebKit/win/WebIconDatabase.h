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

#ifndef WebIconDatabase_H
#define WebIconDatabase_H

#include "IWebIconDatabase.h"

#pragma warning(push, 0)
#include <WebCore/IntSize.h>
#include <WebCore/IntSizeHash.h>
#pragma warning(pop)

#include <WTF/HashMap.h>

#include <windows.h>

namespace WebCore
{
    class IconDatabase;
}; //namespace WebCore
using namespace WebCore;
using namespace WTF;

class WebIconDatabase : public IWebIconDatabase
{
public:
    static WebIconDatabase* createInstance();
    static WebIconDatabase* sharedWebIconDatabase();
private:
    WebIconDatabase();
    ~WebIconDatabase();
    void init();
public:

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebIconDatabase
    virtual HRESULT STDMETHODCALLTYPE sharedIconDatabase( 
        /* [retval][out] */ IWebIconDatabase **result);
    
    virtual HRESULT STDMETHODCALLTYPE iconForURL( 
        /* [in] */ BSTR url,
        /* [optional][in] */ LPSIZE size,
        /* [optional][in] */ BOOL cache,
        /* [retval][out] */ OLE_HANDLE *image);
    
    virtual HRESULT STDMETHODCALLTYPE defaultIconWithSize( 
        /* [in] */ LPSIZE size,
        /* [retval][out] */ OLE_HANDLE *result);
    
    virtual HRESULT STDMETHODCALLTYPE retainIconForURL( 
        /* [in] */ BSTR url);
    
    virtual HRESULT STDMETHODCALLTYPE releaseIconForURL( 
        /* [in] */ BSTR url);

    virtual HRESULT STDMETHODCALLTYPE removeAllIcons( void);
    
    virtual HRESULT STDMETHODCALLTYPE delayDatabaseCleanup( void);
    
    virtual HRESULT STDMETHODCALLTYPE allowDatabaseCleanup( void);

protected:
    ULONG m_refCount;
    static WebIconDatabase* m_sharedWebIconDatabase;

    // Keep a set of HBITMAPs around for the default icon, and another
    // to share amongst present site icons
    HBITMAP getOrCreateSharedBitmap(LPSIZE size);
    HBITMAP getOrCreateDefaultIconBitmap(LPSIZE size);
    HashMap<IntSize, HBITMAP> m_defaultIconMap;
    HashMap<IntSize, HBITMAP> m_sharedIconMap;
};

#endif
