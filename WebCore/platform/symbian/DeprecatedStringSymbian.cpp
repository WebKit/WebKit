/*
* ==============================================================================
*    Copyright (c) 2006, Nokia Corporation
*    All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without
*   modification, are permitted provided that the following conditions
*   are met:
*
*      * Redistributions of source code must retain the above copyright
*        notice, this list of conditions and the following disclaimer.
*      * Redistributions in binary form must reproduce the above copyright
*        notice, this list of conditions and the following disclaimer in
*        the documentation and/or other materials provided with the
*        distribution.
*      * Neither the name of the Nokia Corporation nor the names of its
*        contributors may be used to endorse or promote products derived
*        from this software without specific prior written permission.
*
*   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
*   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
*   DAMAGE.
*
* ==============================================================================
*/

#include "config.h"
#include "DeprecatedString.h"

namespace WebCore {

void DeprecatedString::setBufferFromDes(const TDesC& des)
{
    setUnicode((const DeprecatedChar *)des.Ptr(), (uint)des.Length());
}

void DeprecatedString::setBufferFromDes(const TDesC8& des)
{
    setLatin1((const char*)des.Ptr(), (uint)des.Length());
}

DeprecatedString DeprecatedString::fromDes(const TDesC& des)
{
    DeprecatedString s;
    s.setBufferFromDes(des);
    return s;
}

DeprecatedString DeprecatedString::fromDes(const TDesC8& des)
{
    DeprecatedString s;
    s.setBufferFromDes(des);
    return s;
}

TPtrC DeprecatedString::des() const
{
    if (!dataHandle[0]->_isUnicodeValid)
        dataHandle[0]->makeUnicode();

    TPtrC tstr((const TUint16*)unicode(), length() );

    return tstr;
}

TPtrC8 DeprecatedString::des8() const
{
    TPtrC8 tstr((const TUint8*)latin1(), length());

    return tstr;
}

}