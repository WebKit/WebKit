/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "Application.h"

#if USE(GLIB)
#include <glib.h>
#include <mutex>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

const CString& applicationID()
{
    static NeverDestroyed<CString> id;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        if (auto* app = g_application_get_default()) {
            if (const char* appID = g_application_get_application_id(app)) {
                id.get() = appID;
                return;
            }
        }

        const char* programName = g_get_prgname();
        if (programName && g_application_id_is_valid(programName)) {
            id.get() = programName;
            return;
        }

        // There must be some id for xdg-desktop-portal to function.
        // xdg-desktop-portal uses this id for permissions.
        // This creates a somewhat reliable id based on the executable path
        // which will avoid potentially gaining permissions from another app
        // and won't flood xdg-desktop-portal with new ids.
        if (auto executablePath = FileSystem::currentExecutablePath(); !executablePath.isNull()) {
            GUniquePtr<char> digest(g_compute_checksum_for_data(G_CHECKSUM_SHA256, reinterpret_cast<const uint8_t*>(executablePath.data()), executablePath.length()));
            id.get() = makeString("org.webkit.app-"_s, span(digest.get())).utf8();
            return;
        }

        // If it is not possible to obtain the executable path, generate a random identifier as a fallback.
        auto uuid = WTF::UUID::createVersion4Weak();
        id.get() = makeString("org.webkit.app-"_s, uuid.toString()).utf8();
    });
    return id.get();
}

} // namespace WTF

#endif // USE(GLIB)
