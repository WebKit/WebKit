/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ResourceRequestCFNet.h"

#include "HTTPHeaderNames.h"
#include "PublicSuffixStore.h"
#include "RegistrableDomain.h"
#include "ResourceLoadPriority.h"
#include "ResourceRequest.h"
#include <dlfcn.h>
#include <pal/spi/cf/CFNetworkSPI.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/cf/TypeCastsCF.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResourceRequest);

// FIXME: Make this a NetworkingContext property.
#if PLATFORM(IOS_FAMILY)
bool ResourceRequest::s_httpPipeliningEnabled = true;
#else
bool ResourceRequest::s_httpPipeliningEnabled = false;
#endif

void ResourceRequest::updateFromDelegatePreservingOldProperties(const ResourceRequest& delegateProvidedRequest)
{
    // These are things we don't want willSendRequest delegate to mutate or reset.
    ResourceLoadPriority oldPriority = priority();
    RefPtr<FormData> oldHTTPBody = httpBody();
    bool isHiddenFromInspector = hiddenFromInspector();
    auto oldRequester = requester();
    auto oldInitiatorIdentifier = initiatorIdentifier();
    auto oldInspectorInitiatorNodeIdentifier = inspectorInitiatorNodeIdentifier();
    auto oldAppInitiatedValue = isAppInitiated();
    auto oldPrivacyProxyFailClosedForUnreachableNonMainHosts = privacyProxyFailClosedForUnreachableNonMainHosts();
    auto oldUseAdvancedPrivacyProtections = useAdvancedPrivacyProtections();

    *this = delegateProvidedRequest;

    setPriority(oldPriority);
    setHTTPBody(WTFMove(oldHTTPBody));
    setHiddenFromInspector(isHiddenFromInspector);
    setRequester(oldRequester);
    setInitiatorIdentifier(oldInitiatorIdentifier);
    if (oldInspectorInitiatorNodeIdentifier)
        setInspectorInitiatorNodeIdentifier(*oldInspectorInitiatorNodeIdentifier);
    setIsAppInitiated(oldAppInitiatedValue);
    setPrivacyProxyFailClosedForUnreachableNonMainHosts(oldPrivacyProxyFailClosedForUnreachableNonMainHosts);
    setUseAdvancedPrivacyProtections(oldUseAdvancedPrivacyProtections);
}

bool ResourceRequest::httpPipeliningEnabled()
{
    return s_httpPipeliningEnabled;
}

void ResourceRequest::setHTTPPipeliningEnabled(bool flag)
{
    s_httpPipeliningEnabled = flag;
}

// FIXME: It is confusing that this function both sets connection count and determines maximum request count at network layer. This can and should be done separately.
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    static const unsigned preferredConnectionCount = 6;
    static const unsigned unlimitedRequestCount = 10000;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);
    unsigned maximumHTTPConnectionCountPerHost = _CFNetworkHTTPConnectionCacheGetLimit(kHTTPLoadWidth);

    Boolean keyExistsAndHasValidFormat = false;
    Boolean prefValue = CFPreferencesGetAppBooleanValue(CFSTR("WebKitEnableHTTPPipelining"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
        ResourceRequest::setHTTPPipeliningEnabled(prefValue);

    // Use WebCore scheduler when we can't use request priorities with CFNetwork.
    if (!ResourceRequest::resourcePrioritiesEnabled())
        return maximumHTTPConnectionCountPerHost;

    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, resourceLoadPriorityCount);
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(ResourceLoadPriority::Medium));

    return unlimitedRequestCount;
}

#if PLATFORM(IOS_FAMILY)
void initializeHTTPConnectionSettingsOnStartup()
{
    // This need to be called from WebKitInitialize so the calls happen early enough, before any requests are made. <rdar://problem/9691871>
    // Desktop doesn't have early initialization so it is not clear how this should be done there. The CFNetwork SPI probably
    // needs to become more forgiving.
    // We can't read settings here as this is called too early for that. All values need to be constants.
    static const unsigned preferredConnectionCount = 6;
    static const unsigned fastLaneConnectionCount = 1;
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPLoadWidth, preferredConnectionCount);
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPPriorityNumLevels, resourceLoadPriorityCount);
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPMinimumFastLanePriority, toPlatformRequestPriority(ResourceLoadPriority::Medium));
    _CFNetworkHTTPConnectionCacheSetLimit(kHTTPNumFastLanes, fastLaneConnectionCount);
}
#endif

CFStringRef ResourceRequest::isUserInitiatedKey()
{
    static CFStringRef key = CFSTR("ResourceRequestIsUserInitiatedKey");
    return key;
}

} // namespace WebCore
