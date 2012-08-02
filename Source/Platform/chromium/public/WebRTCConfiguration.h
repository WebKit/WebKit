/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebRTCConfiguration_h
#define WebRTCConfiguration_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebVector.h"

namespace WebCore {
class RTCIceServer;
class RTCConfiguration;
}

namespace WebKit {
class WebString;
class WebURL;

class WebRTCICEServer {
public:
    WebRTCICEServer() { }
    WebRTCICEServer(const WebRTCICEServer& other) { assign(other); }
    ~WebRTCICEServer() { reset(); }

    WebRTCICEServer& operator=(const WebRTCICEServer& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebRTCICEServer&);

    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebURL uri() const;
    WEBKIT_EXPORT WebString credential() const;

#if WEBKIT_IMPLEMENTATION
    WebRTCICEServer(const WTF::PassRefPtr<WebCore::RTCIceServer>&);
#endif

private:
    WebPrivatePtr<WebCore::RTCIceServer> m_private;
};

class WebRTCConfiguration {
public:
    WebRTCConfiguration() { }
    WebRTCConfiguration(const WebRTCConfiguration& other) { assign(other); }
    ~WebRTCConfiguration() { reset(); }

    WebRTCConfiguration& operator=(const WebRTCConfiguration& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebRTCConfiguration&);

    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT size_t numberOfServers() const;
    WEBKIT_EXPORT WebRTCICEServer server(size_t index) const;

#if WEBKIT_IMPLEMENTATION
    WebRTCConfiguration(const WTF::PassRefPtr<WebCore::RTCConfiguration>&);
#endif

private:
    WebPrivatePtr<WebCore::RTCConfiguration> m_private;
};

} // namespace WebKit

#endif // WebRTCConfiguration_h
