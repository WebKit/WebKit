/*
 * Copyright (C) 2012 University of Washington. All rights reserved.
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
#include "SerializationMethods.h"

#if ENABLE(WEB_REPLAY)

#include "AllReplayInputs.h"
#include "ReplayInputTypes.h"
#include "SecurityOrigin.h"
#include "URL.h"

using WebCore::SecurityOrigin;
using WebCore::URL;
using WebCore::inputTypes;

#define IMPORT_FROM_WEBCORE_NAMESPACE(name) \
using WebCore::name; \

WEB_REPLAY_INPUT_NAMES_FOR_EACH(IMPORT_FROM_WEBCORE_NAMESPACE)
#undef IMPORT_FROM_WEBCORE_NAMESPACE

namespace JSC {

EncodedValue EncodingTraits<NondeterministicInputBase>::encodeValue(const NondeterministicInputBase& input)
{
    EncodedValue encodedValue = EncodedValue::createObject();
    const AtomicString& type = input.type();
    encodedValue.put<String>(ASCIILiteral("type"), type.string());

#define ENCODE_IF_TYPE_TAG_MATCHES(name) \
    if (type == inputTypes().name) { \
        InputTraits<name>::encode(encodedValue, static_cast<const name&>(input)); \
        return encodedValue; \
    } \

    JS_REPLAY_INPUT_NAMES_FOR_EACH(ENCODE_IF_TYPE_TAG_MATCHES)
    WEB_REPLAY_INPUT_NAMES_FOR_EACH(ENCODE_IF_TYPE_TAG_MATCHES)
#undef ENCODE_IF_TYPE_TAG_MATCHES

    // The macro won't work here because of the class template argument.
    if (type == inputTypes().MemoizedDOMResult) {
        InputTraits<MemoizedDOMResultBase>::encode(encodedValue, static_cast<const MemoizedDOMResultBase&>(input));
        return encodedValue;
    }

    ASSERT_NOT_REACHED();
    return EncodedValue();
}

bool EncodingTraits<NondeterministicInputBase>::decodeValue(EncodedValue& encodedValue, std::unique_ptr<NondeterministicInputBase>& input)
{
    String type;
    if (!encodedValue.get<String>(ASCIILiteral("type"), type))
        return false;

#define DECODE_IF_TYPE_TAG_MATCHES(name) \
    if (type == inputTypes().name) { \
        std::unique_ptr<name> decodedInput; \
        if (!InputTraits<name>::decode(encodedValue, decodedInput)) \
            return false; \
        \
        input = std::move(decodedInput); \
        return true; \
    } \

    JS_REPLAY_INPUT_NAMES_FOR_EACH(DECODE_IF_TYPE_TAG_MATCHES)
    WEB_REPLAY_INPUT_NAMES_FOR_EACH(DECODE_IF_TYPE_TAG_MATCHES)
#undef DECODE_IF_TYPE_TAG_MATCHES

    if (type == inputTypes().MemoizedDOMResult) {
        std::unique_ptr<MemoizedDOMResultBase> decodedInput;
        if (!InputTraits<MemoizedDOMResultBase>::decode(encodedValue, decodedInput))
            return false;

        input = std::move(decodedInput);
        return true;
    }

    return false;
}

EncodedValue EncodingTraits<SecurityOrigin>::encodeValue(RefPtr<SecurityOrigin> input)
{
    return EncodedValue::createString(input->toString());
}

bool EncodingTraits<SecurityOrigin>::decodeValue(EncodedValue& encodedValue, RefPtr<SecurityOrigin>& input)
{
    input = SecurityOrigin::createFromString(encodedValue.convertTo<String>());
    return true;
}

EncodedValue EncodingTraits<URL>::encodeValue(const URL& input)
{
    return EncodedValue::createString(input.string());
}

bool EncodingTraits<URL>::decodeValue(EncodedValue& encodedValue, URL& input)
{
    input = URL(WebCore::ParsedURLString, encodedValue.convertTo<String>());
    return true;
}

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)
