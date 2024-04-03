/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_WIN)

#include "include/core/SkStream.h"
#include "src/utils/win/SkIStream.h"

/**
 * SkBaseIStream
 */
SkBaseIStream::SkBaseIStream() : _refcount(1) { }
SkBaseIStream::~SkBaseIStream() { }

SK_STDMETHODIMP SkBaseIStream::QueryInterface(REFIID iid, void ** ppvObject) {
    if (nullptr == ppvObject) {
        return E_INVALIDARG;
    }
    if (iid == __uuidof(IUnknown)
        || iid == __uuidof(IStream)
        || iid == __uuidof(ISequentialStream))
    {
        *ppvObject = static_cast<IStream*>(this);
        AddRef();
        return S_OK;
    } else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
}

SK_STDMETHODIMP_(ULONG) SkBaseIStream::AddRef() {
    return (ULONG)InterlockedIncrement(&_refcount);
}

SK_STDMETHODIMP_(ULONG) SkBaseIStream::Release() {
    ULONG res = (ULONG) InterlockedDecrement(&_refcount);
    if (0 == res) {
        delete this;
    }
    return res;
}

// ISequentialStream Interface
SK_STDMETHODIMP SkBaseIStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Write(void const* pv, ULONG cb, ULONG* pcbWritten)
{ return E_NOTIMPL; }

// IStream Interface
SK_STDMETHODIMP SkBaseIStream::SetSize(ULARGE_INTEGER)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Commit(DWORD)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Revert()
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Clone(IStream**)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Seek(LARGE_INTEGER liDistanceToMove,
                                    DWORD dwOrigin,
                                    ULARGE_INTEGER* lpNewFilePointer)
{ return E_NOTIMPL; }

SK_STDMETHODIMP SkBaseIStream::Stat(STATSTG* pStatstg, DWORD grfStatFlag)
{ return E_NOTIMPL; }


/**
 * SkIStream
 */
SkIStream::SkIStream(std::unique_ptr<SkStreamAsset> stream)
    : SkBaseIStream()
    , fSkStream(std::move(stream))
    , fLocation()
{
    this->fSkStream->rewind();
}

SkIStream::~SkIStream() {}

HRESULT SkIStream::CreateFromSkStream(std::unique_ptr<SkStreamAsset> stream, IStream** ppStream) {
    if (nullptr == stream) {
        return E_INVALIDARG;
    }
    *ppStream = new SkIStream(std::move(stream));
    return S_OK;
}

// ISequentialStream Interface
SK_STDMETHODIMP SkIStream::Read(void* pv, ULONG cb, ULONG* pcbRead) {
    *pcbRead = static_cast<ULONG>(this->fSkStream->read(pv, cb));
    this->fLocation.QuadPart += *pcbRead;
    return (*pcbRead == cb) ? S_OK : S_FALSE;
}

SK_STDMETHODIMP SkIStream::Write(void const* pv, ULONG cb, ULONG* pcbWritten) {
    return STG_E_CANTSAVE;
}

// IStream Interface
SK_STDMETHODIMP SkIStream::Seek(LARGE_INTEGER liDistanceToMove,
                                DWORD dwOrigin,
                                ULARGE_INTEGER* lpNewFilePointer)
{
    HRESULT hr = S_OK;

    switch(dwOrigin) {
    case STREAM_SEEK_SET: {
        if (!this->fSkStream->rewind()) {
            hr = E_FAIL;
        } else {
            size_t skip = static_cast<size_t>(liDistanceToMove.QuadPart);
            size_t skipped = this->fSkStream->skip(skip);
            this->fLocation.QuadPart = skipped;
            if (skipped != skip) {
                hr = E_FAIL;
            }
        }
        break;
    }
    case STREAM_SEEK_CUR: {
        size_t skip = static_cast<size_t>(liDistanceToMove.QuadPart);
        size_t skipped = this->fSkStream->skip(skip);
        this->fLocation.QuadPart += skipped;
        if (skipped != skip) {
            hr = E_FAIL;
        }
        break;
    }
    case STREAM_SEEK_END: {
        if (!this->fSkStream->rewind()) {
            hr = E_FAIL;
        } else {
            size_t skip = static_cast<size_t>(this->fSkStream->getLength() +
                                              liDistanceToMove.QuadPart);
            size_t skipped = this->fSkStream->skip(skip);
            this->fLocation.QuadPart = skipped;
            if (skipped != skip) {
                hr = E_FAIL;
            }
        }
        break;
    }
    default:
        hr = STG_E_INVALIDFUNCTION;
        break;
    }

    if (lpNewFilePointer) {
        lpNewFilePointer->QuadPart = this->fLocation.QuadPart;
    }
    return hr;
}

SK_STDMETHODIMP SkIStream::Stat(STATSTG* pStatstg, DWORD grfStatFlag) {
    if (0 == (grfStatFlag & STATFLAG_NONAME)) {
        return STG_E_INVALIDFLAG;
    }
    pStatstg->pwcsName = nullptr;
    pStatstg->cbSize.QuadPart = this->fSkStream->getLength();
    pStatstg->clsid = CLSID_NULL;
    pStatstg->type = STGTY_STREAM;
    pStatstg->grfMode = STGM_READ;
    return S_OK;
}


/**
 * SkIWStream
 */
SkWIStream::SkWIStream(SkWStream* stream)
    : SkBaseIStream()
    , fSkWStream(stream)
{ }

SkWIStream::~SkWIStream() {
    if (this->fSkWStream) {
        this->fSkWStream->flush();
    }
}

HRESULT SkWIStream::CreateFromSkWStream(SkWStream* stream, IStream ** ppStream) {
    *ppStream = new SkWIStream(stream);
    return S_OK;
}

// ISequentialStream Interface
SK_STDMETHODIMP SkWIStream::Write(void const* pv, ULONG cb, ULONG* pcbWritten) {
    HRESULT hr = S_OK;
    bool wrote = this->fSkWStream->write(pv, cb);
    if (wrote) {
        *pcbWritten = cb;
    } else {
        *pcbWritten = 0;
        hr = S_FALSE;
    }
    return hr;
}

// IStream Interface
SK_STDMETHODIMP SkWIStream::Commit(DWORD) {
    this->fSkWStream->flush();
    return S_OK;
}

SK_STDMETHODIMP SkWIStream::Stat(STATSTG* pStatstg, DWORD grfStatFlag) {
    if (0 == (grfStatFlag & STATFLAG_NONAME)) {
        return STG_E_INVALIDFLAG;
    }
    pStatstg->pwcsName = nullptr;
    pStatstg->cbSize.QuadPart = 0;
    pStatstg->clsid = CLSID_NULL;
    pStatstg->type = STGTY_STREAM;
    pStatstg->grfMode = STGM_WRITE;
    return S_OK;
}
#endif//defined(SK_BUILD_FOR_WIN)
