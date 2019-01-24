/*
 * Copyright (C) 2007-2008, 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "WebDatabaseManager.h"
#include "WebKitDLL.h"

#include "COMEnumVariant.h"
#include "COMPropertyBag.h"
#include "MarshallingHelpers.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include "WebSecurityOrigin.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/DatabaseManager.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>

#if ENABLE(INDEXED_DATABASE)
#include "WebDatabaseProvider.h"
#endif

using namespace WebCore;

static CFStringRef WebDatabaseDirectoryDefaultsKey = CFSTR("WebDatabaseDirectory");

static inline bool isEqual(LPCWSTR s1, LPCWSTR s2)
{
    return !wcscmp(s1, s2);
}

class DatabaseDetailsPropertyBag final : public IPropertyBag {
    WTF_MAKE_NONCOPYABLE(DatabaseDetailsPropertyBag);
public:
    static DatabaseDetailsPropertyBag* createInstance(const DatabaseDetails&);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IPropertyBag
    virtual HRESULT STDMETHODCALLTYPE Read(_In_ LPCOLESTR pszPropName, _Inout_ VARIANT*, _In_ IErrorLog*);
    virtual HRESULT STDMETHODCALLTYPE Write(_In_ LPCOLESTR pszPropName, _In_ VARIANT*);
private:
    DatabaseDetailsPropertyBag(const DatabaseDetails& details) 
        : m_details(details) { }
    ~DatabaseDetailsPropertyBag() { }

    ULONG m_refCount { 0 };
    DatabaseDetails m_details;
};

// DatabaseDetailsPropertyBag ------------------------------------------------------
DatabaseDetailsPropertyBag* DatabaseDetailsPropertyBag::createInstance(const DatabaseDetails& details)
{
    DatabaseDetailsPropertyBag* instance = new DatabaseDetailsPropertyBag(details);
    instance->AddRef();
    return instance;
}

// IUnknown ------------------------------------------------------------------------
ULONG DatabaseDetailsPropertyBag::AddRef()
{
    return ++m_refCount;
}

ULONG DatabaseDetailsPropertyBag::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

HRESULT DatabaseDetailsPropertyBag::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<DatabaseDetailsPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<DatabaseDetailsPropertyBag*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

// IPropertyBag --------------------------------------------------------------------
HRESULT DatabaseDetailsPropertyBag::Read(_In_ LPCOLESTR pszPropName, _Inout_ VARIANT* pVar, _In_ IErrorLog*)
{
    if (!pszPropName || !pVar)
        return E_POINTER;

    ::VariantInit(pVar);

    if (isEqual(pszPropName, WebDatabaseDisplayNameKey)) {
        COMVariantSetter<String>::setVariant(pVar, m_details.displayName());
        return S_OK;
    } else if (isEqual(pszPropName, WebDatabaseExpectedSizeKey)) {
        COMVariantSetter<unsigned long long>::setVariant(pVar, m_details.expectedUsage());
        return S_OK;
    } else if (isEqual(pszPropName, WebDatabaseUsageKey)) {
        COMVariantSetter<unsigned long long>::setVariant(pVar, m_details.currentUsage());
        return S_OK;
    }

    return E_INVALIDARG;
}

HRESULT DatabaseDetailsPropertyBag::Write(_In_ LPCOLESTR pszPropName, _In_ VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;

    return E_NOTIMPL;
}

static COMPtr<WebDatabaseManager> s_sharedWebDatabaseManager;

// WebDatabaseManager --------------------------------------------------------------
WebDatabaseManager* WebDatabaseManager::createInstance()
{
    WebDatabaseManager* manager = new WebDatabaseManager();
    manager->AddRef();
    return manager;    
}

WebDatabaseManager::WebDatabaseManager()
{
    gClassCount++;
    gClassNameCount().add("WebDatabaseManager");
}

WebDatabaseManager::~WebDatabaseManager()
{
    gClassCount--;
    gClassNameCount().remove("WebDatabaseManager");
}

// IUnknown ------------------------------------------------------------------------
HRESULT WebDatabaseManager::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else if (IsEqualGUID(riid, IID_IWebDatabaseManager))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else if (IsEqualGUID(riid, IID_IWebDatabaseManager2))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebDatabaseManager::AddRef()
{
    return ++m_refCount;
}

ULONG WebDatabaseManager::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

template<> struct COMVariantSetter<RefPtr<SecurityOrigin> > : COMIUnknownVariantSetter<WebSecurityOrigin, RefPtr<SecurityOrigin> > {};

// IWebDatabaseManager -------------------------------------------------------------
HRESULT WebDatabaseManager::sharedWebDatabaseManager(_COM_Outptr_opt_ IWebDatabaseManager** result)
{
    if (!result)
        return E_POINTER;

    if (!s_sharedWebDatabaseManager) {
        s_sharedWebDatabaseManager.adoptRef(WebDatabaseManager::createInstance());
        DatabaseManager::singleton().setClient(s_sharedWebDatabaseManager.get());
    }

    return s_sharedWebDatabaseManager.copyRefTo(result);
}

HRESULT WebDatabaseManager::origins(_COM_Outptr_opt_ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    Vector<RefPtr<SecurityOrigin>> origins;
    for (auto& origin : DatabaseTracker::singleton().origins())
        origins.append(origin.securityOrigin());

    COMPtr<COMEnumVariant<Vector<RefPtr<SecurityOrigin>>>> enumVariant(AdoptCOM, COMEnumVariant<Vector<RefPtr<SecurityOrigin>>>::adopt(origins));

    *result = enumVariant.leakRef();
    return S_OK;
}
    
HRESULT WebDatabaseManager::databasesWithOrigin(_In_opt_ IWebSecurityOrigin* origin, _COM_Outptr_opt_ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!origin)
        return E_POINTER;

    *result = nullptr;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    auto databaseNames = DatabaseTracker::singleton().databaseNames(webSecurityOrigin->securityOrigin()->data());

    COMPtr<COMEnumVariant<Vector<String>>> enumVariant(AdoptCOM, COMEnumVariant<Vector<String>>::adopt(databaseNames));

    *result = enumVariant.leakRef();
    return S_OK;
}

HRESULT WebDatabaseManager::detailsForDatabase(_In_ BSTR databaseName, _In_opt_ IWebSecurityOrigin* origin, _COM_Outptr_opt_ IPropertyBag** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    auto details = DatabaseManager::singleton().detailsForNameAndOrigin(String(databaseName, SysStringLen(databaseName)),
        *webSecurityOrigin->securityOrigin());

    if (details.name().isNull())
        return E_INVALIDARG;

    *result = DatabaseDetailsPropertyBag::createInstance(details);
    return S_OK;
}

HRESULT WebDatabaseManager::deleteAllDatabases()
{
    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    DatabaseTracker::singleton().deleteAllDatabasesImmediately();

    return S_OK;
}

HRESULT WebDatabaseManager::deleteOrigin(_In_opt_ IWebSecurityOrigin* origin)
{
    if (!origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    DatabaseTracker::singleton().deleteOrigin(webSecurityOrigin->securityOrigin()->data());

    return S_OK;
}
    
HRESULT WebDatabaseManager::deleteDatabase(_In_ BSTR databaseName, _In_opt_ IWebSecurityOrigin* origin)
{
    if (!origin)
        return E_POINTER;

    if (!databaseName)
        return E_INVALIDARG;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    DatabaseTracker::singleton().deleteDatabase(webSecurityOrigin->securityOrigin()->data(), String(databaseName, SysStringLen(databaseName)));

    return S_OK;
}

HRESULT WebDatabaseManager::deleteAllIndexedDatabases()
{
#if ENABLE(INDEXED_DATABASE)
    WebDatabaseProvider::singleton().deleteAllDatabases();
#endif
    return S_OK;
}

HRESULT WebDatabaseManager::setIDBPerOriginQuota(unsigned long long quota)
{
#if ENABLE(INDEXED_DATABASE)
    WebDatabaseProvider::singleton().setIDBPerOriginQuota(quota);
#endif
    return S_OK;
}

class DidModifyOriginData {
    WTF_MAKE_NONCOPYABLE(DidModifyOriginData);
public:
    static void dispatchToMainThread(WebDatabaseManager* databaseManager, const SecurityOriginData& origin)
    {
        DidModifyOriginData* context = new DidModifyOriginData(databaseManager, origin.isolatedCopy());
        callOnMainThread([context] {
            dispatchDidModifyOriginOnMainThread(context);
        });
    }

private:
    DidModifyOriginData(WebDatabaseManager* databaseManager, const SecurityOriginData& origin)
        : databaseManager(databaseManager)
        , origin(origin)
    {
    }

    static void dispatchDidModifyOriginOnMainThread(void* context)
    {
        ASSERT(isMainThread());
        DidModifyOriginData* info = static_cast<DidModifyOriginData*>(context);
        info->databaseManager->dispatchDidModifyOrigin(info->origin);
        delete info;
    }

    WebDatabaseManager* databaseManager;
    SecurityOriginData origin;
};

void WebDatabaseManager::dispatchDidModifyOrigin(const SecurityOriginData& origin)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    static BSTR databaseDidModifyOriginName = SysAllocString(WebDatabaseDidModifyOriginNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();

    COMPtr<WebSecurityOrigin> securityOrigin(AdoptCOM, WebSecurityOrigin::createInstance(origin.securityOrigin().ptr()));
    notifyCenter->postNotificationName(databaseDidModifyOriginName, securityOrigin.get(), 0);
}

HRESULT WebDatabaseManager::setQuota(_In_ BSTR origin, unsigned long long quota)
{
    if (!origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    DatabaseTracker::singleton().setQuota(SecurityOrigin::createFromString(origin)->data(), quota);

    return S_OK;
}

void WebDatabaseManager::dispatchDidModifyDatabase(const SecurityOriginData& origin, const String& databaseName)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    static BSTR databaseDidModifyOriginName = SysAllocString(WebDatabaseDidModifyDatabaseNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();

    COMPtr<WebSecurityOrigin> securityOrigin(AdoptCOM, WebSecurityOrigin::createInstance(origin.securityOrigin().ptr()));

    HashMap<String, String> userInfo;
    userInfo.set(WebDatabaseNameKey, databaseName);
    COMPtr<IPropertyBag> userInfoBag(AdoptCOM, COMPropertyBag<String>::adopt(userInfo));

    notifyCenter->postNotificationName(databaseDidModifyOriginName, securityOrigin.get(), userInfoBag.get());
}

static WTF::String databasesDirectory()
{
#if USE(CF)
    RetainPtr<CFPropertyListRef> directoryPref = adoptCF(CFPreferencesCopyAppValue(WebDatabaseDirectoryDefaultsKey, WebPreferences::applicationId()));
    if (directoryPref && (CFStringGetTypeID() == CFGetTypeID(directoryPref.get())))
        return static_cast<CFStringRef>(directoryPref.get());
#endif

    return FileSystem::pathByAppendingComponent(FileSystem::localUserSpecificStorageDirectory(), "Databases");
}

void WebKitInitializeWebDatabasesIfNecessary()
{
    static bool initialized = false;
    if (initialized)
        return;

    WebCore::DatabaseManager::singleton().initialize(databasesDirectory());

    initialized = true;
}
