/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "WebDatabaseManager.h"
#include "WebKitDLL.h"

#include "COMEnumVariant.h"
#include "WebSecurityOrigin.h"

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/FileSystem.h>
#include <WebCore/SecurityOriginData.h>

using namespace WebCore;

static COMPtr<WebDatabaseManager> s_sharedWebDatabaseManager;

// WebDatabaseManager --------------------------------------------------------------
WebDatabaseManager* WebDatabaseManager::createInstance()
{
    WebDatabaseManager* manager = new WebDatabaseManager();
    manager->AddRef();
    return manager;    
}

WebDatabaseManager::WebDatabaseManager()
    : m_refCount(0)
{
    gClassCount++;
}

WebDatabaseManager::~WebDatabaseManager()
{
    gClassCount--;
}

// IUnknown ------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WebDatabaseManager::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else if (IsEqualGUID(riid, IID_IWebDatabaseManager))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDatabaseManager::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebDatabaseManager::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

template<> struct COMVariantSetter<SecurityOriginData> : COMIUnknownVariantSetter<WebSecurityOrigin, SecurityOriginData> {};

// IWebDatabaseManager -------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WebDatabaseManager::sharedWebDatabaseManager( 
    /* [retval][out] */ IWebDatabaseManager** result)
{
    if (!s_sharedWebDatabaseManager)
        s_sharedWebDatabaseManager.adoptRef(WebDatabaseManager::createInstance());

    return s_sharedWebDatabaseManager.copyRefTo(result);
}

HRESULT STDMETHODCALLTYPE WebDatabaseManager::origins( 
    /* [retval][out] */ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    Vector<SecurityOriginData> origins;
    DatabaseTracker::tracker().origins(origins);
    COMPtr<COMEnumVariant<Vector<SecurityOriginData> > > enumVariant(AdoptCOM, COMEnumVariant<Vector<SecurityOriginData> >::adopt(origins));

    *result = enumVariant.releaseRef();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::databasesWithOrigin( 
    /* [in] */ IWebSecurityOrigin* origin,
    /* [retval][out] */ IEnumVARIANT** result)
{
    if (!origin || !result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    Vector<String> databaseNames;
    DatabaseTracker::tracker().databaseNamesForOrigin(webSecurityOrigin->securityOriginData(), databaseNames);

    COMPtr<COMEnumVariant<Vector<String> > > enumVariant(AdoptCOM, COMEnumVariant<Vector<String> >::adopt(databaseNames));

    *result = enumVariant.releaseRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDatabaseManager::detailsForDatabaseWithOrigin( 
    /* [in] */ BSTR* database,
    /* [in] */ IWebSecurityOrigin* origin,
    /* [retval][out] */ IPropertyBag** result)
{
    if (!database || !origin || !result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteAllDatabases()
{
    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    return E_NOTIMPL;
}
   
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteDatabasesWithOrigin( 
    /* [in] */ IWebSecurityOrigin* origin)
{
    if (!origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteDatabaseWithOrigin( 
    /* [in] */ BSTR* databaseName,
    /* [in] */ IWebSecurityOrigin* origin)
{
    if (!databaseName || !origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    return E_NOTIMPL;
}

void WebKitSetWebDatabasesPathIfNecessary()
{
    static bool pathSet = false;
    if (pathSet)
        return;

    WebCore::String databasesDirectory = WebCore::pathByAppendingComponent(WebCore::localUserSpecificStorageDirectory(), "Databases");
    WebCore::DatabaseTracker::tracker().setDatabasePath(databasesDirectory);

    pathSet = true;
}

