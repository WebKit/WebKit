/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Jan Michael Alonzo
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityController.h"

#include "AccessibilityUIElement.h"
#include "DumpRenderTree.h"
#include "WebCoreSupport/DumpRenderTreeSupportGtk.h"

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

static bool loggingAccessibilityEvents = false;

static guint state_change_listener_id = 0;
static guint focus_event_listener_id = 0;
static guint active_descendant_changed_listener_id = 0;
static guint children_changed_listener_id = 0;
static guint property_changed_listener_id = 0;
static guint visible_data_changed_listener_id = 0;

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

AccessibilityUIElement AccessibilityController::elementAtPoint(int x, int y)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    AtkObject* accessible =  DumpRenderTreeSupportGtk::getFocusedAccessibleElement(mainFrame);
    if (!accessible)
        return 0;

    return AccessibilityUIElement(accessible);
}

AccessibilityUIElement AccessibilityController::rootElement()
{
    AtkObject* accessible = DumpRenderTreeSupportGtk::getRootAccessibleElement(mainFrame);
    if (!accessible)
        return 0;

    return AccessibilityUIElement(accessible);
}

void AccessibilityController::setLogFocusEvents(bool)
{
}

void AccessibilityController::setLogScrollingStartEvents(bool)
{
}

void AccessibilityController::setLogValueChangeEvents(bool)
{
}

static gboolean accessibility_event_listener(GSignalInvocationHint *signal_hint,
                                            guint n_param_values,
                                            const GValue *param_values,
                                            gpointer data)
{
    // At least we should receive the instance emitting the signal.
    if (n_param_values < 1)
        return TRUE;

    AtkObject* accessible = ATK_OBJECT(g_value_get_object(&param_values[0]));
    const gchar* axObjectName = 0;
    guint axObjectRole = 0;
    if (accessible) {
        axObjectName = atk_object_get_name(accessible);
        axObjectRole = atk_object_get_role(accessible);
    }

    GSignalQuery signal_query;
    gchar* signalName = 0;

    g_signal_query(signal_hint->signal_id, &signal_query);

    if (!g_strcmp0(signal_query.signal_name, "state-change")) {
        signalName = g_strdup_printf("state-change:%s = %d",
                                     g_value_get_string(&param_values[1]),
                                     g_value_get_boolean(&param_values[2]));
    } else if (!g_strcmp0(signal_query.signal_name, "focus-event")) {
        signalName = g_strdup_printf("focus-event = %d",
                                     g_value_get_boolean(&param_values[1]));
    } else if (!g_strcmp0(signal_query.signal_name, "children-changed")) {
        signalName = g_strdup_printf("children-changed = %d",
                                     g_value_get_uint(&param_values[1]));
    } else {
        signalName = g_strdup(signal_query.signal_name);
    }

    // Try to provide always a name to be logged for the object.
    gchar* objectName = 0;
    if (!axObjectName || *axObjectName == '\0')
        objectName = g_strdup("(No name)");
    else
        objectName = g_strdup(axObjectName);

    printf("Accessibility object emitted \"%s\" / Name: \"%s\" / Role: %d\n", signalName, objectName, axObjectRole);
    g_free(signalName);
    g_free(objectName);

    return TRUE;
}

void AccessibilityController::setLogAccessibilityEvents(bool logAccessibilityEvents)
{
    if (logAccessibilityEvents == loggingAccessibilityEvents)
        return;

    if (state_change_listener_id) {
        atk_remove_global_event_listener(state_change_listener_id);
        state_change_listener_id = 0;
    }
    if (focus_event_listener_id) {
        atk_remove_global_event_listener(focus_event_listener_id);
        focus_event_listener_id = 0;
    }
    if (active_descendant_changed_listener_id) {
        atk_remove_global_event_listener(active_descendant_changed_listener_id);
        active_descendant_changed_listener_id = 0;
    }
    if (children_changed_listener_id) {
        atk_remove_global_event_listener(children_changed_listener_id);
        children_changed_listener_id = 0;
    }
    if (property_changed_listener_id) {
        atk_remove_global_event_listener(property_changed_listener_id);
        property_changed_listener_id = 0;
    }
    if (visible_data_changed_listener_id) {
        atk_remove_global_event_listener(visible_data_changed_listener_id);
        visible_data_changed_listener_id = 0;
    }

    if (!logAccessibilityEvents) {
        loggingAccessibilityEvents = false;
        return;
    }

    // Ensure that accessibility is initialized for the WebView by querying for
    // the root accessible object.
    rootElement();

    // Add global listeners
    state_change_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:state-change");
    focus_event_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:focus-event");
    active_descendant_changed_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:active-descendant-changed");
    children_changed_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:children-changed");
    property_changed_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:property-change");
    visible_data_changed_listener_id = atk_add_global_event_listener(accessibility_event_listener, "Gtk:AtkObject:visible-data-changed");

    loggingAccessibilityEvents = true;
}

void AccessibilityController::addNotificationListener(PlatformUIElement, JSObjectRef)
{
}

void AccessibilityController::notificationReceived(PlatformUIElement, const std::string&)
{
}
