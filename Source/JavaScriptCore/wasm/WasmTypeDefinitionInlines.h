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

#include "WasmTypeDefinition.h"

namespace JSC { namespace Wasm {

inline TypeInformation& TypeInformation::singleton()
{
    static TypeInformation* theOne;
    static std::once_flag typeInformationFlag;

    std::call_once(typeInformationFlag, [] () {
        theOne = new TypeInformation;
    });
    return *theOne;
}

inline const TypeDefinition& TypeInformation::get(TypeIndex index)
{
    ASSERT(index != TypeDefinition::invalidIndex);
    return *bitwise_cast<const TypeDefinition*>(index);
}

inline const FunctionSignature& TypeInformation::getFunctionSignature(TypeIndex index)
{
    const TypeDefinition& signature = get(index);
    if (signature.is<Projection>()) {
        const TypeDefinition& expanded = signature.expand();
        ASSERT(expanded.is<FunctionSignature>());
        return *expanded.as<FunctionSignature>();
    }
    ASSERT(signature.is<FunctionSignature>());
    return *signature.as<FunctionSignature>();
}

inline TypeIndex TypeInformation::get(const TypeDefinition& type)
{
    if (ASSERT_ENABLED) {
        TypeInformation& info = singleton();
        Locker locker { info.m_lock };
        ASSERT_UNUSED(info, info.m_typeSet.contains(TypeHash { const_cast<TypeDefinition&>(type) }));
    }
    return bitwise_cast<TypeIndex>(&type);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
