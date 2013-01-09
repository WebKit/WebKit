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

#ifndef WebDeviceOrientationClientMock_h
#define WebDeviceOrientationClientMock_h

#include "WebDeviceOrientationClient.h"
#include <public/WebCommon.h>
#include <public/WebPrivateOwnPtr.h>

namespace WebCore { class DeviceOrientationClientMock; }

namespace WebKit {

class WebDeviceOrientationClientMock : public WebDeviceOrientationClient {
public:
    WEBKIT_EXPORT static WebDeviceOrientationClientMock* create();
    ~WebDeviceOrientationClientMock() { reset(); }

    virtual void setController(WebDeviceOrientationController*) OVERRIDE;
    virtual void startUpdating() OVERRIDE;
    virtual void stopUpdating() OVERRIDE;
    virtual WebDeviceOrientation lastOrientation() const OVERRIDE;

    WEBKIT_EXPORT void setOrientation(WebDeviceOrientation&);

private:
    WebDeviceOrientationClientMock() { initialize(); }
    WEBKIT_EXPORT void initialize();
    WEBKIT_EXPORT void reset();

    WebPrivateOwnPtr<WebCore::DeviceOrientationClientMock> m_clientMock;
};

} // namespace WebKit

#endif // WebDeviceOrientationClientMock_h
