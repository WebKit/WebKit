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
#include "WPEGamepad.h"

#include "WPEEnumTypes.h"
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

/**
 * WPEGamepad:
 *
 */
struct _WPEGamepadPrivate {
    CString name;
    bool isMonitoringInput;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEGamepad, wpe_gamepad, G_TYPE_OBJECT)

enum {
    PROP_0,

    PROP_NAME,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

enum {
    BUTTON_EVENT,
    AXIS_EVENT,

    LAST_SIGNAL
};

static std::array<unsigned, LAST_SIGNAL> signals;

static void wpeGamepadSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* gamepad = WPE_GAMEPAD(object);

    switch (propId) {
    case PROP_NAME:
        gamepad->priv->name = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeGamepadGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* gamepad = WPE_GAMEPAD(object);

    switch (propId) {
    case PROP_NAME:
        g_value_set_string(value, wpe_gamepad_get_name(gamepad));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpe_gamepad_class_init(WPEGamepadClass* gamepadClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(gamepadClass);
    objectClass->set_property = wpeGamepadSetProperty;
    objectClass->get_property = wpeGamepadGetProperty;

    /**
     * WPEGamepad:name:
     *
     * The name of the gamepad.
     */
    sObjProperties[PROP_NAME] =
        g_param_spec_string(
            "name",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());

    /**
     * WPEGamepad::button-event:
     * @gamepad: a #WPEGamepad
     * @button: a #WPEGamepadButton
     * @is_pressed: whether @button is pressed or not
     *
     * Emitted when a button is pressed or released.
     */
    signals[BUTTON_EVENT] = g_signal_new(
        "button-event",
        G_TYPE_FROM_CLASS(gamepadClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WPE_TYPE_GAMEPAD_BUTTON,
        G_TYPE_BOOLEAN);

    /**
     * WPEGamepad::axis-event:
     * @gamepad: a #WPEGamepad
     * @axis: a #WPEGamepadAxis
     * @value: the value of @axis
     *
     * Emitted when value of @axis changes.
     */
    signals[AXIS_EVENT] = g_signal_new(
        "axis-event",
        G_TYPE_FROM_CLASS(gamepadClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WPE_TYPE_GAMEPAD_AXIS,
        G_TYPE_DOUBLE);
}

/**
 * wpe_gamepad_get_name:
 * @gamepad: a #WPEGamepad
 *
 * Get the @gamepad name.
 *
 * Returns: (transfer none): the gamepad name
 */
const char* wpe_gamepad_get_name(WPEGamepad* gamepad)
{
    g_return_val_if_fail(WPE_IS_GAMEPAD(gamepad), nullptr);

    return gamepad->priv->name.data();
}

/**
 * wpe_gamepad_start_input_monitor:
 * @gamepad: a #WPEGamepad
 *
 * Start monitoring input on @gamepad. Signals #WPEGamepad::button-event
 * and #WPEGamepad::axis-event will not be emitted until this function
 * is called.
 */
void wpe_gamepad_start_input_monitor(WPEGamepad* gamepad)
{
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));

    if (gamepad->priv->isMonitoringInput)
        return;

    gamepad->priv->isMonitoringInput = true;
    auto* gamepadClass = WPE_GAMEPAD_GET_CLASS(gamepad);
    if (gamepadClass->start_input_monitor)
        gamepadClass->start_input_monitor(gamepad);
}

/**
 * wpe_gamepad_stop_input_monitor:
 * @gamepad: a #WPEGamepad
 *
 * Stop monitoring input on @gamepad. Signals #WPEGamepad::button-event
 * and #WPEGamepad::axis-event will not be emitted after this function
 * is called.
 */
void wpe_gamepad_stop_input_monitor(WPEGamepad* gamepad)
{
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));

    if (!gamepad->priv->isMonitoringInput)
        return;

    gamepad->priv->isMonitoringInput = false;
    auto* gamepadClass = WPE_GAMEPAD_GET_CLASS(gamepad);
    if (gamepadClass->stop_input_monitor)
        gamepadClass->stop_input_monitor(gamepad);
}

/**
 * wpe_gamepad_button_event:
 * @gamepad: a #WPEGamepad
 * @button: a #WPEGamepadButton
 * @is_pressed: whether @button is pressed or not
 *
 * Emit the signal #WPEGamepad::button-event
 */
void wpe_gamepad_button_event(WPEGamepad* gamepad, WPEGamepadButton button, gboolean isPressed)
{
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));

    g_signal_emit(gamepad, signals[BUTTON_EVENT], 0, button, isPressed);
}

/**
 * wpe_gamepad_axis_event:
 * @gamepad: a #WPEGamepad
 * @axis: a #WPEGamepadAxis
 * @value: the value of @axis
 *
 * Emit the signal #WPEGamepad::axis-event
 */
void wpe_gamepad_axis_event(WPEGamepad* gamepad, WPEGamepadAxis axis, gdouble value)
{
    g_return_if_fail(WPE_IS_GAMEPAD(gamepad));
    g_return_if_fail(value >= -1. && value <= 1.);

    g_signal_emit(gamepad, signals[AXIS_EVENT], 0, axis, value);
}

/**
 * wpe_gamepad_has_rumble:
 * @gamepad: a #WPEGamepad
 *
 * Checks whether a #WPEGamepad has rumble support.
 * Returns %TRUE if the @gamepad has rumble, %FALSE otherwise.
 */
gboolean wpe_gamepad_has_rumble(WPEGamepad *gamepad)
{
    g_return_val_if_fail(WPE_IS_GAMEPAD(gamepad), FALSE);

    auto* gamepadClass = WPE_GAMEPAD_GET_CLASS(gamepad);
    if (gamepadClass->has_rumble)
        return gamepadClass->has_rumble(gamepad);

    return FALSE;
}

/**
 * wpe_gamepad_rumble:
 * @gamepad: a #WPEGamepad
 * @strong_magnitude: the magnitude for the heavy motor
 * @weak_magnitude: the magnitude for the light motor
 * @duration_ms: the rumble effect play time, in milliseconds
 *
 * Attempts to make a #WPEGamepad rumble for @duration_ms milliseconds.
 * Returns %TRUE if the rumble effect was played, %FALSE otherwise.
 */
gboolean wpe_gamepad_rumble(WPEGamepad *gamepad, gdouble strongMagnitude, gdouble weakMagnitude, guint durationMs)
{
    g_return_val_if_fail(WPE_IS_GAMEPAD(gamepad), FALSE);

    auto* gamepadClass = WPE_GAMEPAD_GET_CLASS(gamepad);
    if (gamepadClass->rumble)
        return gamepadClass->rumble(gamepad, strongMagnitude, weakMagnitude, durationMs);

    return FALSE;
}
