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
#include "WPEGamepadManagerManette.h"

#include "WPEGamepadManette.h"
#include <libmanette.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEGamepadManagerManettePrivate {
    GRefPtr<ManetteMonitor> monitor;
    HashMap<ManetteDevice*, GRefPtr<WPEGamepad>> devices;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEGamepadManagerManette, wpe_gamepad_manager_manette, WPE_TYPE_GAMEPAD_MANAGER, WPEGamepadManager)

static void wpeGamepadManagerManetteAddDevice(WPEGamepadManagerManette* manager, ManetteDevice* device)
{
    GRefPtr<WPEGamepad> gamepad = adoptGRef(wpeGamepadManetteCreate(device));
    auto addResult = manager->priv->devices.set(device, WTF::move(gamepad));
    wpe_gamepad_manager_add_device(WPE_GAMEPAD_MANAGER(manager), addResult.iterator->value.get());
}

static void wpeGamepadManagerManetteRemoveDevice(WPEGamepadManagerManette* manager, ManetteDevice* device)
{
    if (auto gamepad = manager->priv->devices.take(device))
        wpe_gamepad_manager_remove_device(WPE_GAMEPAD_MANAGER(manager), gamepad.get());
}

static void wpeGamepadManagerManetteConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_gamepad_manager_manette_parent_class)->constructed(object);

    auto* manager = WPE_GAMEPAD_MANAGER_MANETTE(object);
    auto* priv = manager->priv;
    priv->monitor = adoptGRef(manette_monitor_new());
    g_signal_connect_object(priv->monitor.get(), "device-connected", G_CALLBACK(+[](WPEGamepadManagerManette* manager, ManetteDevice* device) {
        wpeGamepadManagerManetteAddDevice(manager, device);
    }), object, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->monitor.get(), "device-disconnected", G_CALLBACK(+[](WPEGamepadManagerManette* manager, ManetteDevice* device) {
        wpeGamepadManagerManetteRemoveDevice(manager, device);
    }), object, G_CONNECT_SWAPPED);

    ManetteDevice* device;
    ManetteMonitorIter* iter = manette_monitor_iterate(priv->monitor.get());
    while (manette_monitor_iter_next(iter, &device))
        wpeGamepadManagerManetteAddDevice(manager, device);
    manette_monitor_iter_free(iter);
}

static void wpeGamepadManagerManetteDispose(GObject* object)
{
    auto* priv = WPE_GAMEPAD_MANAGER_MANETTE(object)->priv;
    priv->monitor = nullptr;

    G_OBJECT_CLASS(wpe_gamepad_manager_manette_parent_class)->dispose(object);
}

static void wpe_gamepad_manager_manette_class_init(WPEGamepadManagerManetteClass* gamepadManagerManetteClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(gamepadManagerManetteClass);
    objectClass->constructed = wpeGamepadManagerManetteConstructed;
    objectClass->dispose = wpeGamepadManagerManetteDispose;
}

WPEGamepadManager* wpeGamepadManagerManetteCreate()
{
    return WPE_GAMEPAD_MANAGER(g_object_new(WPE_TYPE_GAMEPAD_MANAGER_MANETTE, nullptr));
}
