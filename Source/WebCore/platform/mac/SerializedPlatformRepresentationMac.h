/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef SerializedPlatformRepresentationMac_h
#define SerializedPlatformRepresentationMac_h

#if ENABLE(VIDEO_TRACK) && ENABLE(DATACUE_VALUE)

#include "SerializedPlatformRepresentation.h"

#if USE(FOUNDATION) && !defined(__OBJC__)
typedef struct objc_object *id;
#endif

namespace WebCore {

class SerializedPlatformRepresentationMac : public SerializedPlatformRepresentation {
public:
    virtual ~SerializedPlatformRepresentationMac();
    static Ref<SerializedPlatformRepresentation> create(id);

    JSC::JSValue deserialize(JSC::ExecState*) const override;
    RefPtr<ArrayBuffer> data() const override;

    bool isEqual(const SerializedPlatformRepresentation&) const override;

    PlatformType platformType() const override { return SerializedPlatformRepresentation::ObjC; }

    id nativeValue() const { return m_nativeValue.get(); }

private:
    SerializedPlatformRepresentationMac(id nativeValue);

    RetainPtr<id> m_nativeValue;
};

SerializedPlatformRepresentationMac* toSerializedPlatformRepresentationMac(SerializedPlatformRepresentation*);
const SerializedPlatformRepresentationMac* toSerializedPlatformRepresentationMac(const SerializedPlatformRepresentation*);

} // namespace WebCore

#endif
#endif // SerializedPlatformRepresentationMac_h
