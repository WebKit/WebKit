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

#ifndef JSCryptoAlgorithmBuilder_h
#define JSCryptoAlgorithmBuilder_h

#include "CryptoAlgorithmDescriptionBuilder.h"

#if ENABLE(SUBTLE_CRYPTO)

namespace JSC {
class ExecState;
class JSObject;
}

namespace WebCore {

class JSCryptoAlgorithmBuilder final : public CryptoAlgorithmDescriptionBuilder {
public:
    JSCryptoAlgorithmBuilder(JSC::ExecState*);
    virtual ~JSCryptoAlgorithmBuilder();

    JSC::JSObject* result() const { return m_dictionary; }

    virtual std::unique_ptr<CryptoAlgorithmDescriptionBuilder> createEmptyClone() const override;

    virtual void add(const char*, unsigned) override;
    virtual void add(const char*, const String&) override;
    virtual void add(const char*, const Vector<uint8_t>&) override;
    virtual void add(const char*, const CryptoAlgorithmDescriptionBuilder&) override;

private:
    JSC::ExecState* m_exec;
    JSC::JSObject* m_dictionary;
};

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
#endif // JSCryptoAlgorithmBuilder_h
