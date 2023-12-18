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

#ifndef WPEKeymap_h
#define WPEKeymap_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEEvent.h>

G_BEGIN_DECLS

#define WPE_TYPE_KEYMAP (wpe_keymap_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEKeymap, wpe_keymap, WPE, KEYMAP, GObject)

typedef struct _WPEKeymapEntry WPEKeymapEntry;

/**
 * WPEKeymapEntry:
 * @keycode: the hardware keycode. This is an identifying number for a physical key.
 * @group: indicates movement in a horizontal direction. Usually groups are used
 *   for two different languages. In group 0, a key might have two English
 *   characters, and in group 1 it might have two Hebrew characters. The Hebrew
 *   characters will be printed on the key next to the English characters.
 * @level: indicates which symbol on the key will be used, in a vertical direction.
 *   So on a standard US keyboard, the key with the number “1” on it also has the
 *   exclamation point ("!") character on it. The level indicates whether to use
 *   the “1” or the “!” symbol. The letter keys are considered to have a lowercase
 *   letter at level 0, and an uppercase letter at level 1, though only the
 *   uppercase letter is printed.
 *
 * A WPEKeymapEntry is a map entry retrurned by wpe_keymap_get_entries_for_keyval().
 */
struct _WPEKeymapEntry
{
  guint keycode;
  int   group;
  int   level;
};

struct _WPEKeymapClass
{
    GObjectClass parent_class;

    gboolean     (* get_entries_for_keyval) (WPEKeymap       *keymap,
                                             guint            keyval,
                                             WPEKeymapEntry **entries,
                                             guint           *n_entries);
    WPEModifiers (* get_modifiers)          (WPEKeymap       *keymap);

    gpointer padding[32];
};

WPE_API gboolean     wpe_keymap_get_entries_for_keyval (WPEKeymap       *keymap,
                                                        guint            keyval,
                                                        WPEKeymapEntry **entries,
                                                        guint           *n_entries);
WPE_API WPEModifiers wpe_keymap_get_modifiers          (WPEKeymap       *keymap);

G_END_DECLS

#endif /* WPEKeymap_h */
