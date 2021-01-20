/*
 * Copyright (C) 2007, 2014-2015 Apple Inc.  All rights reserved.
 * Copyright (C) 2012 Baidu Inc.  All rights reserved.
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

#ifndef DRTDataObject_h
#define DRTDataObject_h

#include <ShlObj.h>
#include <objidl.h>
#include <wtf/Vector.h>

FORMATETC* cfHDropFormat();

FORMATETC* cfFileNameWFormat();

FORMATETC* cfUrlWFormat();

struct StgMediumDeleter {
    void operator()(STGMEDIUM* medium)
    {
        ::ReleaseStgMedium(medium);
    }
};

class DRTDataObject final : public IDataObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void CopyMedium(STGMEDIUM* pMedDest, STGMEDIUM* pMedSrc, FORMATETC* pFmtSrc);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);        
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IDataObject
    virtual HRESULT STDMETHODCALLTYPE GetData(_In_ FORMATETC*, _Out_ STGMEDIUM*);
    virtual HRESULT STDMETHODCALLTYPE GetDataHere(_In_ FORMATETC*, _Inout_ STGMEDIUM*);
    virtual HRESULT STDMETHODCALLTYPE QueryGetData(_In_opt_ FORMATETC*);
    virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(_In_opt_ FORMATETC*, _Out_ FORMATETC*);
    virtual HRESULT STDMETHODCALLTYPE SetData(_In_ FORMATETC*, _In_ STGMEDIUM*, BOOL release);
    virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, _COM_Outptr_opt_ IEnumFORMATETC**);
    virtual HRESULT STDMETHODCALLTYPE DAdvise(_In_ FORMATETC*, DWORD, _Inout_ IAdviseSink*, _Out_ DWORD*);
    virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD);
    virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(_COM_Outptr_opt_ IEnumSTATDATA**);

    void clearData(CLIPFORMAT);
    
    static HRESULT createInstance(DRTDataObject**);
private:
    DRTDataObject();
    long m_ref { 1 };
    Vector<std::unique_ptr<FORMATETC>> m_formats;
    Vector<std::unique_ptr<STGMEDIUM, StgMediumDeleter>> m_medium;
};

#endif // DRTDataObject_h
