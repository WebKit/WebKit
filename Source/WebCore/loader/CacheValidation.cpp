/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2011, 2014 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CacheValidation.h"

#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

double computeCurrentAge(const ResourceResponse& response, double responseTimestamp)
{
    // RFC2616 13.2.3
    // No compensation for latency as that is not terribly important in practice
    double dateValue = response.date();
    double apparentAge = std::isfinite(dateValue) ? std::max(0., responseTimestamp - dateValue) : 0;
    double ageValue = response.age();
    double correctedReceivedAge = std::isfinite(ageValue) ? std::max(apparentAge, ageValue) : apparentAge;
    double residentTime = currentTime() - responseTimestamp;
    return correctedReceivedAge + residentTime;
}

double computeFreshnessLifetimeForHTTPFamily(const ResourceResponse& response, double responseTimestamp)
{
    ASSERT(response.url().protocolIsInHTTPFamily());
    // RFC2616 13.2.4
    double maxAgeValue = response.cacheControlMaxAge();
    if (std::isfinite(maxAgeValue))
        return maxAgeValue;
    double expiresValue = response.expires();
    double dateValue = response.date();
    double creationTime = std::isfinite(dateValue) ? dateValue : responseTimestamp;
    if (std::isfinite(expiresValue))
        return expiresValue - creationTime;
    double lastModifiedValue = response.lastModified();
    if (std::isfinite(lastModifiedValue))
        return (creationTime - lastModifiedValue) * 0.1;
    // If no cache headers are present, the specification leaves the decision to the UA. Other browsers seem to opt for 0.
    return 0;
}

void updateRedirectChainStatus(RedirectChainCacheStatus& redirectChainCacheStatus, const ResourceResponse& response)
{
    if (redirectChainCacheStatus.status == RedirectChainCacheStatus::NotCachedRedirection)
        return;
    if (response.cacheControlContainsNoStore() || response.cacheControlContainsNoCache() || response.cacheControlContainsMustRevalidate()) {
        redirectChainCacheStatus.status = RedirectChainCacheStatus::NotCachedRedirection;
        return;
    }
    redirectChainCacheStatus.status = RedirectChainCacheStatus::CachedRedirection;
    double responseTimestamp = currentTime();
    // Store the nearest end of cache validity date
    double endOfValidity = responseTimestamp + computeFreshnessLifetimeForHTTPFamily(response, responseTimestamp) - computeCurrentAge(response, responseTimestamp);
    redirectChainCacheStatus.endOfValidity = std::min(redirectChainCacheStatus.endOfValidity, endOfValidity);
}

bool redirectChainAllowsReuse(RedirectChainCacheStatus redirectChainCacheStatus)
{
    switch (redirectChainCacheStatus.status) {
    case RedirectChainCacheStatus::NoRedirection:
        return true;
    case RedirectChainCacheStatus::NotCachedRedirection:
        return false;
    case RedirectChainCacheStatus::CachedRedirection:
        return currentTime() <= redirectChainCacheStatus.endOfValidity;
    }
    ASSERT_NOT_REACHED();
    return false;
}

}
