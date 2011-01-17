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

#include "config.h"
#include "WebGeolocationPermissionRequestManager.h"

#include "WebGeolocationPermissionRequest.h"
#include <wtf/HashMap.h>

namespace WebCore {
class Geolocation;
}

using namespace WebCore;

typedef HashMap<Geolocation*, int> GeolocationIdMap;
typedef HashMap<int, Geolocation*> IdGeolocationMap;

namespace WebKit {
class WebGeolocationPermissionRequestManagerPrivate {
public:
    GeolocationIdMap m_geolocationIdMap;
    IdGeolocationMap m_idGeolocationMap;
};
}

using namespace WebKit;

int WebGeolocationPermissionRequestManager::add(const WebKit::WebGeolocationPermissionRequest& permissionRequest)
{
    Geolocation* geolocation = permissionRequest.geolocation();
    ASSERT(!m_private->m_geolocationIdMap.contains(geolocation));
    int id = ++m_lastId;
    m_private->m_geolocationIdMap.add(geolocation, id);
    m_private->m_idGeolocationMap.add(id, geolocation);
    return id;
}

bool WebGeolocationPermissionRequestManager::remove(const WebKit::WebGeolocationPermissionRequest& permissionRequest, int& id)
{
    Geolocation* geolocation = permissionRequest.geolocation();
    GeolocationIdMap::iterator it = m_private->m_geolocationIdMap.find(geolocation);
    if (it == m_private->m_geolocationIdMap.end())
        return false;
    id = it->second;
    m_private->m_geolocationIdMap.remove(it);
    m_private->m_idGeolocationMap.remove(id);
    return true;
}

bool WebGeolocationPermissionRequestManager::remove(int id, WebKit::WebGeolocationPermissionRequest& permissionRequest)
{
    IdGeolocationMap::iterator it = m_private->m_idGeolocationMap.find(id);
    if (it == m_private->m_idGeolocationMap.end())
        return false;
    Geolocation* geolocation = it->second;
    permissionRequest = WebGeolocationPermissionRequest(geolocation);
    m_private->m_idGeolocationMap.remove(it);
    m_private->m_geolocationIdMap.remove(geolocation);
    return true;
}

void WebGeolocationPermissionRequestManager::init()
{
    m_lastId = 0;
    m_private.reset(new WebGeolocationPermissionRequestManagerPrivate);
}

void WebGeolocationPermissionRequestManager::reset()
{
    m_private.reset(0);
}

