/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextProxyMessages.h"

namespace WebKit {

void WebExtensionContext::firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType type, const PermissionsSet& permissions, const MatchPatternSet& matchPatterns)
{
    ASSERT(type == WebExtensionEventListenerType::PermissionsOnAdded || type == WebExtensionEventListenerType::PermissionsOnRemoved);

    HashSet<String> origins = toStrings(matchPatterns);

    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchPermissionsEvent(type, permissions, origins));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
