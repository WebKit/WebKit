/*
 * Copyright (C) 2025 Igalia S.L.
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

#if OS(ANDROID)
#include "Logging.h"

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#include <sys/system_properties.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

String logLevelString()
{
    const char* propertyValue = nullptr;

    if (const auto* propertyInfo = __system_property_find("debug." LOG_CHANNEL_WEBKIT_SUBSYSTEM ".log")) {
        __system_property_read_callback(propertyInfo, [](void *userData, const char*, const char* value, unsigned) {
            auto **propertyValue = static_cast<const char**>(userData);
            *propertyValue = value;
        }, &propertyValue);
    }

    // Disable all log channels if the property is unset or empty.
    if (!propertyValue || !*propertyValue)
        return makeString("-all"_s);

    return String::fromLatin1(propertyValue);
}

} // namespace WTF

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

#endif // OS(ANDROID)
