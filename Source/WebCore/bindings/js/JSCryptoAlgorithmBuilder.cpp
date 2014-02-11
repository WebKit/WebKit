/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSCryptoAlgorithmBuilder.h"

#if ENABLE(SUBTLE_CRYPTO)

#include <runtime/JSCInlines.h>
#include <runtime/ObjectConstructor.h>
#include <runtime/TypedArrays.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/VMEntryScope.h>

using namespace JSC;

namespace WebCore {

JSCryptoAlgorithmBuilder::JSCryptoAlgorithmBuilder(ExecState* exec)
    : m_exec(exec)
    , m_dictionary(constructEmptyObject(exec))
{
}

JSCryptoAlgorithmBuilder::~JSCryptoAlgorithmBuilder()
{
}

std::unique_ptr<CryptoAlgorithmDescriptionBuilder> JSCryptoAlgorithmBuilder::createEmptyClone() const
{
    return std::make_unique<JSCryptoAlgorithmBuilder>(m_exec);
}

void JSCryptoAlgorithmBuilder::add(const char* key, unsigned value)
{
    VM& vm = m_exec->vm();
    Identifier identifier(&vm, key);
    m_dictionary->putDirect(vm, identifier, jsNumber(value));
}

void JSCryptoAlgorithmBuilder::add(const char* key, const String& value)
{
    VM& vm = m_exec->vm();
    Identifier identifier(&vm, key);
    m_dictionary->putDirect(vm, identifier, jsString(m_exec, value));
}

void JSCryptoAlgorithmBuilder::add(const char* key, const Vector<uint8_t>& buffer)
{
    VM& vm = m_exec->vm();
    Identifier identifier(&vm, key);
    RefPtr<Uint8Array> arrayView = Uint8Array::create(buffer.data(), buffer.size());
    m_dictionary->putDirect(vm, identifier, arrayView->wrap(m_exec, vm.entryScope->globalObject()));
}

void JSCryptoAlgorithmBuilder::add(const char* key, const CryptoAlgorithmDescriptionBuilder& nestedBuilder)
{
    VM& vm = m_exec->vm();
    Identifier identifier(&vm, key);
    const JSCryptoAlgorithmBuilder& jsBuilder = static_cast<const JSCryptoAlgorithmBuilder&>(nestedBuilder);
    m_dictionary->putDirect(vm, identifier, jsBuilder.result());
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
