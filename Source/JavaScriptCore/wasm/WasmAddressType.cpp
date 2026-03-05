/* Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WasmAddressType.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmOps.h"

namespace JSC { namespace Wasm {


AddressType::AddressType(AddressType::Kind addressType) : m_type(addressType) { };

AddressType::AddressType(bool is64Bit) : m_type(is64Bit ? AddressType::I64 : AddressType::I32) { };


AddressType::AddressType(TypeKind typeKind)
{
    switch (typeKind) {
    case TypeKind::I32:
        m_type = AddressType::I32;
        break;
    case TypeKind::I64:
        m_type = AddressType::I64;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED("Invalid Wasm Type to AddressType conversion");
    }
}

TypeKind AddressType::asTypeKind() const
{
    switch (m_type) {
    case AddressType::I32:
        return TypeKind::I32;
    case AddressType::I64:
        return TypeKind::I64;
    }
    RELEASE_ASSERT_NOT_REACHED("Invalid Wasm Type to AddressType conversion");
}

bool operator==(const AddressType& lhs, const AddressType& rhs)
{
    return lhs.m_type == rhs.m_type;
}

bool operator!=(const AddressType& lhs, const AddressType& rhs)
{
    return lhs.m_type != rhs.m_type;
}

} }

#endif // ENABLE(WEBASSEMBLY)
