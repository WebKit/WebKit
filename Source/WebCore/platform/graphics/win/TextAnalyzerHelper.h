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

#pragma once

#include <dwrite.h>

namespace WebCore {

struct TextAnalyzerHelper : public IDWriteTextAnalysisSink, IDWriteTextAnalysisSource {
    TextAnalyzerHelper(WCHAR* localeName, WCHAR* buffer, unsigned bufferLength);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void**);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IDWriteTextAnalysisSource 
    virtual HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength);
    virtual HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength);
    virtual DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() { return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT; }
    virtual HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName);
    virtual HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution**);

    // IDWriteTextAnalysisSink
    virtual HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT*);
    virtual HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS*);
    virtual HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel);
    virtual HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution*);

    WCHAR* m_localeName { nullptr };
    WCHAR* m_buffer { nullptr };
    unsigned m_bufferLength { 0 };
    DWRITE_SCRIPT_ANALYSIS m_analysis { };
    ULONG m_refCount { 0 };
};

}
