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

#if USE(SOUP)

#include "NetworkStorageSession.h"
#include <libsoup/soup.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<SoupCookieJar*, std::function<void ()>>& cookieChangeCallbackMap()
{
    static NeverDestroyed<HashMap<SoupCookieJar*, std::function<void ()>>> map;
    return map;
}

static void soupCookiesChanged(SoupCookieJar* jar)
{
    if (auto callback = cookieChangeCallbackMap().get(jar))
        callback();
}

void startObservingCookieChanges(const NetworkStorageSession& storageSession, std::function<void ()>&& callback)
{
    auto* jar = storageSession.cookieStorage();
    ASSERT(!cookieChangeCallbackMap().contains(jar));
    cookieChangeCallbackMap().add(jar, WTFMove(callback));
    g_signal_connect(jar, "changed", G_CALLBACK(soupCookiesChanged), nullptr);
}

void stopObservingCookieChanges(const NetworkStorageSession& storageSession)
{
    auto* jar = storageSession.cookieStorage();
    ASSERT(cookieChangeCallbackMap().contains(jar));
    cookieChangeCallbackMap().remove(jar);
    g_signal_handlers_disconnect_by_func(jar, reinterpret_cast<void*>(soupCookiesChanged), nullptr);
}

}

#endif
