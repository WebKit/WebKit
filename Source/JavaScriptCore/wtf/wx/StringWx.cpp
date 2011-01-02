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

#include <unicode/ustring.h>
#include <wtf/text/CString.h>
#include <wx/defs.h>
#include <wx/string.h>

namespace WTF {

String::String(const wxString& wxstr)
{
#if !wxUSE_UNICODE
    #error "This code only works in Unicode build of wxWidgets"
#endif

#if SIZEOF_WCHAR_T == sizeof(UChar)

    m_impl = StringImpl::create(wxstr.wc_str(), wxstr.length());

#else // SIZEOF_WCHAR_T == 4

    // NB: we can't simply use wxstr.mb_str(wxMBConvUTF16()) here because
    //     the number of characters in UTF-16 encoding of the string may differ
    //     from the number of UTF-32 values and we can't get the length from
    //     returned buffer:

#if defined(wxUSE_UNICODE_UTF8) && wxUSE_UNICODE_UTF8
    // in wx3's UTF8 mode, wc_str() returns a buffer, not raw pointer
    wxWCharBuffer wideString(wxstr.wc_str());
#else
    const wxChar *wideString = wxstr.wc_str();
#endif
    size_t wideLength = wxstr.length();

    UChar* data;
    wxMBConvUTF16 conv;
    unsigned utf16Length = conv.FromWChar(0, 0, wideString, wideLength);
    m_impl = StringImpl::createUninitialized(utf16Length, data)
    conv.FromWChar(data, utf16Length, wideString, wideLength);

#endif // SIZEOF_WCHAR_T == 4
}

String::operator wxString() const
{
    return wxString(utf8().data(), wxConvUTF8);
}

} // namespace WTF
