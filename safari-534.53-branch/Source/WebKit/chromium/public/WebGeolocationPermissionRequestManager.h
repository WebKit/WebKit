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

#ifndef WebGeolocationPermissionRequestManager_h
#define WebGeolocationPermissionRequestManager_h

#include "WebNonCopyable.h"
#include "WebPrivateOwnPtr.h"

namespace WebKit {

class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManagerPrivate;

// This class is used to map between integer identifiers and WebGeolocationPermissionRequest
// instances. The intended usage is that on WebGeolocationClient::requestPermission(),
// the implementer can call add() to associate an id with the WebGeolocationPermissionRequest object.
// Once the permission request has been decided, the second remove() method can be used to
// find the request. On WebGeolocationClient::cancelPermissionRequest, the first remove() method will
// remove the association with the id.
class WebGeolocationPermissionRequestManager : public WebNonCopyable {
public:
    WebGeolocationPermissionRequestManager() { init(); }
    ~WebGeolocationPermissionRequestManager() { reset(); }

    WEBKIT_API int add(const WebKit::WebGeolocationPermissionRequest&);
    WEBKIT_API bool remove(const WebKit::WebGeolocationPermissionRequest&, int&);
    WEBKIT_API bool remove(int, WebKit::WebGeolocationPermissionRequest&);

private:
    WEBKIT_API void init();
    WEBKIT_API void reset();

    WebPrivateOwnPtr<WebGeolocationPermissionRequestManagerPrivate> m_private;
    int m_lastId;
};

}

#endif // WebGeolocationPermissionRequestManager_h

