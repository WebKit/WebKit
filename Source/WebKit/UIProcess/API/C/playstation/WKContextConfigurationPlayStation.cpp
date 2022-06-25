/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "WKContextConfigurationPlayStation.h"

#include "APIProcessPoolConfiguration.h"
#include "WKAPICast.h"

void WKContextConfigurationSetWebProcessPath(WKContextConfigurationRef configuration, WKStringRef webProcessPath)
{
    WebKit::toImpl(configuration)->setWebProcessPath(WebKit::toImpl(webProcessPath)->string());
}

WKStringRef WKContextConfigurationCopyWebProcessPath(WKContextConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->webProcessPath());
}

void WKContextConfigurationSetNetworkProcessPath(WKContextConfigurationRef configuration, WKStringRef networkProcessPath)
{
    WebKit::toImpl(configuration)->setNetworkProcessPath(WebKit::toImpl(networkProcessPath)->string());
}

WKStringRef WKContextConfigurationCopyNetworkProcessPath(WKContextConfigurationRef configuration)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(configuration)->networkProcessPath());
}

void WKContextConfigurationSetUserId(WKContextConfigurationRef configuration, int32_t userId)
{
    WebKit::toImpl(configuration)->setUserId(userId);
}

int32_t WKContextConfigurationGetUserId(WKContextConfigurationRef configuration)
{
    return WebKit::toImpl(configuration)->userId();
}
