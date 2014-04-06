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
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class Frame;
class Page;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class SecurityOrigin;
class URL;

#if USE(APPKIT)
struct KeypressCommand;
#endif

unsigned long frameIndexFromDocument(const Document*);
unsigned long frameIndexFromFrame(const Frame*);
Document* documentFromFrameIndex(Page*, unsigned long frameIndex);
Frame* frameFromFrameIndex(Page*, unsigned long frameIndex);

} // namespace WebCore

// Template specializations must be defined in the same namespace as the template declaration.
namespace JSC {

#if USE(APPKIT)
template<> struct EncodingTraits<WebCore::KeypressCommand> {
    typedef WebCore::KeypressCommand DecodedType;

    static EncodedValue encodeValue(const WebCore::KeypressCommand& value);
    static bool decodeValue(EncodedValue&, WebCore::KeypressCommand& value);
};
#endif // USE(APPKIT)

template<> struct EncodingTraits<NondeterministicInputBase> {
    typedef NondeterministicInputBase DecodedType;

    static EncodedValue encodeValue(const NondeterministicInputBase& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<NondeterministicInputBase>& value);
};

template<> struct EncodingTraits<WebCore::PlatformKeyboardEvent> {
    typedef WebCore::PlatformKeyboardEvent DecodedType;

    static EncodedValue encodeValue(const WebCore::PlatformKeyboardEvent& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<WebCore::PlatformKeyboardEvent>& value);
};

template<> struct EncodingTraits<WebCore::PlatformMouseEvent> {
    typedef WebCore::PlatformMouseEvent DecodedType;

    static EncodedValue encodeValue(const WebCore::PlatformMouseEvent& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<WebCore::PlatformMouseEvent>& value);
};

template<> struct EncodingTraits<WebCore::PlatformWheelEvent> {
    typedef WebCore::PlatformWheelEvent DecodedType;

    static EncodedValue encodeValue(const WebCore::PlatformWheelEvent& value);
    static bool decodeValue(EncodedValue&, std::unique_ptr<WebCore::PlatformWheelEvent>& value);
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
