/*
 * Copyright (C) 2007 Vaclav Slavik, Kevin Ollivier <kevino@theolliviers.com>
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
#include "PlatformString.h"

#include "unicode/ustring.h"
#include <wtf/text/CString.h>

#include <wx/defs.h>
#include <wx/string.h>

namespace WebCore {

// String conversions
String::String(const wxString& wxstr)
{
#if !wxUSE_UNICODE
    #error "This code only works in Unicode build of wxWidgets"
#endif

    // ICU's UChar is 16bit wide, UTF-16, and the code below counts on it, so
    // it would break if the definition changed:
    wxCOMPILE_TIME_ASSERT(sizeof(UChar) == 2, UCharSizeMustBe16Bit);

#if SIZEOF_WCHAR_T == 2 // wchar_t==UChar

    const UChar* str = wxstr.wc_str();
    const size_t len = wxstr.length();

#else // SIZEOF_WCHAR_T == 4

    // NB: we can't simply use wxstr.mb_str(wxMBConvUTF16()) here because
    //     the number of characters in UTF-16 encoding of the string may differ
    //     from the number of UTF-32 values and we can't get the length from
    //     returned buffer:

#if defined(wxUSE_UNICODE_UTF8) && wxUSE_UNICODE_UTF8
    // in wx3's UTF8 mode, wc_str() returns a buffer, not raw pointer
   wxWCharBuffer widestr(wxstr.wc_str());
#else
    const wxChar *widestr = wxstr.wc_str();
#endif
    const size_t widelen = wxstr.length();

    // allocate buffer for the UTF-16 string:
    wxMBConvUTF16 conv;
    const size_t utf16bufLen = conv.FromWChar(NULL, 0, widestr, widelen);
    wxCharBuffer utf16buf(utf16bufLen);

    // and convert wxString to UTF-16 (=UChar*):
    const UChar* str = (const UChar*)utf16buf.data();
    size_t len = conv.FromWChar(utf16buf.data(), utf16bufLen, widestr, widelen) / 2;

#endif // SIZEOF_WCHAR_T == 4

    // conversion to UTF-16 or getting internal buffer isn't supposed to fail:
    wxASSERT_MSG(str != NULL, _T("failed string conversion?"));

    m_impl = StringImpl::create(str, len);
}

String::operator wxString() const
{
    return wxString(utf8().data(), wxConvUTF8);
}

}

// vim: ts=4 sw=4 et
