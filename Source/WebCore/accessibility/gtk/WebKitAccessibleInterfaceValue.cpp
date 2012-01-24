/*
 * Copyright (C) 2011, 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitAccessibleInterfaceValue.h"

#include "AccessibilityObject.h"
#include "WebKitAccessibleWrapperAtk.h"

using namespace WebCore;

static AccessibilityObject* core(AtkValue* value)
{
    if (!WEBKIT_IS_ACCESSIBLE(value))
        return 0;

    return webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(value));
}

void webkitAccessibleValueInterfaceInit(AtkValueIface* iface)
{
    iface->get_current_value = webkitAccessibleValueGetCurrentValue;
    iface->get_maximum_value = webkitAccessibleValueGetMaximumValue;
    iface->get_minimum_value = webkitAccessibleValueGetMinimumValue;
    iface->set_current_value = webkitAccessibleValueSetCurrentValue;
    iface->get_minimum_increment = webkitAccessibleValueGetMinimumIncrement;
}

void webkitAccessibleValueGetCurrentValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->valueForRange());
}

void webkitAccessibleValueGetMaximumValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->maxValueForRange());
}

void webkitAccessibleValueGetMinimumValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->minValueForRange());
}

gboolean webkitAccessibleValueSetCurrentValue(AtkValue* value, const GValue* gValue)
{
    if (!G_VALUE_HOLDS_DOUBLE(gValue) && !G_VALUE_HOLDS_INT(gValue))
        return FALSE;

    AccessibilityObject* coreObject = core(value);
    if (!coreObject->canSetValueAttribute())
        return FALSE;

    if (G_VALUE_HOLDS_DOUBLE(gValue))
        coreObject->setValue(String::number(g_value_get_double(gValue)));
    else
        coreObject->setValue(String::number(g_value_get_int(gValue)));

    return TRUE;
}

void webkitAccessibleValueGetMinimumIncrement(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);

    // There's not such a thing in the WAI-ARIA specification, thus return zero.
    g_value_set_double(gValue, 0.0);
}
