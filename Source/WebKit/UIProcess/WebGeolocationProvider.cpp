/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGeolocationProvider.h"

#include "WKAPICast.h"
#include "WebGeolocationManagerProxy.h"

namespace WebKit {

WebGeolocationProvider::WebGeolocationProvider(const WKGeolocationProviderBase* provider)
{
    initialize(provider);
}

void WebGeolocationProvider::startUpdating(WebGeolocationManagerProxy& geolocationManager)
{
    if (!m_client.startUpdating)
        return;

    m_client.startUpdating(toAPI(&geolocationManager), m_client.base.clientInfo);
}

void WebGeolocationProvider::stopUpdating(WebGeolocationManagerProxy& geolocationManager)
{
    if (!m_client.stopUpdating)
        return;

    m_client.stopUpdating(toAPI(&geolocationManager), m_client.base.clientInfo);
}

void WebGeolocationProvider::setEnableHighAccuracy(WebGeolocationManagerProxy& geolocationManager, bool enabled)
{
    if (!m_client.setEnableHighAccuracy)
        return;

    m_client.setEnableHighAccuracy(toAPI(&geolocationManager), enabled, m_client.base.clientInfo);
}

} // namespace WebKit
