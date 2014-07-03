/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoizedDOMResult.h"

#if ENABLE(WEB_REPLAY)

#include "ReplayInputTypes.h"
#include "SerializationMethods.h"
#include "WebReplayInputs.h"

namespace WebCore {

const AtomicString& MemoizedDOMResultBase::type() const
{
    return inputTypes().MemoizedDOMResult;
}

std::unique_ptr<MemoizedDOMResultBase> MemoizedDOMResultBase::createFromEncodedResult(const String& attribute, EncodedCType ctype, EncodedValue encodedValue, ExceptionCode exceptionCode)
{
    switch (ctype) {
#define CREATE_DECODE_SWITCH_CASE(name, type) \
    case CTypeTraits<type>::encodedType: { \
        CTypeTraits<type>::CType result; \
        if (!EncodingTraits<type>::decodeValue(encodedValue, result)) \
            return nullptr; \
        return std::make_unique<MemoizedDOMResult<type>>(attribute, result, exceptionCode); \
    } \
\

FOR_EACH_MEMOIZED_CTYPE(CREATE_DECODE_SWITCH_CASE)
#undef CREATE_DECODE_SWITCH_CASE
    }

    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore

namespace JSC {

using WebCore::EncodedCType;
using WebCore::ExceptionCode;
using WebCore::MemoizedDOMResult;
using WebCore::SerializedScriptValue;

const AtomicString& InputTraits<MemoizedDOMResultBase>::type()
{
    return WebCore::inputTypes().MemoizedDOMResult;
}

void InputTraits<MemoizedDOMResultBase>::encode(EncodedValue& encodedValue, const MemoizedDOMResultBase& input)
{
    encodedValue.put<String>(ASCIILiteral("attribute"), input.attribute());
    encodedValue.put<EncodedCType>(ASCIILiteral("ctype"), input.ctype());
    encodedValue.put<EncodedValue>(ASCIILiteral("result"), input.encodedResult());
    if (input.exceptionCode())
        encodedValue.put<ExceptionCode>(ASCIILiteral("exceptionCode"), input.exceptionCode());
}

bool InputTraits<MemoizedDOMResultBase>::decode(EncodedValue& encodedValue, std::unique_ptr<MemoizedDOMResultBase>& input)
{
    String attribute;
    if (!encodedValue.get<String>(ASCIILiteral("attribute"), attribute))
        return false;

    EncodedCType ctype;
    if (!encodedValue.get<EncodedCType>(ASCIILiteral("ctype"), ctype))
        return false;

    EncodedValue encodedResult;
    if (!encodedValue.get<EncodedValue>(ASCIILiteral("result"), encodedResult))
        return false;

    ExceptionCode exceptionCode = 0;
    encodedValue.get<ExceptionCode>(ASCIILiteral("exceptionCode"), exceptionCode);

    std::unique_ptr<MemoizedDOMResultBase> decodedInput = MemoizedDOMResultBase::createFromEncodedResult(attribute, ctype, encodedResult, exceptionCode);
    if (!decodedInput)
        return false;
    input = WTF::move(decodedInput);
    return true;
}

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)
