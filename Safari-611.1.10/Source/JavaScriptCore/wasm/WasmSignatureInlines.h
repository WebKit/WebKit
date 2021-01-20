/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmSignature.h"

namespace JSC { namespace Wasm {

inline SignatureInformation& SignatureInformation::singleton()
{
    std::call_once(signatureInformationFlag, [] () {
        theOne = new SignatureInformation;
    });
    return *theOne;
}

inline const Signature& SignatureInformation::get(SignatureIndex index)
{
    ASSERT(index != Signature::invalidIndex);
    return *bitwise_cast<const Signature*>(index);
}

inline SignatureIndex SignatureInformation::get(const Signature& signature)
{
    if (ASSERT_ENABLED) {
        SignatureInformation& info = singleton();
        auto locker = holdLock(info.m_lock);
        ASSERT_UNUSED(info, info.m_signatureSet.contains(SignatureHash { makeRef(const_cast<Signature&>(signature)) }));
    }
    return bitwise_cast<SignatureIndex>(&signature);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
