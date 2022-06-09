/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "ParseHash.h"

#include "SourceCode.h"
#include <wtf/SHA1.h>

namespace JSC {

ParseHash::ParseHash(const SourceCode& sourceCode)
{
    SHA1 sha1;
    sha1.addBytes(sourceCode.toUTF8());
    SHA1::Digest digest;
    sha1.computeHash(digest);
    unsigned hash = digest[0] | (digest[1] << 8) | (digest[2] << 16) | (digest[3] << 24);

    if (hash == 0 || hash == 1)
        hash += 0x2d5a93d0; // Ensures a non-zero hash, and gets us #Azero0 for CodeForCall and #Azero1 for CodeForConstruct.
    static_assert(static_cast<unsigned>(CodeForCall) == 0, "");
    static_assert(static_cast<unsigned>(CodeForConstruct) == 1, "");
    unsigned hashForCall = hash ^ static_cast<unsigned>(CodeForCall);
    unsigned hashForConstruct = hash ^ static_cast<unsigned>(CodeForConstruct);

    m_hashForCall = CodeBlockHash(hashForCall);
    m_hashForConstruct = CodeBlockHash(hashForConstruct);
    ASSERT(hashForCall);
    ASSERT(hashForConstruct);
}

} // namespace JSC

