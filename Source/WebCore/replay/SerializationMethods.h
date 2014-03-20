/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

#ifndef SerializationMethods_h
#define SerializationMethods_h

#if ENABLE(WEB_REPLAY)

#include <replay/EncodedValue.h>
#include <replay/NondeterministicInput.h>

namespace WebCore {
class Document;
class Frame;
class Page;
class PlatformMouseEvent;
class SecurityOrigin;
class URL;
} // namespace WebCore

// Template specializations must be defined in the same namespace as the template declaration.
namespace JSC {

template<> struct EncodingTraits<NondeterministicInputBase> {
    typedef NondeterministicInputBase DecodedType;

    static EncodedValue encodeValue(const NondeterministicInputBase& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<NondeterministicInputBase>& value);
};

template<> struct EncodingTraits<WebCore::PlatformMouseEvent> {
    typedef WebCore::PlatformMouseEvent DecodedType;

    static EncodedValue encodeValue(const WebCore::PlatformMouseEvent& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<WebCore::PlatformMouseEvent>& value);
};

template<> struct EncodingTraits<WebCore::URL> {
    typedef WebCore::URL DecodedType;

    static EncodedValue encodeValue(const WebCore::URL& value);
    static bool decodeValue(EncodedValue&, WebCore::URL& value);
};

template<> struct EncodingTraits<WebCore::SecurityOrigin> {
    typedef RefPtr<WebCore::SecurityOrigin> DecodedType;

    static EncodedValue encodeValue(RefPtr<WebCore::SecurityOrigin> value);
    static bool decodeValue(EncodedValue&, RefPtr<WebCore::SecurityOrigin>& value);
};

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)

#endif // SerializationMethods_h
