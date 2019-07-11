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

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityObject.h"
#include "HTMLNames.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"
#include <wtf/text/CString.h>

using namespace WebCore;

static AccessibilityObject* core(AtkValue* value)
{
    if (!WEBKIT_IS_ACCESSIBLE(value))
        return 0;

    return &webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(value));
}

static bool webkitAccessibleSetNewValue(AtkValue* coreValue, const gdouble newValue)
{
    AccessibilityObject* coreObject = core(coreValue);
    if (!coreObject->canSetValueAttribute())
        return FALSE;

    // Check value against range limits
    double value;
    value = std::max(static_cast<double>(coreObject->minValueForRange()), newValue);
    value = std::min(static_cast<double>(coreObject->maxValueForRange()), newValue);

    coreObject->setValue(String::numberToStringFixedPrecision(value));
    return TRUE;
}

static float webkitAccessibleGetIncrementValue(AccessibilityObject* coreObject)
{
    if (!coreObject->getAttribute(HTMLNames::stepAttr).isEmpty())
        return coreObject->stepValueForRange();

    // If 'step' attribute is not defined, WebCore assumes a 5% of the
    // range between minimum and maximum values. Implicit value of step should be one or larger.
    float step = (coreObject->maxValueForRange() - coreObject->minValueForRange()) * 0.05;
    return step < 1 ? 1 : step;
}

static void webkitAccessibleGetValueAndText(AtkValue* value, gdouble* currentValue, gchar** alternativeText)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    AccessibilityObject* coreObject = core(value);
    if (!coreObject)
        return;

    if (currentValue)
        *currentValue = coreObject->valueForRange();
    if (alternativeText)
        *alternativeText = g_strdup_printf("%s", coreObject->valueDescription().utf8().data());
}

static double webkitAccessibleGetIncrement(AtkValue* value)
{
    g_return_val_if_fail(ATK_VALUE(value), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value), 0);

    AccessibilityObject* coreObject = core(value);
    if (!coreObject)
        return 0;

    return webkitAccessibleGetIncrementValue(coreObject);
}

static void webkitAccessibleSetValue(AtkValue* value, const gdouble newValue)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    webkitAccessibleSetNewValue(value, newValue);
}

static AtkRange* webkitAccessibleGetRange(AtkValue* value)
{
    g_return_val_if_fail(ATK_VALUE(value), nullptr);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value), nullptr);

    AccessibilityObject* coreObject = core(value);
    if (!coreObject)
        return nullptr;

    gdouble minValue = coreObject->minValueForRange();
    gdouble maxValue = coreObject->maxValueForRange();
    gchar* valueDescription = g_strdup_printf("%s", coreObject->valueDescription().utf8().data());
    return atk_range_new(minValue, maxValue, valueDescription);
}

static void webkitAccessibleValueGetCurrentValue(AtkValue* value, GValue* gValue)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_FLOAT);
    g_value_set_float(gValue, core(value)->valueForRange());
}

static void webkitAccessibleValueGetMaximumValue(AtkValue* value, GValue* gValue)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_FLOAT);
    g_value_set_float(gValue, core(value)->maxValueForRange());
}

static void webkitAccessibleValueGetMinimumValue(AtkValue* value, GValue* gValue)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_FLOAT);
    g_value_set_float(gValue, core(value)->minValueForRange());
}

static gboolean webkitAccessibleValueSetCurrentValue(AtkValue* value, const GValue* gValue)
{
    g_return_val_if_fail(ATK_VALUE(value), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value), FALSE);

    double newValue;
    if (G_VALUE_HOLDS_DOUBLE(gValue))
        newValue = g_value_get_double(gValue);
    else if (G_VALUE_HOLDS_FLOAT(gValue))
        newValue = g_value_get_float(gValue);
    else if (G_VALUE_HOLDS_INT64(gValue))
        newValue = g_value_get_int64(gValue);
    else if (G_VALUE_HOLDS_INT(gValue))
        newValue = g_value_get_int(gValue);
    else if (G_VALUE_HOLDS_LONG(gValue))
        newValue = g_value_get_long(gValue);
    else if (G_VALUE_HOLDS_ULONG(gValue))
        newValue = g_value_get_ulong(gValue);
    else if (G_VALUE_HOLDS_UINT64(gValue))
        newValue = g_value_get_uint64(gValue);
    else if (G_VALUE_HOLDS_UINT(gValue))
        newValue = g_value_get_uint(gValue);
    else
        return FALSE;

    return webkitAccessibleSetNewValue(value, newValue);
}

static void webkitAccessibleValueGetMinimumIncrement(AtkValue* value, GValue* gValue)
{
    g_return_if_fail(ATK_VALUE(value));
    returnIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(value));

    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_FLOAT);

    AccessibilityObject* coreObject = core(value);
    g_value_set_float(gValue, webkitAccessibleGetIncrementValue(coreObject));
}

void webkitAccessibleValueInterfaceInit(AtkValueIface* iface)
{
    iface->get_value_and_text = webkitAccessibleGetValueAndText;
    iface->get_increment = webkitAccessibleGetIncrement;
    iface->set_value = webkitAccessibleSetValue;
    iface->get_range = webkitAccessibleGetRange;
    iface->get_current_value = webkitAccessibleValueGetCurrentValue;
    iface->get_maximum_value = webkitAccessibleValueGetMaximumValue;
    iface->get_minimum_value = webkitAccessibleValueGetMinimumValue;
    iface->set_current_value = webkitAccessibleValueSetCurrentValue;
    iface->get_minimum_increment = webkitAccessibleValueGetMinimumIncrement;
}

#endif
