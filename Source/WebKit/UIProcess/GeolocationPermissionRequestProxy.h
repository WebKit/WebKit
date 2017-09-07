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

#pragma once

#include "APIObject.h"
#include <wtf/Function.h>

namespace WebKit {

class GeolocationPermissionRequestManagerProxy;

class GeolocationPermissionRequestProxy : public RefCounted<GeolocationPermissionRequestProxy> {
public:
    static Ref<GeolocationPermissionRequestProxy> create(GeolocationPermissionRequestManagerProxy* manager, uint64_t geolocationID)
    {
        return adoptRef(*new GeolocationPermissionRequestProxy(manager, geolocationID));
    }

    void allow();
    void deny();
    
    void invalidate();

private:
    GeolocationPermissionRequestProxy(GeolocationPermissionRequestManagerProxy*, uint64_t geolocationID);

    GeolocationPermissionRequestManagerProxy* m_manager;
    uint64_t m_geolocationID;
};

class GeolocationPermissionRequest : public API::ObjectImpl<API::Object::Type::GeolocationPermissionRequest> {
public:
    static Ref<GeolocationPermissionRequest> create(Function<void(bool)>&& completionHandler)
    {
        return adoptRef(*new GeolocationPermissionRequest(WTFMove(completionHandler)));
    }
    
    void allow() { m_completionHandler(true); }
    void deny() { m_completionHandler(false); }

private:
    GeolocationPermissionRequest(Function<void(bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    { }
    
    Function<void(bool)> m_completionHandler;
};

} // namespace WebKit
