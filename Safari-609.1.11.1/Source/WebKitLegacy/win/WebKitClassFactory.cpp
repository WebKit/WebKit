/*
 * Copyright (C) 2006-2007, 2014-2015 Apple Inc.  All rights reserved.
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
#include "WebKitClassFactory.h"

#include "CFDictionaryPropertyBag.h"
#include "ForEachCoClass.h"
#include "WebApplicationCache.h"
#include "WebArchive.h"
#include "WebCache.h"
#include "WebCoreStatistics.h"
#include "WebDatabaseManager.h"
#include "WebDownload.h"
#include "WebError.h"
#include "WebFrame.h"
#include "WebGeolocationPosition.h"
#include "WebHistory.h"
#include "WebHistoryItem.h"
#include "WebJavaScriptCollector.h"
#include "WebKit.h"
#include "WebKitMessageLoop.h"
#include "WebKitStatistics.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include "WebScriptWorld.h"
#include "WebSerializedJSValue.h"
#include "WebTextRenderer.h"
#include "WebURLCredential.h"
#include "WebURLProtectionSpace.h"
#include "WebURLResponse.h"
#include "WebUserContentURLPattern.h"
#include "WebView.h"
#include "WebWorkersPrivate.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <wtf/MainThread.h>
#include <wtf/SoftLinking.h>

// WebKitClassFactory ---------------------------------------------------------
WebKitClassFactory::WebKitClassFactory(CLSID targetClass)
    : m_targetClass(targetClass)
{
    JSC::initializeThreading();
    WTF::initializeMainThread();

    gClassCount++;
    gClassNameCount().add("WebKitClassFactory");
}

WebKitClassFactory::~WebKitClassFactory()
{
    gClassCount--;
    gClassNameCount().remove("WebKitClassFactory");
}

// IUnknown -------------------------------------------------------------------
HRESULT WebKitClassFactory::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IClassFactory))
        *ppvObject = static_cast<IClassFactory*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebKitClassFactory::AddRef()
{
    return ++m_refCount;
}

ULONG WebKitClassFactory::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef && !gLockCount)
        delete(this);

    return newRef;
}

// FIXME: Remove these functions once all createInstance() functions return COMPtr.
template <typename T>
static T* leakRefFromCreateInstance(T* object)
{
    return object;
}

template <typename T>
static T* leakRefFromCreateInstance(COMPtr<T> object)
{
    return object.leakRef();
}

// IClassFactory --------------------------------------------------------------
HRESULT WebKitClassFactory::CreateInstance(_In_opt_ IUnknown* pUnkOuter, _In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;

    IUnknown* unknown = nullptr;
    *ppvObject = nullptr;
    
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

#define INITIALIZE_IF_CLASS(cls) \
    if (IsEqualGUID(m_targetClass, CLSID_##cls)) \
        unknown = static_cast<I##cls*>(leakRefFromCreateInstance(cls::createInstance())); \
    else \
    // end of macro

    // These #defines are needed to appease the INITIALIZE_IF_CLASS macro.
    // There is no ICFDictionaryPropertyBag, we use IPropertyBag instead.
#define ICFDictionaryPropertyBag IPropertyBag
    // There is no class called WebURLRequest -- WebMutableURLRequest implements it for us.
#define WebURLRequest WebMutableURLRequest

    FOR_EACH_COCLASS(INITIALIZE_IF_CLASS)
        // This is the final else case
        return CLASS_E_CLASSNOTAVAILABLE;

#undef ICFDictionaryPropertyBag
#undef WebURLRequest
#undef INITIALIZE_IF_CLASS

    if (!unknown)
        return E_OUTOFMEMORY;

    HRESULT hr = unknown->QueryInterface(riid, ppvObject);
    if (FAILED(hr))
        delete unknown;
    else
        unknown->Release(); // both createInstance() and QueryInterface() added refs

    return hr;
}

HRESULT WebKitClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
        gLockCount++;
    else
        gLockCount--;

    return S_OK;
}
