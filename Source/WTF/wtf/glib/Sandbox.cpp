/*
 * Copyright (C) 2021 Igalia S.L.
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
#include <wtf/glib/Sandbox.h>

#include <glib.h>

namespace WTF {

bool isInsideFlatpak()
{
    static bool returnValue = g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
    return returnValue;
}

bool isInsideDocker()
{
    static bool returnValue = g_file_test("/.dockerenv", G_FILE_TEST_EXISTS);
    return returnValue;
}

bool isInsideSnap()
{
    // The "SNAP" environment variable is not unlikely to be set for/by something other
    // than Snap, so check a couple of additional variables to avoid false positives.
    // See: https://snapcraft.io/docs/environment-variables
    static bool returnValue = g_getenv("SNAP") && g_getenv("SNAP_NAME") && g_getenv("SNAP_REVISION");
    return returnValue;
}

bool shouldUsePortal()
{
    static bool returnValue = []() -> bool {
        const char* usePortal = isInsideFlatpak() || isInsideSnap() ? "1" : g_getenv("WEBKIT_USE_PORTAL");
        return usePortal && usePortal[0] != '0';
    }();
    return returnValue;
}

String& sandboxedAccessibilityBusAddress()
{
    static String accessibilityBusAddress;
    return accessibilityBusAddress;
}

void setSandboxedAccessibilityBusAddress(String&& address)
{
    sandboxedAccessibilityBusAddress() = address;
}

} // namespace WTF
