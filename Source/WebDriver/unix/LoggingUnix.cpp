/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
#include "LogInitialization.h"

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#include <wtf/LogInitialization.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

String logLevelString()
{
#if PLATFORM(COCOA)
    // WTF reads the channels from the WTFLogging default on Cocoa. Take them from WEBKIT_DEBUG,
    // like on the other ports, which is what the usage statement documents.
    if (auto* channels = getenv("WEBKIT_DEBUG"))
        return String::fromUTF8(channels);
#endif
    return WTF::logLevelString();
}

} // namespace WebDriver

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED
