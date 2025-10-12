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
#include "WPEGamepadManette.h"

#include <linux/input-event-codes.h>
#include <optional>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

static constexpr size_t wpeGamepadAxisCount = static_cast<size_t>(WPE_GAMEPAD_AXIS_RIGHT_Y) + 1;
static constexpr size_t wpeGamepadButtonCount = static_cast<size_t>(WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_CENTER) + 1;

struct _WPEGamepadManettePrivate {
    GRefPtr<ManetteDevice> device;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEGamepadManette, wpe_gamepad_manette, WPE_TYPE_GAMEPAD, WPEGamepad)

enum {
    PROP_0,

    PROP_DEVICE,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void wpeGamepadManetteDispose(GObject* object)
{
    auto* priv = WPE_GAMEPAD_MANETTE(object)->priv;
    priv->device = nullptr;

    G_OBJECT_CLASS(wpe_gamepad_manette_parent_class)->dispose(object);
}

static void wpeGamepadManetteSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* gamepad = WPE_GAMEPAD_MANETTE(object);

    switch (propId) {
    case PROP_DEVICE:
        gamepad->priv->device = MANETTE_DEVICE(g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static WPEGamepadButton wpeGamepadButton(uint16_t button)
{
    switch (button) {
    case BTN_SOUTH:
        return WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_BOTTOM;
    case BTN_EAST:
        return WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_RIGHT;
    case BTN_NORTH:
        return WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_TOP;
    case BTN_WEST:
        return WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_LEFT;
    case BTN_TL:
        return WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_FRONT;
    case BTN_TR:
        return WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_FRONT;
    case BTN_TL2:
        return WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_BACK;
    case BTN_TR2:
        return WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_BACK;
    case BTN_SELECT:
        return WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_LEFT;
    case BTN_START:
        return WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_RIGHT;
    case BTN_THUMBL:
        return WPE_GAMEPAD_BUTTON_LEFT_THUMB;
    case BTN_THUMBR:
        return WPE_GAMEPAD_BUTTON_RIGHT_THUMB;
    case BTN_DPAD_UP:
        return WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_TOP;
    case BTN_DPAD_DOWN:
        return WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_BOTTOM;
    case BTN_DPAD_LEFT:
        return WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_LEFT;
    case BTN_DPAD_RIGHT:
        return WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_RIGHT;
    case BTN_MODE:
        return WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_CENTER;
    default:
        break;
    }
    // In case could not map button to a WPE_GAMEPAD_BUTTON value, return
    // a valid WPE_GAMEPAD_BUTTON value anyway.
    return static_cast<WPEGamepadButton>(button % wpeGamepadButtonCount);
}

static WPEGamepadAxis wpeGamepadAxis(uint16_t axis)
{
    switch (axis) {
    case ABS_X:
        return WPE_GAMEPAD_AXIS_LEFT_X;
    case ABS_Y:
        return WPE_GAMEPAD_AXIS_LEFT_Y;
    case ABS_RX:
        return WPE_GAMEPAD_AXIS_RIGHT_X;
    case ABS_RY:
        return WPE_GAMEPAD_AXIS_RIGHT_Y;
    }
    // In case could not map axis to a WPE_GAMEPAD_AXIS value, return
    // a valid WPE_GAMEPAD_AXIS value anyway.
    return static_cast<WPEGamepadAxis>(axis % wpeGamepadAxisCount);
}

static void wpeGamepadManetteStartInputMonitor(WPEGamepad* gamepad)
{
    auto* priv = WPE_GAMEPAD_MANETTE(gamepad)->priv;

    g_signal_connect_object(priv->device.get(), "button-press-event", G_CALLBACK(+[](WPEGamepad* gamepad, ManetteEvent* event) {
        uint16_t button;
        if (!manette_event_get_button(event, &button))
            return;
        wpe_gamepad_button_event(gamepad, wpeGamepadButton(button), TRUE);
    }), gamepad, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->device.get(), "button-release-event", G_CALLBACK(+[](WPEGamepad* gamepad, ManetteEvent* event) {
        uint16_t button;
        if (!manette_event_get_button(event, &button))
            return;
        wpe_gamepad_button_event(gamepad, wpeGamepadButton(button), FALSE);
    }), gamepad, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->device.get(), "absolute-axis-event", G_CALLBACK(+[](WPEGamepad* gamepad, ManetteEvent* event) {
        uint16_t axis;
        double value;
        if (!manette_event_get_absolute(event, &axis, &value))
            return;
        wpe_gamepad_axis_event(gamepad, wpeGamepadAxis(axis), value);
    }), gamepad, G_CONNECT_SWAPPED);
}

static void wpeGamepadManetteStopInputMonitor(WPEGamepad* gamepad)
{
    auto* priv = WPE_GAMEPAD_MANETTE(gamepad)->priv;
    g_signal_handlers_disconnect_by_data(priv->device.get(), gamepad);
}

static void wpe_gamepad_manette_class_init(WPEGamepadManetteClass* gamepadManetteClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(gamepadManetteClass);
    objectClass->set_property = wpeGamepadManetteSetProperty;
    objectClass->dispose = wpeGamepadManetteDispose;

    WPEGamepadClass* gamepadClass = WPE_GAMEPAD_CLASS(gamepadManetteClass);
    gamepadClass->start_input_monitor = wpeGamepadManetteStartInputMonitor;
    gamepadClass->stop_input_monitor = wpeGamepadManetteStopInputMonitor;

    sObjProperties[PROP_DEVICE] =
        g_param_spec_object(
            "device",
            nullptr, nullptr,
            MANETTE_TYPE_DEVICE,
            static_cast<GParamFlags>(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

WPEGamepad* wpeGamepadManetteCreate(ManetteDevice* device)
{
    return WPE_GAMEPAD(g_object_new(WPE_TYPE_GAMEPAD_MANETTE, "name", manette_device_get_name(device), "device", device, nullptr));
}
