/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGeolocationController_h
#define WebGeolocationController_h

#include <public/WebCommon.h>
#include <public/WebNonCopyable.h>

namespace WebCore { class GeolocationController; }

namespace WebKit {

class WebGeolocationPosition;
class WebGeolocationError;

// Note that the WebGeolocationController is invalid after the
// WebGeolocationClient::geolocationDestroyed() has been received.
class WebGeolocationController : public WebNonCopyable {
public:
    WEBKIT_EXPORT void positionChanged(const WebGeolocationPosition&);
    WEBKIT_EXPORT void errorOccurred(const WebGeolocationError&);

#if WEBKIT_IMPLEMENTATION
    WebGeolocationController(WebCore::GeolocationController* c)
        : m_private(c)
    {
    }

    WebCore::GeolocationController* controller() const { return m_private; }
#endif

private:
    // No implementation for the default constructor. Declared private to ensure that no instances
    // can be created by the consumers of Chromium WebKit.
    WebGeolocationController();

    WebCore::GeolocationController* m_private;
};

} // namespace WebKit

#endif // WebGeolocationController_h
