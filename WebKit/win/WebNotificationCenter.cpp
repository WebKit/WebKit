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
#include "WebNotificationCenter.h"

#include "WebNotification.h"
#pragma warning( push, 0 )
#include <WebCore/StringImpl.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#pragma warning(pop)
#include <tchar.h>

using namespace WebCore;

// ----------------------------------------------------------------------------

struct ObserverHash;

class ObserverKey
{
    friend ObserverHash;
public:
    ObserverKey(BSTR n = 0, IUnknown* o = 0);
    ObserverKey(const ObserverKey& key);
    ~ObserverKey();

    bool operator==(const ObserverKey &) const;
    ObserverKey& operator=(const ObserverKey &);

private:
    BSTR m_notificationName;
    IUnknown* m_anObject;
};

ObserverKey::ObserverKey(BSTR n, IUnknown* o)
: m_notificationName(0)
, m_anObject(o)
{
    if (n == (BSTR)-1)
        m_notificationName = n;
    else if (n)
        m_notificationName = SysAllocString(n);
    if (o)
       o->AddRef();
}

ObserverKey::ObserverKey(const ObserverKey& other)
: m_notificationName(0)
, m_anObject(0)
{
    *this = other;
}

ObserverKey::~ObserverKey()
{
    if (m_notificationName && m_notificationName != (BSTR)-1)
        SysFreeString(m_notificationName);
    if (m_anObject)
        m_anObject->Release();
}

bool ObserverKey::operator==(const ObserverKey &other) const
{
    if (m_anObject && other.m_anObject && m_anObject != other.m_anObject)
        return false; // only fail to match anObject if both are non-null (treat null as a wildcard)

    if (m_notificationName == other.m_notificationName)
        return true;

    if (!m_notificationName || m_notificationName == (BSTR)-1 || !other.m_notificationName || other.m_notificationName == (BSTR)-1)
        return false;

    return !_tcscmp(m_notificationName, other.m_notificationName);
}

ObserverKey& ObserverKey::operator=(const ObserverKey& other)
{
    if (m_notificationName && m_notificationName != (BSTR)-1)
        SysFreeString(m_notificationName);
    m_notificationName = other.m_notificationName;
    if (m_notificationName && m_notificationName != (BSTR)-1)
        m_notificationName = SysAllocString(m_notificationName);
    if (m_anObject != other.m_anObject) {
        if (m_anObject)
            m_anObject->Release();
        m_anObject = other.m_anObject;
        if (m_anObject)
            m_anObject->AddRef();
    }
    return *this;
}

struct ObserverKeyTraits : WTF::GenericHashTraits<ObserverKey> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = true;
    static ObserverKey deletedValue() { return ObserverKey((BSTR)-1, 0); }
};

struct ObserverHash {
    static unsigned hash(const ObserverKey&);
    static bool equal(const ObserverKey& a, const ObserverKey& b) { return a == b; }
};

unsigned ObserverHash::hash(const ObserverKey& key)
{
    unsigned h = 0;

    if (key.m_notificationName) {
        if (key.m_notificationName == (BSTR)-1)
            h = (unsigned)-1;
        else
            h = StringImpl::computeHash(key.m_notificationName, SysStringLen(key.m_notificationName));
    }

    // DO NOT take m_anObject into account for the hash.  We need to match based on m_notificationName only.
    // We ensure a match in operator== if both of the values are non-null.  This matches semantics of
    // NSNotificationCenter.

    return h;
}

typedef HashMap<ObserverKey, Vector<IWebNotificationObserver*>, ObserverHash, ObserverKeyTraits> MappedObservers;

struct WebNotificationCenterPrivate
{
    MappedObservers m_mappedObservers;
};

// WebNotificationCenter ----------------------------------------------------------------

IWebNotificationCenter* WebNotificationCenter::m_defaultCenter = 0;

WebNotificationCenter::WebNotificationCenter()
: m_refCount(0)
, d(new WebNotificationCenterPrivate)
{
    gClassCount++;
}

WebNotificationCenter::~WebNotificationCenter()
{
    delete d;
    gClassCount--;
}

WebNotificationCenter* WebNotificationCenter::createInstance()
{
    WebNotificationCenter* instance = new WebNotificationCenter();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebNotificationCenter::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebNotificationCenter*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationCenter))
        *ppvObject = static_cast<IWebNotificationCenter*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebNotificationCenter::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebNotificationCenter::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

IWebNotificationCenter* WebNotificationCenter::defaultCenterInternal()
{
    if (!m_defaultCenter)
        m_defaultCenter = WebNotificationCenter::createInstance();
    return m_defaultCenter;
}

void WebNotificationCenter::postNotificationInternal(IWebNotification* notification, BSTR notificationName, IUnknown* anObject)
{
    ObserverKey key(notificationName, anObject);
    MappedObservers::iterator it = d->m_mappedObservers.find(key);
    if (it != d->m_mappedObservers.end()) {
        Vector<IWebNotificationObserver*> list = it->second;

        Vector<IWebNotificationObserver*>::iterator end = list.end();
        for (Vector<IWebNotificationObserver*>::iterator it2 = list.begin(); it2 != end; ++it2) {
            (*it2)->onNotify(notification);
        }
    }
}

// IWebNotificationCenter -----------------------------------------------------

HRESULT STDMETHODCALLTYPE WebNotificationCenter::defaultCenter( 
    /* [retval][out] */ IWebNotificationCenter** center)
{
    *center = defaultCenterInternal();
    (*center)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebNotificationCenter::addObserver( 
    /* [in] */ IWebNotificationObserver* observer,
    /* [in] */ BSTR notificationName,
    /* [in] */ IUnknown* anObject)
{
    ObserverKey key(notificationName, anObject);
    MappedObservers::iterator it = d->m_mappedObservers.find(key);
    observer->AddRef();
    if (it != d->m_mappedObservers.end())
        it->second.append(observer);
    else {
        Vector<IWebNotificationObserver*> list;
        list.append(observer);
        d->m_mappedObservers.add(key, list);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebNotificationCenter::postNotification( 
    /* [in] */ IWebNotification* notification)
{
    BSTR name;
    HRESULT hr = notification->name(&name);
    if (SUCCEEDED(hr)) {
        IUnknown* obj;
        hr = notification->getObject(&obj);
        if (SUCCEEDED(hr)) {
            postNotificationInternal(notification, name, obj);
            if (obj)
                obj->Release();
        }
        SysFreeString(name);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE WebNotificationCenter::postNotificationName( 
    /* [in] */ BSTR notificationName,
    /* [in] */ IUnknown* anObject,
    /* [optional][in] */ IPropertyBag* userInfo)
{
    WebNotification* notification = WebNotification::createInstance(notificationName, anObject, userInfo);
    postNotificationInternal(notification, notificationName, anObject);
    notification->Release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebNotificationCenter::removeObserver( 
    /* [in] */ IWebNotificationObserver* anObserver,
    /* [in] */ BSTR notificationName,
    /* [optional][in] */ IUnknown* anObject)
{
    ObserverKey key(notificationName, anObject);
    MappedObservers::iterator it = d->m_mappedObservers.find(key);
    if (it == d->m_mappedObservers.end())
        return E_FAIL;

    Vector<IWebNotificationObserver*>& observerList = it->second;
    Vector<IWebNotificationObserver*>::iterator end = observerList.end();
    int i=0;
    for (Vector<IWebNotificationObserver*>::iterator it2 = observerList.begin(); it2 != end; ++it2, i++) {
        if (*it2 == anObserver) {
            (*it2)->Release();
            observerList.remove(i);
            break;
        }
    }

    if (observerList.isEmpty())
        d->m_mappedObservers.remove(key);

    return S_OK;
}
