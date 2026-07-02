/*
 * Copyright (C) 2025 Igalia S.L.
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

#ifndef WPEGamepadManager_h
#define WPEGamepadManager_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEGamepad.h>

G_BEGIN_DECLS

#define WPE_TYPE_GAMEPAD_MANAGER (wpe_gamepad_manager_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEGamepadManager, wpe_gamepad_manager, WPE, GAMEPAD_MANAGER, GObject)

struct _WPEGamepadManagerClass
{
    GObjectClass parent_class;

    gpointer padding[32];
};

WPE_API void         wpe_gamepad_manager_add_device    (WPEGamepadManager *manager,
                                                        WPEGamepad        *gamepad);
WPE_API void         wpe_gamepad_manager_remove_device (WPEGamepadManager *manager,
                                                        WPEGamepad        *gamepad);
WPE_API WPEGamepad **wpe_gamepad_manager_list_devices  (WPEGamepadManager *manager,
                                                        gsize             *n_devices);

G_END_DECLS

#endif /* WPEGamepadManager_h */
