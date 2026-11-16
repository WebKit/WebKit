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

#ifndef WPEKeymapXKB_h
#define WPEKeymapXKB_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEKeymap.h>
#include <xkbcommon/xkbcommon.h>

G_BEGIN_DECLS

#define WPE_TYPE_KEYMAP_XKB (wpe_keymap_xkb_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEKeymapXKB, wpe_keymap_xkb, WPE, KEYMAP_XKB, WPEKeymap)

WPE_API WPEKeymap         *wpe_keymap_xkb_new            (void);
WPE_API void               wpe_keymap_xkb_update         (WPEKeymapXKB *keymap,
                                                          guint         format,
                                                          int           fd,
                                                          guint         size);
WPE_API struct xkb_keymap *wpe_keymap_xkb_get_xkb_keymap (WPEKeymapXKB *keymap);
WPE_API struct xkb_state  *wpe_keymap_xkb_get_xkb_state  (WPEKeymapXKB *keymap);

G_END_DECLS

#endif /* WPEKeymapXKB_h */
