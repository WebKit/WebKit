/*
 * Copyright (C) 2007, 2014-2015 Apple Inc.  All rights reserved.
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

#ifndef WCDataObject_h
#define WCDataObject_h

#include "DragData.h"
#include <ShlObj.h>
#include <objidl.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

struct StgMediumDeleter {
    void operator()(STGMEDIUM* medium)
    {
        ::ReleaseStgMedium(medium);
    }
};

class WCDataObject final : public IDataObject {
public:
    void CopyMedium(STGMEDIUM* pMedDest, STGMEDIUM* pMedSrc, FORMATETC* pFmtSrc);

    //IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);        
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    //IDataObject
    virtual HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatIn, STGMEDIUM* pmedium);
    virtual HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* pformat, STGMEDIUM* pmedium);
    virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformat);
    virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* pformatectIn,FORMATETC* pformatOut);
    virtual HRESULT STDMETHODCALLTYPE SetData(FORMATETC* pformat, STGMEDIUM*pmedium, BOOL release);
    virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc);
    virtual HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*);
    virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD);
    virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**);

    void clearData(CLIPFORMAT);
    
    static HRESULT createInstance(WCDataObject**);
    static HRESULT createInstance(WCDataObject**, const DragDataMap&);
private:
    WCDataObject();

    Vector<std::unique_ptr<FORMATETC>> m_formats;
    Vector<std::unique_ptr<STGMEDIUM, StgMediumDeleter>> m_medium;
    long m_ref { 1 };
};

}

#endif //!WCDataObject_h
