/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "BytecodeIntrinsicRegistry.h"

#include "CommonIdentifiers.h"
#include "Nodes.h"

namespace JSC {

#define INITIALISE_BYTECODE_INTRINSIC_NAMES_TO_SET(name) m_bytecodeIntrinsicMap.add(propertyNames.name##PrivateName.impl(), &BytecodeIntrinsicNode::emit_intrinsic_##name);

BytecodeIntrinsicRegistry::BytecodeIntrinsicRegistry(const CommonIdentifiers& propertyNames)
    : m_propertyNames(propertyNames)
    , m_bytecodeIntrinsicMap()
{
    JSC_COMMON_BYTECODE_INTRINSICS_EACH_NAME(INITIALISE_BYTECODE_INTRINSIC_NAMES_TO_SET)
}

BytecodeIntrinsicNode::EmitterType BytecodeIntrinsicRegistry::lookup(const Identifier& ident) const
{
    if (!m_propertyNames.isPrivateName(ident))
        return nullptr;
    auto iterator = m_bytecodeIntrinsicMap.find(ident.impl());
    if (iterator == m_bytecodeIntrinsicMap.end())
        return nullptr;
    return iterator->value;
}

} // namespace JSC

