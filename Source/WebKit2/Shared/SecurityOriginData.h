/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "GenericCallback.h" // FIXME: This is a UIProcess file, and may not be included from Shared directory files.
#include <wtf/text/WTFString.h>

namespace IPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

namespace WebKit {

typedef GenericCallback<API::Array*> ArrayCallback;

struct SecurityOriginData {
    static SecurityOriginData fromSecurityOrigin(const WebCore::SecurityOrigin*);
    PassRefPtr<WebCore::SecurityOrigin> securityOrigin() const;

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, SecurityOriginData&);

    // FIXME <rdar://9018386>: We should be sending more state across the wire than just the protocol,
    // host, and port.

    String protocol;
    String host;
    int port;

    SecurityOriginData isolatedCopy() const;
};

void performAPICallbackWithSecurityOriginDataVector(const Vector<SecurityOriginData>&, ArrayCallback*);

bool operator==(const SecurityOriginData&, const SecurityOriginData&);

} // namespace WebKit

#endif // SecurityOriginData_h
