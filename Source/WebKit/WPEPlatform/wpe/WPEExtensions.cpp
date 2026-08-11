/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEExtensions.h"

#include "WPEDisplay.h"
#include <gio/gio.h>
#include <mutex>
#include <wtf/glib/GUniquePtr.h>

#if ENABLE(WPE_PLATFORM_DRM)
#include "wpe/drm/WPEDisplayDRM.h"
#endif

#if ENABLE(WPE_PLATFORM_HEADLESS)
#include "wpe/headless/WPEDisplayHeadless.h"
#endif

#if ENABLE(WPE_PLATFORM_WAYLAND)
#include "wpe/wayland/WPEDisplayWayland.h"
#endif

void wpeEnsureExtensionPointsRegistered()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto* extensionPoint = g_io_extension_point_register(WPE_DISPLAY_EXTENSION_POINT_NAME);
        g_io_extension_point_set_required_type(extensionPoint, WPE_TYPE_DISPLAY);
    });
}

void wpeEnsureExtensionPointsLoaded()
{
    wpeEnsureExtensionPointsRegistered();

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GIOModuleScope* scope = g_io_module_scope_new(G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);
        const char* path = g_getenv("WPE_PLATFORMS_PATH");
        if (path && *path) {
            GUniquePtr<char*> paths(g_strsplit(path, G_SEARCHPATH_SEPARATOR_S, 0));
            for (size_t i = 0; paths.get()[i]; ++i)
                g_io_modules_scan_all_in_directory_with_scope(paths.get()[i], scope);
        }

        g_io_modules_scan_all_in_directory_with_scope(WPE_PLATFORM_MODULE_DIR, scope);
        g_io_module_scope_free(scope);

        // Initialize types of builtin extensions.
#if ENABLE(WPE_PLATFORM_DRM)
        g_type_ensure(wpe_display_drm_get_type());
#endif
#if ENABLE(WPE_PLATFORM_HEADLESS)
        g_type_ensure(wpe_display_headless_get_type());
#endif
#if ENABLE(WPE_PLATFORM_WAYLAND)
        g_type_ensure(wpe_display_wayland_get_type());
#endif
    });
}
