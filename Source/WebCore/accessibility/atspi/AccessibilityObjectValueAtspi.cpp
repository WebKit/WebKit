/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "AccessibilityObjectAtspi.h"

#if USE(ATSPI)

#include "AccessibilityAtspi.h"
#include "AccessibilityObject.h"
#include <gio/gio.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_valueFunctions = {
    // method_call
    nullptr,
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "CurrentValue"))
            return g_variant_new_double(atspiObject->currentValue());
        if (!g_strcmp0(propertyName, "MinimumValue"))
            return g_variant_new_double(atspiObject->minimumValue());
        if (!g_strcmp0(propertyName, "MaximumValue"))
            return g_variant_new_double(atspiObject->maximumValue());
        if (!g_strcmp0(propertyName, "MinimumIncrement"))
            return g_variant_new_double(atspiObject->minimumIncrement());

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GVariant* propertyValue, GError** error, gpointer userData) -> gboolean {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "CurrentValue"))
            return atspiObject->setCurrentValue(g_variant_get_double(propertyValue));

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return FALSE;
    },
    // padding
    { nullptr }
};

double AccessibilityObjectAtspi::currentValue() const
{
    return m_coreObject ? m_coreObject->valueForRange() : 0;
}

bool AccessibilityObjectAtspi::setCurrentValue(double value)
{
    if (!m_coreObject)
        return false;

    if (!m_coreObject->canSetValueAttribute())
        return false;

    if (m_coreObject->canSetNumericValue())
        return m_coreObject->setValue(value);

    return m_coreObject->setValue(String::numberToStringFixedPrecision(value));
}

double AccessibilityObjectAtspi::minimumValue() const
{
    return m_coreObject ? m_coreObject->minValueForRange() : 0;
}

double AccessibilityObjectAtspi::maximumValue() const
{
    return m_coreObject ? m_coreObject->maxValueForRange() : 0;
}

double AccessibilityObjectAtspi::minimumIncrement() const
{
    if (!m_coreObject)
        return 0;

    auto stepAttribute = static_cast<AccessibilityObject*>(m_coreObject)->getAttribute(HTMLNames::stepAttr);
    if (!stepAttribute.isEmpty())
        return stepAttribute.toFloat();

    // If 'step' attribute is not defined, WebCore assumes a 5% of the range between
    // minimum and maximum values. Implicit value of step should be one or larger.
    float step = (m_coreObject->maxValueForRange() - m_coreObject->minValueForRange()) * 0.05;
    return step < 1 ? 1 : step;
}

void AccessibilityObjectAtspi::valueChanged(double value)
{
    AccessibilityAtspi::singleton().valueChanged(*this, value);
}

} // namespace WebCore

#endif // USE(ATSPI)
