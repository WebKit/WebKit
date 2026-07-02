/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#if ENABLE(WPE_PLATFORM)
#include "WPEUtilities.h"
#include <wpe/wpe-platform.h>
#endif
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

namespace WebKit {

Vector<double> availableScreenScales()
{
    Vector<double> screenScales;

#if ENABLE(WPE_PLATFORM)
    if (WKWPE::isUsingWPEPlatformAPI()) {
        GUniquePtr<GList> toplevels(wpe_toplevel_list());
        for (GList* iter = toplevels.get(); iter; iter = g_list_next(iter)) {
            auto* screen = wpe_toplevel_get_screen(WPE_TOPLEVEL(iter->data));
            screenScales.append(wpe_screen_get_scale(screen));
        }
    }
#endif

    if (!screenScales.isEmpty())
        return screenScales;

    // Assume 1x if we got no results. This can happen on headless devices (bots).
    return { 1.0 };
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
