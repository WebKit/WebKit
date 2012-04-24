/*
 *  Copyright (C) 2012 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "CookieStorage.h"

#include "CookieJarSoup.h"
#include "NotImplemented.h"

#if USE(PLATFORM_STRATEGIES)
#include "CookiesStrategy.h"
#include "PlatformStrategies.h"
#endif

#include <stdio.h>

namespace WebCore {

void setCookieStoragePrivateBrowsingEnabled(bool enabled)
{
    notImplemented();
}

#if USE(PLATFORM_STRATEGIES)
static void soupCookiesChanged(SoupCookieJar*, SoupCookie*, SoupCookie*, gpointer)
{
    platformStrategies()->cookiesStrategy()->notifyCookiesChanged();
}
#endif

void startObservingCookieChanges()
{
#if USE(PLATFORM_STRATEGIES)
    g_signal_connect(soupCookieJar(), "changed", G_CALLBACK(soupCookiesChanged), 0);
#endif
}

void stopObservingCookieChanges()
{
#if USE(PLATFORM_STRATEGIES)
    g_signal_handlers_disconnect_by_func(soupCookieJar(), reinterpret_cast<void*>(soupCookiesChanged), 0);
#endif
}

}
