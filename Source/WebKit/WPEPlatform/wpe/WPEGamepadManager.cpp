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

#include "config.h"
#include "WPEGamepadManager.h"

#include <wtf/ListHashSet.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEGamepadManager:
 *
 */
struct _WPEGamepadManagerPrivate {
    ListHashSet<GRefPtr<WPEGamepad>> gamepads;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEGamepadManager, wpe_gamepad_manager, G_TYPE_OBJECT)

enum {
    DEVICE_ADDED,
    DEVICE_REMOVED,

    LAST_SIGNAL
};

static std::array<unsigned, LAST_SIGNAL> signals;

static void wpe_gamepad_manager_class_init(WPEGamepadManagerClass* gamepadManagerClass)
{
    /**
     * WPEGamepadManager::device-added:
     * @manager: a #WPEGamepadManager
     * @gamepad: the #WPEGamepad added
     *
     * Emitted when a gamepad device is added
     */
    signals[DEVICE_ADDED] = g_signal_new(
        "device-added",
        G_TYPE_FROM_CLASS(gamepadManagerClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_GAMEPAD);

    /**
     * WPEGamepadManager::device-removed:
     * @manager: a #WPEGamepadManager
     * @gamepad: the #WPEGamepad removed
     *
     * Emitted after a gaempad device is removed.
     */
    signals[DEVICE_REMOVED] = g_signal_new(
        "device-removed",
        G_TYPE_FROM_CLASS(gamepadManagerClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_GAMEPAD);
}

/**
 * wpe_gamepad_manager_add_device:
 * @manager: a #WPEGamepadManager
 * @gamepad: a #WPEGamepad
 *
 * Add @gamepad to @manager and emit the signal #WPEGamepadManager::device-added if
 * it wasn't already present.
 */
void wpe_gamepad_manager_add_device(WPEGamepadManager* manager, WPEGamepad* gamepad)
{
    g_return_if_fail(WPE_IS_GAMEPAD_MANAGER(manager));
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));

    auto addResult = manager->priv->gamepads.add(gamepad);
    if (addResult.isNewEntry)
        g_signal_emit(manager, signals[DEVICE_ADDED], 0, gamepad);
}

/**
 * wpe_gamepad_manager_remove_device:
 * @manager: a #WPEGamepadManager
 * @gamepad: a #WPEGamepad
 *
 * Remove @gamepad from @manager and emit the signal #WPEGamepadManager::device-removed if
 * it was present.
 */
void wpe_gamepad_manager_remove_device(WPEGamepadManager* manager, WPEGamepad* gamepad)
{
    g_return_if_fail(WPE_IS_GAMEPAD_MANAGER(manager));
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));

    GRefPtr<WPEGamepad> protectedGamepad = gamepad;
    if (manager->priv->gamepads.remove(gamepad))
        g_signal_emit(manager, signals[DEVICE_REMOVED], 0, gamepad);
}

/**
 * wpe_gamepad_manager_list_devices:
 * @manager: a #WPEGamepadManager
 * @n_devices: (out): return location for the length of the array
 *
 * Get the list of #WPEGamepad devices in @manager
 *
 * Returns: (transfer container) (array length=n_devices) (element-type WPEGamepad): the list of gamepad devices
 */
WPEGamepad** wpe_gamepad_manager_list_devices(WPEGamepadManager* manager, gsize* deviceCount)
{
    g_return_val_if_fail(WPE_IS_GAMEPAD_MANAGER(manager), nullptr);
    g_return_val_if_fail(deviceCount, nullptr);

    auto* priv = manager->priv;
    if (priv->gamepads.isEmpty()) {
        *deviceCount = 0;
        return nullptr;
    }

    GRefPtr<GPtrArray> array = adoptGRef(g_ptr_array_sized_new(priv->gamepads.size()));
    for (const auto& gamepad : priv->gamepads)
        g_ptr_array_add(array.get(), gamepad.get());

    return reinterpret_cast<WPEGamepad**>(g_ptr_array_steal(array.get(), deviceCount));
}
