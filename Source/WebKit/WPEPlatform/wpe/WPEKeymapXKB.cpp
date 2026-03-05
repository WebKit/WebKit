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
#include "WPEKeymapXKB.h"

#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <xkbcommon/xkbcommon.h>

/**
 * WPEKeymapXKB:
 *
 */
struct _WPEKeymapXKBPrivate {
    struct xkb_keymap* xkbKeymap;
    struct xkb_state* xkbState;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEKeymapXKB, wpe_keymap_xkb, WPE_TYPE_KEYMAP, WPEKeymap)

static void wpeKeymapXKBDispose(GObject* object)
{
    auto* priv = WPE_KEYMAP_XKB(object)->priv;

    g_clear_pointer(&priv->xkbKeymap, xkb_keymap_unref);
    g_clear_pointer(&priv->xkbState, xkb_state_unref);

    G_OBJECT_CLASS(wpe_keymap_xkb_parent_class)->dispose(object);
}

static gboolean wpeKeymapXKBGetEntriesForKeyval(WPEKeymap* keymap, guint keyval, WPEKeymapEntry** entries, guint* entriesCount)
{
    GRefPtr<GArray> array = adoptGRef(g_array_new(FALSE, FALSE, sizeof(WPEKeymapEntry)));

    auto* priv = WPE_KEYMAP_XKB(keymap)->priv;
    auto minKeycode = xkb_keymap_min_keycode(priv->xkbKeymap);
    auto maxKeycode = xkb_keymap_max_keycode(priv->xkbKeymap);
    for (auto keycode = minKeycode; keycode < maxKeycode; ++keycode) {
        int numLayouts = xkb_keymap_num_layouts_for_key(priv->xkbKeymap, keycode);;
        for (int layout = 0; layout < numLayouts; ++layout) {
            int numLevels = xkb_keymap_num_levels_for_key(priv->xkbKeymap, keycode, layout);
            for (int level = 0; level < numLevels; ++level) {
                const xkb_keysym_t* syms;
                int numSyms = xkb_keymap_key_get_syms_by_level(priv->xkbKeymap, keycode, layout, level, &syms);
                for (int sym = 0; sym < numSyms; ++sym) {
                    if (syms[sym] != keyval)
                        continue;

                    WPEKeymapEntry entry = { keycode, layout, level };
                    g_array_append_val(array.get(), entry);
                }
            }
        }
    }

    if (array->len) {
        *entriesCount = array->len;
        *entries = reinterpret_cast<WPEKeymapEntry*>(g_array_free(array.leakRef(), FALSE));
        return TRUE;
    }

    return FALSE;
}

static xkb_mod_mask_t xkbModifiersFromWPEModifiers(struct xkb_keymap* xkbKeymap, WPEModifiers modifiers)
{
    xkb_mod_mask_t mask = 0;
    if (modifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
        mask |= 1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_CTRL);
    if (modifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
        mask |= 1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_SHIFT);
    if (modifiers & WPE_MODIFIER_KEYBOARD_ALT)
        mask |= 1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_ALT);
    if (modifiers & WPE_MODIFIER_KEYBOARD_META)
        mask |= 1 << xkb_keymap_mod_get_index(xkbKeymap, "Meta");
    if (modifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK)
        mask |= 1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_CAPS);
    return mask;
}

static WPEModifiers wpeModifiersFromXKBModifiers(struct xkb_keymap* xkbKeymap, xkb_state* xkbState, xkb_mod_mask_t mask)
{
    unsigned modifiers = 0;
    if (mask & (1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_CTRL)))
        modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (mask & (1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_SHIFT)))
        modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (mask & (1 << xkb_keymap_mod_get_index(xkbKeymap, XKB_MOD_NAME_ALT)))
        modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
    if (mask & (1 << xkb_keymap_mod_get_index(xkbKeymap, "Meta")))
        modifiers |= WPE_MODIFIER_KEYBOARD_META;
    if (xkb_state_led_name_is_active(xkbState, XKB_LED_NAME_CAPS))
        modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
    return static_cast<WPEModifiers>(modifiers);
}

static gboolean wpeKeymapXKBTranslateKeyboardState(WPEKeymap* keymap, guint keycode, WPEModifiers modifiers, int group, guint* keyval, int* effectiveGroup, int* level, WPEModifiers* consumedModifiers)
{
    g_return_val_if_fail(group < 4, FALSE);

    auto* priv = WPE_KEYMAP_XKB(keymap)->priv;
    auto* xkbState = xkb_state_new(priv->xkbKeymap);
    xkb_mod_mask_t mask = xkbModifiersFromWPEModifiers(priv->xkbKeymap, modifiers);
    xkb_state_update_mask(xkbState, mask, 0, 0, group, 0, 0);

    auto keysym = xkb_state_key_get_one_sym(xkbState, keycode);
    if (keyval)
        *keyval = keysym;
    if (effectiveGroup)
        *effectiveGroup = xkb_state_key_get_layout(xkbState, keycode);
    if (level) {
        auto layout = xkb_state_key_get_layout(xkbState, keycode);
        *level = xkb_state_key_get_level(xkbState, keycode, layout);
    }
    if (consumedModifiers) {
        xkb_mod_mask_t consumed = mask & ~xkb_state_mod_mask_remove_consumed(xkbState, keycode, mask);
        *consumedModifiers = wpeModifiersFromXKBModifiers(priv->xkbKeymap, xkbState, consumed);
    }

    xkb_state_unref(xkbState);

    return keysym != XKB_KEY_NoSymbol;
}

static WPEModifiers wpeKeymapXKBGetModifiers(WPEKeymap* keymap)
{
    auto* priv = WPE_KEYMAP_XKB(keymap)->priv;
    xkb_mod_mask_t mask = xkb_state_serialize_mods(priv->xkbState, XKB_STATE_MODS_EFFECTIVE);
    return wpeModifiersFromXKBModifiers(priv->xkbKeymap, priv->xkbState, mask);
}

static void wpe_keymap_xkb_class_init(WPEKeymapXKBClass* keymapXKBClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(keymapXKBClass);
    objectClass->dispose = wpeKeymapXKBDispose;

    WPEKeymapClass* keymapClass = WPE_KEYMAP_CLASS(keymapXKBClass);
    keymapClass->get_entries_for_keyval = wpeKeymapXKBGetEntriesForKeyval;
    keymapClass->translate_keyboard_state = wpeKeymapXKBTranslateKeyboardState;
    keymapClass->get_modifiers = wpeKeymapXKBGetModifiers;
}

namespace WTF {

template<>
struct DefaultRefDerefTraits<struct xkb_context> {
    static struct xkb_context* refIfNotNull(struct xkb_context* ptr)
    {
        if (ptr) [[likely]]
            xkb_context_ref(ptr);
        return ptr;
    }

    static void derefIfNotNull(struct xkb_context* ptr)
    {
        if (ptr) [[likely]]
            xkb_context_unref(ptr);
    }
};

} // namespace WTF

static RefPtr<struct xkb_context> createXKBContext()
{
    auto context = adoptRef(xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES));
    RELEASE_ASSERT(context);

#if !OS(ANDROID)
    if (!xkb_context_include_path_append_default(context.get()))
        LOG_ERROR_ONCE("Could not add default XKB include paths, continuing but input may not work as expected.");
#endif

    return context;
}

/**
 * wpe_keymap_xkb_new: (skip)
 *
 * Create a new #WPEKeymapXKB
 *
 * Returns: (transfer full): a #WPEKeymapXKB
 */
WPEKeymap* wpe_keymap_xkb_new()
{
    auto* keymap = WPE_KEYMAP_XKB(g_object_new(WPE_TYPE_KEYMAP_XKB, nullptr));

    auto context = createXKBContext();
    struct xkb_rule_names names = { "evdev", "pc105", "us", "", "" };
    keymap->priv->xkbKeymap = xkb_keymap_new_from_names(context.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    keymap->priv->xkbState = xkb_state_new(keymap->priv->xkbKeymap);

    return WPE_KEYMAP(keymap);
}

/**
 * wpe_keymap_xkb_update:
 * @keymap: a #WPEKeymapXKB
 * @format: the format
 * @fd: the file descriptor
 * @size: the size of the map
 *
 * Update @keymap from the map at @fd with @format.
 */
void wpe_keymap_xkb_update(WPEKeymapXKB* keymap, guint format, int fd, guint size)
{
    g_return_if_fail(WPE_IS_KEYMAP_XKB(keymap));

    auto* mapping = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        close(fd);
        return;
    }

    auto context = createXKBContext();
    auto* xkbKeymap = xkb_keymap_new_from_string(context.get(), static_cast<char*>(mapping), static_cast<xkb_keymap_format>(format), XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(mapping, size);
    close(fd);
    if (xkbKeymap) {
        auto* priv = keymap->priv;
        g_clear_pointer(&priv->xkbKeymap, xkb_keymap_unref);
        g_clear_pointer(&priv->xkbState, xkb_state_unref);
        priv->xkbKeymap = xkbKeymap;
        keymap->priv->xkbState = xkb_state_new(keymap->priv->xkbKeymap);
    }
}

/**
 * wpe_keymap_xkb_get_xkb_keymap: (skip)
 * @keymap: a #WPEKeymapXKB
 *
 * Get the `xkb_keymap` of @keymap
 *
 * Returns: (transfer none): a `struct xkb_keymap`
 */
struct xkb_keymap* wpe_keymap_xkb_get_xkb_keymap(WPEKeymapXKB* keymap)
{
    g_return_val_if_fail(WPE_IS_KEYMAP_XKB(keymap), nullptr);

    return keymap->priv->xkbKeymap;
}

/**
 * wpe_keymap_xkb_get_xkb_state:
 * @keymap: a #WPEKeymapXKB
 *
 * Get the `xkb_state` of @keymap
 *
 * Returns: (transfer none): a `struct xkb_state`
 */
struct xkb_state* wpe_keymap_xkb_get_xkb_state(WPEKeymapXKB* keymap)
{
    g_return_val_if_fail(WPE_IS_KEYMAP_XKB(keymap), nullptr);

    return keymap->priv->xkbState;
}
