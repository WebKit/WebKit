 /*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

#ifndef SecurityOriginData_h
#define SecurityOriginData_h

#include <wtf/text/WTFString.h>

namespace WebCore {
class Frame;
class SecurityOrigin;

struct SecurityOriginData {
    WEBCORE_EXPORT static SecurityOriginData fromSecurityOrigin(const SecurityOrigin&);
    WEBCORE_EXPORT static SecurityOriginData fromFrame(Frame*);

    WEBCORE_EXPORT Ref<SecurityOrigin> securityOrigin() const;

    // FIXME <rdar://9018386>: We should be sending more state across the wire than just the protocol,
    // host, and port.

    String protocol;
    String host;
    int port { 0 };

    WEBCORE_EXPORT SecurityOriginData isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, SecurityOriginData&);

#ifndef NDEBUG
    String debugString() const;
#endif
};

WEBCORE_EXPORT bool operator==(const SecurityOriginData&, const SecurityOriginData&);

template<class Encoder>
void SecurityOriginData::encode(Encoder& encoder) const
{
    encoder << protocol;
    encoder << host;
    encoder << port;
}

template<class Decoder>
bool SecurityOriginData::decode(Decoder& decoder, SecurityOriginData& securityOriginData)
{
    if (!decoder.decode(securityOriginData.protocol))
        return false;
    if (!decoder.decode(securityOriginData.host))
        return false;
    if (!decoder.decode(securityOriginData.port))
        return false;

    return true;
}

} // namespace WebCore

#endif // SecurityOriginData_h
