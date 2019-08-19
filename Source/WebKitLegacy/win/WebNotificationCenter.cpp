/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebNotificationCenter.h"

#include "WebNotification.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <utility>
#include <wchar.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

typedef std::pair<COMPtr<IUnknown>, COMPtr<IWebNotificationObserver> > ObjectObserverPair;
typedef Vector<ObjectObserverPair> ObjectObserverList;
typedef ObjectObserverList::iterator ObserverListIterator;
typedef HashMap<String, ObjectObserverList> MappedObservers;

struct WebNotificationCenterPrivate {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    MappedObservers m_mappedObservers;
};

// WebNotificationCenter ----------------------------------------------------------------

IWebNotificationCenter* WebNotificationCenter::m_defaultCenter = 0;

WebNotificationCenter::WebNotificationCenter()
    : d(makeUnique<WebNotificationCenterPrivate>())
{
    gClassCount++;
    gClassNameCount().add("WebNotificationCenter");
}

WebNotificationCenter::~WebNotificationCenter()
{
    gClassCount--;
    gClassNameCount().remove("WebNotificationCenter");
}

WebNotificationCenter* WebNotificationCenter::createInstance()
{
    WebNotificationCenter* instance = new WebNotificationCenter();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebNotificationCenter::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebNotificationCenter*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationCenter))
        *ppvObject = static_cast<IWebNotificationCenter*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebNotificationCenter::AddRef()
{
    return ++m_refCount;
}

ULONG WebNotificationCenter::Release()
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
    String name(notificationName, SysStringLen(notificationName));
    MappedObservers::iterator it = d->m_mappedObservers.find(name);
    if (it == d->m_mappedObservers.end())
        return;

    // Intentionally make a copy of the list to avoid the possibility of errors
    // from a mutation of the list in the onNotify callback.
    ObjectObserverList list = it->value;

    ObserverListIterator end = list.end();
    for (ObserverListIterator it2 = list.begin(); it2 != end; ++it2) {
        IUnknown* observedObject = it2->first.get();
        IWebNotificationObserver* observer = it2->second.get();
        if (!observedObject || !anObject || observedObject == anObject)
            observer->onNotify(notification);
    }
}

// IWebNotificationCenter -----------------------------------------------------

HRESULT WebNotificationCenter::defaultCenter(_COM_Outptr_opt_ IWebNotificationCenter** center)
{
    if (!center)
        return E_POINTER;
    *center = defaultCenterInternal();
    (*center)->AddRef();
    return S_OK;
}

HRESULT WebNotificationCenter::addObserver(_In_opt_ IWebNotificationObserver* observer, _In_ BSTR notificationName, _In_opt_ IUnknown* anObject)
{
    String name(notificationName, SysStringLen(notificationName));
    MappedObservers::iterator it = d->m_mappedObservers.find(name);
    if (it != d->m_mappedObservers.end())
        it->value.append(ObjectObserverPair(anObject, observer));
    else {
        ObjectObserverList list;
        list.append(ObjectObserverPair(anObject, observer));
        d->m_mappedObservers.add(name, list);
    }

    return S_OK;
}

HRESULT WebNotificationCenter::postNotification(_In_opt_ IWebNotification* notification)
{
    BString name;
    HRESULT hr = notification->name(&name);
    if (FAILED(hr))
        return hr;

    COMPtr<IUnknown> obj;
    hr = notification->getObject(&obj);
    if (FAILED(hr))
        return hr;

    postNotificationInternal(notification, name, obj.get());

    return hr;
}

HRESULT WebNotificationCenter::postNotificationName(_In_ BSTR notificationName, _In_opt_ IUnknown* anObject, _In_opt_ IPropertyBag* userInfo)
{
    COMPtr<WebNotification> notification(AdoptCOM, WebNotification::createInstance(notificationName, anObject, userInfo));
    postNotificationInternal(notification.get(), notificationName, anObject);
    return S_OK;
}

HRESULT WebNotificationCenter::removeObserver(_In_opt_ IWebNotificationObserver* anObserver, _In_ BSTR notificationName, _In_opt_ IUnknown* anObject)
{
    String name(notificationName, SysStringLen(notificationName));
    MappedObservers::iterator it = d->m_mappedObservers.find(name);
    if (it == d->m_mappedObservers.end())
        return E_FAIL;

    ObjectObserverList& observerList = it->value;
    observerList.removeFirstMatching([anObject, anObserver] (const ObjectObserverPair& pair) {
        IUnknown* observedObject = pair.first.get();
        IWebNotificationObserver* observer = pair.second.get();
        return observer == anObserver && (!anObject || anObject == observedObject);
    });

    if (observerList.isEmpty())
        d->m_mappedObservers.remove(name);

    return S_OK;
}
