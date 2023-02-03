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
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityObject.h"
#include "AccessibilityObjectInterface.h"
#include "Document.h"
#include "FrameView.h"
#include "RenderLayer.h"

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_componentFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "Contains")) {
            int x, y;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iiu)", &x, &y, &coordinateType);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", !!atspiObject->hitTest({ x, y }, static_cast<Atspi::CoordinateType>(coordinateType))));
        } else if (!g_strcmp0(methodName, "GetAccessibleAtPoint")) {
            int x, y;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iiu)", &x, &y, &coordinateType);
            auto* wrapper = atspiObject->hitTest({ x, y }, static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", wrapper ? wrapper->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetExtents")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = atspiObject->elementRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((iiii))", rect.x(), rect.y(), rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetPosition")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = atspiObject->elementRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(ii)", rect.x(), rect.y()));
        } else if (!g_strcmp0(methodName, "GetSize")) {
            auto rect = atspiObject->elementRect(Atspi::CoordinateType::ParentCoordinates);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(ii)", rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetLayer"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", static_cast<uint32_t>(Atspi::ComponentLayer::WidgetLayer)));
        else if (!g_strcmp0(methodName, "GetMDIZOrder"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(n)", 0));
        else if (!g_strcmp0(methodName, "GrabFocus"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->focus()));
        else if (!g_strcmp0(methodName, "GetAlpha"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(d)", atspiObject->opacity()));
        else if (!g_strcmp0(methodName, "ScrollTo")) {
            uint32_t scrollType;
            g_variant_get(parameters, "(u)", &scrollType);
            atspiObject->scrollToMakeVisible(static_cast<Atspi::ScrollType>(scrollType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", TRUE));
        } else if (!g_strcmp0(methodName, "ScrollToPoint")) {
            int x, y;
            uint32_t coordinateType;
            g_variant_get(parameters, "(uii)", &coordinateType, &x, &y);
            atspiObject->scrollToPoint({ x, y }, static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", TRUE));
        } else if (!g_strcmp0(methodName, "SetExtents") || !g_strcmp0(methodName, "SetPosition") || !g_strcmp0(methodName, "SetSize"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    },
    // get_property
    nullptr,
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

AccessibilityObjectAtspi* AccessibilityObjectAtspi::hitTest(const IntPoint& point, Atspi::CoordinateType coordinateType) const
{
    if (!m_coreObject)
        return nullptr;

    IntPoint convertedPoint = point;
    if (auto* frameView = m_coreObject->documentFrameView()) {
        switch (coordinateType) {
        case Atspi::CoordinateType::ScreenCoordinates:
            convertedPoint = frameView->screenToContents(point);
            break;
        case Atspi::CoordinateType::WindowCoordinates:
            convertedPoint = frameView->windowToContents(point);
            break;
        case Atspi::CoordinateType::ParentCoordinates:
            break;
        }
    }

    m_coreObject->updateChildrenIfNecessary();
    if (auto* coreObject = m_coreObject->accessibilityHitTest(convertedPoint))
        return coreObject->wrapper();

    return nullptr;
}

IntRect AccessibilityObjectAtspi::elementRect(Atspi::CoordinateType coordinateType) const
{
    if (!m_coreObject)
        return { };

    auto rect = snappedIntRect(m_coreObject->elementRect());
    auto* frameView = m_coreObject->documentFrameView();
    if (!frameView)
        return rect;

    switch (coordinateType) {
    case Atspi::CoordinateType::ScreenCoordinates:
        return frameView->contentsToScreen(rect);
    case Atspi::CoordinateType::WindowCoordinates:
        return frameView->contentsToWindow(rect);
    case Atspi::CoordinateType::ParentCoordinates:
        return rect;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessibilityObjectAtspi::focus() const
{
    if (!m_coreObject)
        return false;

    m_coreObject->setFocused(true);
    m_coreObject->updateBackingStore();
    return m_coreObject->isFocused();
}

float AccessibilityObjectAtspi::opacity() const
{
    if (!m_coreObject)
        return 1;

    if (auto* renderer = m_coreObject->renderer())
        return renderer->style().opacity();

    return 1;
}

void AccessibilityObjectAtspi::scrollToMakeVisible(Atspi::ScrollType scrollType) const
{
    auto* liveObject = dynamicDowncast<AccessibilityObject>(m_coreObject);
    if (!liveObject)
        return;

    ScrollAlignment alignX;
    ScrollAlignment alignY;
    switch (scrollType) {
    case Atspi::ScrollType::TopLeft:
        alignX = ScrollAlignment::alignLeftAlways;
        alignY = ScrollAlignment::alignTopAlways;
        break;
    case Atspi::ScrollType::BottomRight:
        alignX = ScrollAlignment::alignRightAlways;
        alignY = ScrollAlignment::alignBottomAlways;
        break;
    case Atspi::ScrollType::TopEdge:
    case Atspi::ScrollType::BottomEdge:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignToEdgeIfNeeded;
        break;
    case Atspi::ScrollType::LeftEdge:
    case Atspi::ScrollType::RightEdge:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignToEdgeIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    case Atspi::ScrollType::Anywhere:
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    }

    liveObject->scrollToMakeVisible({ SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::Yes });
}

void AccessibilityObjectAtspi::scrollToPoint(const IntPoint& point, Atspi::CoordinateType coordinateType) const
{
    if (!m_coreObject)
        return;

    IntPoint convertedPoint(point);
    if (coordinateType == Atspi::CoordinateType::ScreenCoordinates) {
        if (auto* frameView = m_coreObject->documentFrameView())
            convertedPoint = frameView->contentsToWindow(frameView->screenToContents(point));
    }
    m_coreObject->scrollToGlobalPoint(convertedPoint);
}

} // namespace WebCore

#endif // USE(ATSPI)
