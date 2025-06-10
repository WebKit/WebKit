/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DFGDesiredConstantProperties.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "JSImmutableButterfly.h"

namespace JSC { namespace DFG {

DesiredConstantProperties::DesiredConstantProperties() = default;

DesiredConstantProperties::~DesiredConstantProperties() = default;

void DesiredConstantProperties::add(Type type, FrozenValue* base, FrozenValue* value)
{
    m_constantProperties.add({ base->cell(), JSValue::encode(value->value()), static_cast<std::underlying_type_t<Type>>(type) });
}

bool DesiredConstantProperties::reallyAdd(VM&, CommonData*)
{
    for (auto [base, encodedJSValue, type] : m_constantProperties) {
        switch (static_cast<Type>(type)) {
        case Type::ImmutableButterflyLength: {
            auto* immutableButterfly = jsCast<JSImmutableButterfly*>(base);
            JSValue value = JSValue::decode(encodedJSValue);
            if (immutableButterfly->length() != static_cast<uint32_t>(value.asInt32()))
                return false;
            break;
        }
        case Type::None:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
