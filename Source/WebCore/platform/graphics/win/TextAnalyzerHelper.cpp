/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "TextAnalyzerHelper.h"

#if USE(DIRECT2D)

namespace WebCore {

TextAnalyzerHelper::TextAnalyzerHelper(WCHAR* localeName, WCHAR* buffer, unsigned bufferLength)
    : m_localeName(localeName)
    , m_buffer(buffer)
    , m_bufferLength(bufferLength)
{
}

HRESULT TextAnalyzerHelper::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink)))
        *ppvObject = static_cast<IDWriteTextAnalysisSink*>(this);
    else if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource)))
        *ppvObject = static_cast<IDWriteTextAnalysisSink*>(this);
    else
        return E_FAIL;

    AddRef();
    return S_OK;
}

ULONG TextAnalyzerHelper::AddRef()
{
    return ++m_refCount;
}

ULONG TextAnalyzerHelper::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT TextAnalyzerHelper::GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength)
{
    if (!textString || !textLength)
        return E_POINTER;

    if (textPosition >= m_bufferLength) {
        *textString = nullptr;
        *textLength = 0;
        return S_OK;
    }

    *textString = &m_buffer[textPosition];
    *textLength = m_bufferLength - textPosition;

    return S_OK;
}

HRESULT TextAnalyzerHelper::GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength)
{
    if (!textString || !textLength)
        return E_POINTER;

    if (!textPosition || textPosition > m_bufferLength) {
        *textString = nullptr;
        *textLength = 0;
        return S_OK;
    }

    *textString = &m_buffer[0];
    *textLength = textPosition;

    return S_OK;
}

HRESULT TextAnalyzerHelper::GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName)
{
    *localeName = m_localeName;
    *textLength = m_bufferLength - textPosition;

    return S_OK;
}

HRESULT TextAnalyzerHelper::GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution)
{
    *textLength = m_bufferLength - textPosition;
    return S_OK;
}

HRESULT TextAnalyzerHelper::SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT*)
{
    return E_NOTIMPL;
}

HRESULT TextAnalyzerHelper::SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* analysis)
{
    m_analysis = *analysis;

    return S_OK;
}

HRESULT TextAnalyzerHelper::SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel)
{
    return E_NOTIMPL;
}

HRESULT TextAnalyzerHelper::SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution*)
{
    return E_NOTIMPL;
}

}

#endif
