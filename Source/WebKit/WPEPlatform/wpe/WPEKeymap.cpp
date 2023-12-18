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
#include "WPEKeymap.h"

#include <wtf/glib/WTFGType.h>

/**
 * WPEKeymap:
 *
 */
struct _WPEKeymapPrivate {
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEKeymap, wpe_keymap, G_TYPE_OBJECT)

static void wpe_keymap_class_init(WPEKeymapClass*)
{
}

/**
 * wpe_keymap_get_entries_for_keycode:
 * @keymap: a #WPEKaymap
 * @keyval: a keyval
 * @entries: (out): return location for array of #WPEKeymapEntry
 * @n_entries: (out): return location for length of @entries
 *
 * Get the @keymap list of keycode/group/level combinations that will generate @keyval
 *
 * Returns: %TRUE if there were entries, or %FALSE otherwise
 */
gboolean wpe_keymap_get_entries_for_keyval(WPEKeymap* keymap, guint keyval, WPEKeymapEntry** entries, guint *entriesCount)
{
    g_return_val_if_fail(WPE_IS_KEYMAP(keymap), FALSE);
    g_return_val_if_fail(entries, FALSE);
    g_return_val_if_fail(entriesCount, FALSE);

    return WPE_KEYMAP_GET_CLASS(keymap)->get_entries_for_keyval(keymap, keyval, entries, entriesCount);
}

/**
 * wpe_keymap_get_modifiers:
 * @keymap: a #WPEKaymap
 *
 * Get the modifiers state of @keymap
 *
 * Returns: a #WPEModifiers
 */
WPEModifiers wpe_keymap_get_modifiers(WPEKeymap* keymap)
{
    g_return_val_if_fail(WPE_IS_KEYMAP(keymap), static_cast<WPEModifiers>(0));

    return WPE_KEYMAP_GET_CLASS(keymap)->get_modifiers(keymap);
}
