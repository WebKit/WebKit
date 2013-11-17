/*
 * Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef Base64_h
#define Base64_h

#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

enum Base64EncodePolicy {
    Base64DoNotInsertLFs,
    Base64InsertLFs,
    Base64URLPolicy // No padding, no LFs.
};

enum Base64DecodePolicy {
    Base64FailOnInvalidCharacterOrExcessPadding,
    Base64FailOnInvalidCharacter,
    Base64IgnoreWhitespace,
    Base64IgnoreInvalidCharacters
};

class SignedOrUnsignedCharVectorAdapter {
public:
    SignedOrUnsignedCharVectorAdapter(Vector<char>& vector) { m_vector.c = &vector; }
    SignedOrUnsignedCharVectorAdapter(Vector<uint8_t>& vector) { m_vector.u = &vector; }

    operator Vector<char>&() { return *m_vector.c; }
    void clear() { m_vector.c->clear(); }

private:
    union {
        Vector<char>* c;
        Vector<uint8_t>* u;
    } m_vector;
};

class ConstSignedOrUnsignedCharVectorAdapter {
public:
    ConstSignedOrUnsignedCharVectorAdapter(const Vector<char>& vector) { m_vector.c = &vector; }
    ConstSignedOrUnsignedCharVectorAdapter(const Vector<uint8_t>& vector) { m_vector.u = &vector; }

    operator const Vector<char>&() { return *m_vector.c; }
    const char* data() const { return m_vector.c->data(); }
    size_t size() const { return m_vector.c->size(); }

private:
    union {
        const Vector<char>* c;
        const Vector<uint8_t>* u;
    } m_vector;
};

WTF_EXPORT_PRIVATE void base64Encode(const void*, unsigned, Vector<char>&, Base64EncodePolicy = Base64DoNotInsertLFs);
WTF_EXPORT_PRIVATE void base64Encode(ConstSignedOrUnsignedCharVectorAdapter, Vector<char>&, Base64EncodePolicy = Base64DoNotInsertLFs);
WTF_EXPORT_PRIVATE void base64Encode(const CString&, Vector<char>&, Base64EncodePolicy = Base64DoNotInsertLFs);
WTF_EXPORT_PRIVATE String base64Encode(const void*, unsigned, Base64EncodePolicy = Base64DoNotInsertLFs);
WTF_EXPORT_PRIVATE String base64Encode(ConstSignedOrUnsignedCharVectorAdapter, Base64EncodePolicy = Base64DoNotInsertLFs);
WTF_EXPORT_PRIVATE String base64Encode(const CString&, Base64EncodePolicy = Base64DoNotInsertLFs);

WTF_EXPORT_PRIVATE bool base64Decode(const String&, SignedOrUnsignedCharVectorAdapter, Base64DecodePolicy = Base64FailOnInvalidCharacter);
WTF_EXPORT_PRIVATE bool base64Decode(const Vector<char>&, SignedOrUnsignedCharVectorAdapter, Base64DecodePolicy = Base64FailOnInvalidCharacter);
WTF_EXPORT_PRIVATE bool base64Decode(const char*, unsigned, SignedOrUnsignedCharVectorAdapter, Base64DecodePolicy = Base64FailOnInvalidCharacter);

inline void base64Encode(ConstSignedOrUnsignedCharVectorAdapter in, Vector<char>& out, Base64EncodePolicy policy)
{
    base64Encode(in.data(), in.size(), out, policy);
}

inline void base64Encode(const CString& in, Vector<char>& out, Base64EncodePolicy policy)
{
    base64Encode(in.data(), in.length(), out, policy);
}

inline String base64Encode(ConstSignedOrUnsignedCharVectorAdapter in, Base64EncodePolicy policy)
{
    return base64Encode(in.data(), in.size(), policy);
}

inline String base64Encode(const CString& in, Base64EncodePolicy policy)
{
    return base64Encode(in.data(), in.length(), policy);
}

// ======================================================================================
// All the same functions modified for base64url, as defined in RFC 4648.
// This format uses '-' and '_' instead of '+' and '/' respectively.
// ======================================================================================

WTF_EXPORT_PRIVATE void base64URLEncode(const void*, unsigned, Vector<char>&);
WTF_EXPORT_PRIVATE void base64URLEncode(ConstSignedOrUnsignedCharVectorAdapter, Vector<char>&);
WTF_EXPORT_PRIVATE void base64URLEncode(const CString&, Vector<char>&);
WTF_EXPORT_PRIVATE String base64URLEncode(const void*, unsigned);
WTF_EXPORT_PRIVATE String base64URLEncode(ConstSignedOrUnsignedCharVectorAdapter);
WTF_EXPORT_PRIVATE String base64URLEncode(const CString&);

WTF_EXPORT_PRIVATE bool base64URLDecode(const String&, SignedOrUnsignedCharVectorAdapter);
WTF_EXPORT_PRIVATE bool base64URLDecode(const Vector<char>&, SignedOrUnsignedCharVectorAdapter);
WTF_EXPORT_PRIVATE bool base64URLDecode(const char*, unsigned, SignedOrUnsignedCharVectorAdapter);

inline void base64URLEncode(ConstSignedOrUnsignedCharVectorAdapter in, Vector<char>& out)
{
    base64URLEncode(in.data(), in.size(), out);
}

inline void base64URLEncode(const CString& in, Vector<char>& out)
{
    base64URLEncode(in.data(), in.length(), out);
}

inline String base64URLEncode(ConstSignedOrUnsignedCharVectorAdapter in)
{
    return base64URLEncode(in.data(), in.size());
}

inline String base64URLEncode(const CString& in)
{
    return base64URLEncode(in.data(), in.length());
}

} // namespace WTF

using WTF::Base64EncodePolicy;
using WTF::Base64DoNotInsertLFs;
using WTF::Base64InsertLFs;
using WTF::Base64DecodePolicy;
using WTF::Base64FailOnInvalidCharacterOrExcessPadding;
using WTF::Base64FailOnInvalidCharacter;
using WTF::Base64IgnoreWhitespace;
using WTF::Base64IgnoreInvalidCharacters;
using WTF::base64Encode;
using WTF::base64Decode;
using WTF::base64URLDecode;

#endif // Base64_h
