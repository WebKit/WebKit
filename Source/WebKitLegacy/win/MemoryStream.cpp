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
#include "MemoryStream.h"

using std::min;
using namespace WebCore;

// MemoryStream ---------------------------------------------------------------

MemoryStream::MemoryStream(RefPtr<SharedBuffer>&& buffer)
    : m_buffer(WTFMove(buffer))
{
    gClassCount++;
    gClassNameCount().add("MemoryStream");
}

MemoryStream::~MemoryStream()
{
    gClassCount--;
    gClassNameCount().remove("MemoryStream");
}

COMPtr<MemoryStream> MemoryStream::createInstance(RefPtr<SharedBuffer>&& buffer)
{
    return new MemoryStream(WTFMove(buffer));
}

// IUnknown -------------------------------------------------------------------

HRESULT MemoryStream::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_ISequentialStream))
        *ppvObject = static_cast<ISequentialStream*>(this);
    else if (IsEqualGUID(riid, IID_IStream))
        *ppvObject = static_cast<IStream*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG MemoryStream::AddRef()
{
    return ++m_refCount;
}

ULONG MemoryStream::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// ISequentialStream ----------------------------------------------------------

HRESULT MemoryStream::Read(/* [length_is][size_is][out] */ void* pv, /* [in] */ ULONG cb, /* [out] */ ULONG* pcbRead)
{
    *pcbRead = 0;

    if (!m_buffer)
        return E_FAIL;

    if (m_pos >= m_buffer->size())
        return S_FALSE;

    if (m_pos + cb < m_buffer->size())
        *pcbRead = cb;
    else
        *pcbRead = (ULONG) (m_buffer->size() - m_pos);

    memcpy(pv, m_buffer->data() + m_pos, *pcbRead);
    m_pos += *pcbRead;

    return S_OK;
}

HRESULT MemoryStream::Write(/* [size_is][in] */ const void* /*pv*/, /* [in] */ ULONG /*cb*/, /* [out] */ ULONG* /*pcbWritten*/)
{
    // we use this for read-only streams
    return STG_E_ACCESSDENIED;
}

// IStream --------------------------------------------------------------------

HRESULT MemoryStream::Seek(/* [in] */ LARGE_INTEGER dlibMove, /* [in] */ DWORD dwOrigin, /* [out] */ ULARGE_INTEGER* plibNewPosition)
{
    if (!m_buffer)
        return E_FAIL;

    size_t proposedPos = m_pos;
    size_t lowPartNeg;
    switch (dwOrigin) {
        case STREAM_SEEK_SET:
            proposedPos = dlibMove.LowPart;
            if (dlibMove.HighPart)
                return E_FAIL;
            break;
        case STREAM_SEEK_CUR:
            if (!dlibMove.HighPart) {
                if (dlibMove.LowPart > (m_buffer->size() - m_pos))
                    return E_FAIL;
                proposedPos += dlibMove.LowPart;
            } else if (dlibMove.HighPart == -1) {
                lowPartNeg = (~dlibMove.LowPart)+1;
                if (lowPartNeg > m_pos)
                    return E_FAIL;
                proposedPos = m_pos - lowPartNeg;
            } else
                return E_FAIL;
            break;
        case STREAM_SEEK_END:
            if (dlibMove.HighPart != -1)
                return E_FAIL;
            lowPartNeg = (~dlibMove.LowPart)+1;
            if (lowPartNeg > m_buffer->size())
                return E_FAIL;
            proposedPos = m_buffer->size() - lowPartNeg;
            break;
        default:
            return E_FAIL;
    }

    if (proposedPos >= m_buffer->size())
        return E_FAIL;

    if (plibNewPosition) {
        plibNewPosition->HighPart = 0;
        plibNewPosition->LowPart = (DWORD) m_pos;
    }

    return S_OK;
}

HRESULT MemoryStream::SetSize(ULARGE_INTEGER /*libNewSize*/)
{
    return STG_E_INVALIDFUNCTION;
}

HRESULT MemoryStream::CopyTo(/* [unique][in] */ IStream* pstm, /* [in] */ ULARGE_INTEGER cb, /* [out] */ ULARGE_INTEGER* pcbRead, /* [out] */ ULARGE_INTEGER* pcbWritten)
{
    if (!m_buffer)
        return E_FAIL;
    if (cb.HighPart) {
        cb.HighPart = 0;
        cb.LowPart = (DWORD)-1;
    }

    ULONG written = 0;
    ULONG read = min(cb.LowPart, (ULONG)(m_buffer->size()-m_pos));
    HRESULT hr = pstm->Write(m_buffer->data()+m_pos, read, &written);
    if (pcbWritten) {
        pcbWritten->HighPart = 0;
        pcbWritten->LowPart = written;
    }
    if (pcbRead) {
        pcbRead->HighPart = 0;
        pcbRead->LowPart = read;
    }

    return hr;
}

HRESULT MemoryStream::Commit(DWORD /*grfCommitFlags*/)
{
    return S_OK;
}

HRESULT MemoryStream::Revert()
{
    return S_OK;
}

HRESULT MemoryStream::LockRegion(ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/)
{
    return STG_E_INVALIDFUNCTION;
}

HRESULT MemoryStream::UnlockRegion(ULARGE_INTEGER /*libOffset*/, ULARGE_INTEGER /*cb*/, DWORD /*dwLockType*/)
{
    return STG_E_INVALIDFUNCTION;
}

HRESULT MemoryStream::Stat(_Out_ STATSTG* pstatstg, DWORD /*grfStatFlag*/)
{
    if (!pstatstg)
        return E_POINTER;

    if (!m_buffer)
        return E_FAIL;

    memset(pstatstg, 0, sizeof(STATSTG));
    pstatstg->type = STGTY_STREAM;
    pstatstg->cbSize.LowPart = (DWORD) m_buffer->size();
    return S_OK;
}

HRESULT MemoryStream::Clone(_COM_Outptr_ IStream** ppstm)
{
    MemoryStream::createInstance(m_buffer.copyRef()).copyRefTo(ppstm);
    // FIXME: MSDN says we should be returning STG_E_INSUFFICIENT_MEMORY instead of E_OUTOFMEMORY here.
    return (*ppstm) ? S_OK : E_OUTOFMEMORY;
}
