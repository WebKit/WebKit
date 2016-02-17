/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBError_h
#define IDBError_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseException.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBError {
public:
    IDBError() { }
    IDBError(ExceptionCode);
    IDBError(ExceptionCode, const String& message);

    IDBError& operator=(const IDBError&);

    ExceptionCode code() const { return m_code; }
    String name() const;
    String message() const;

    bool isNull() const { return m_code == IDBDatabaseException::NoError; }

    IDBError isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBError&);

private:
    ExceptionCode m_code { IDBDatabaseException::NoError };
    String m_message;
};

template<class Encoder>
void IDBError::encode(Encoder& encoder) const
{
    encoder << m_code << m_message;
}
    
template<class Decoder>
bool IDBError::decode(Decoder& decoder, IDBError& error)
{
    if (!decoder.decode(error.m_code))
        return false;

    if (!decoder.decode(error.m_message))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBError_h
