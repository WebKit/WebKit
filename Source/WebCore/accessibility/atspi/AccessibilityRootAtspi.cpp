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
#include "AccessibilityRootAtspi.h"

#if USE(ATSPI)
#include "AXObjectCache.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityAtspiInterfaces.h"
#include "Document.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include <glib/gi18n-lib.h>
#include <locale.h>

namespace WebCore {

Ref<AccessibilityRootAtspi> AccessibilityRootAtspi::create(Page& page)
{
    return adoptRef(*new AccessibilityRootAtspi(page));
}

AccessibilityRootAtspi::AccessibilityRootAtspi(Page& page)
    : m_page(page)
{
}

GDBusInterfaceVTable AccessibilityRootAtspi::s_accessibleFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(methodName, "GetRole"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", Atspi::Role::Filler));
        else if (!g_strcmp0(methodName, "GetRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", "filler"));
        else if (!g_strcmp0(methodName, "GetLocalizedRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", _("filler")));
        else if (!g_strcmp0(methodName, "GetState")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(au)"));

            g_variant_builder_open(&builder, G_VARIANT_TYPE("au"));
            g_variant_builder_add(&builder, "u", 0);
            g_variant_builder_add(&builder, "u", 0);
            g_variant_builder_close(&builder);

            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        } else if (!g_strcmp0(methodName, "GetAttributes")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(a{ss})"));

            g_variant_builder_open(&builder, G_VARIANT_TYPE("a{ss}"));
#if PLATFORM(GTK)
            g_variant_builder_add(&builder, "{ss}", "toolkit", "WebKitGTK");
#elif PLATFORM(WPE)
            g_variant_builder_add(&builder, "{ss}", "toolkit", "WPEWebKit");
#endif
            g_variant_builder_close(&builder);

            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        } else if (!g_strcmp0(methodName, "GetApplication"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", rootObject.applicationReference()));
        else if (!g_strcmp0(methodName, "GetChildAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            if (!index) {
                if (auto* child = rootObject.child()) {
                    g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", child->reference()));
                    return;
                }
            }
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetChildren")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            if (auto* child = rootObject.child())
                g_variant_builder_add(&builder, "@(so)", child->reference());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(so))", &builder));
        } else if (!g_strcmp0(methodName, "GetIndexInParent")) {
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", 0));
        } else if (!g_strcmp0(methodName, "GetRelationSet")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(ua(so))"));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ua(so)))", &builder));
        } else if (!g_strcmp0(methodName, "GetInterfaces")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("as"));

            g_variant_builder_add(&builder, "s", webkit_accessible_interface.name);
            g_variant_builder_add(&builder, "s", webkit_component_interface.name);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", &builder));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(propertyName, "Name"))
            return g_variant_new_string("");
        if (!g_strcmp0(propertyName, "Description"))
            return g_variant_new_string("");
        if (!g_strcmp0(propertyName, "Locale"))
            return g_variant_new_string(setlocale(LC_MESSAGES, nullptr));
        if (!g_strcmp0(propertyName, "AccessibleId"))
            return g_variant_new_string("");
        if (!g_strcmp0(propertyName, "Parent"))
            return rootObject.parentReference();
        if (!g_strcmp0(propertyName, "ChildCount"))
            return g_variant_new_int32(rootObject.child() ? 1 : 0);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

GDBusInterfaceVTable AccessibilityRootAtspi::s_socketFunctions = {
    // method_call
    [](GDBusConnection*, const gchar* sender, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(methodName, "Embedded")) {
            const char* path;
            g_variant_get(parameters, "(&s)", &path);
            rootObject.embedded(sender, path);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        }
    },
    // get_property
    nullptr,
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

void AccessibilityRootAtspi::registerObject(CompletionHandler<void(const String&)>&& completionHandler)
{
    if (m_page)
        m_page->setAccessibilityRootObject(this);

    Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>> interfaces;
    interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_accessible_interface), &s_accessibleFunctions });
    interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_socket_interface), &s_socketFunctions });
    interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_component_interface), &s_componentFunctions });
    AccessibilityAtspi::singleton().registerRoot(*this, WTFMove(interfaces), WTFMove(completionHandler));
}

void AccessibilityRootAtspi::unregisterObject()
{
    AccessibilityAtspi::singleton().unregisterRoot(*this);

    if (m_page)
        m_page->setAccessibilityRootObject(nullptr);
}

void AccessibilityRootAtspi::setPath(String&& path)
{
    m_path = WTFMove(path);
}

void AccessibilityRootAtspi::embedded(const char* parentUniqueName, const char* parentPath)
{
    m_parentUniqueName = String::fromUTF8(parentUniqueName);
    m_parentPath = String::fromUTF8(parentPath);
    AccessibilityAtspi::singleton().parentChanged(*this);
}

GVariant* AccessibilityRootAtspi::applicationReference() const
{
    if (m_parentUniqueName.isNull())
        return AccessibilityAtspi::singleton().nullReference();
    return g_variant_new("(so)", m_parentUniqueName.utf8().data(), "/org/a11y/atspi/accessible/root");
}

GVariant* AccessibilityRootAtspi::reference() const
{
    return g_variant_new("(so)", AccessibilityAtspi::singleton().uniqueName(), m_path.utf8().data());
}

GVariant* AccessibilityRootAtspi::parentReference() const
{
    if (m_parentUniqueName.isNull())
        return AccessibilityAtspi::singleton().nullReference();
    return g_variant_new("(so)", m_parentUniqueName.utf8().data(), m_parentPath.utf8().data());
}

AccessibilityObjectAtspi* AccessibilityRootAtspi::child() const
{
    if (!m_page)
        return nullptr;

    Frame& frame = m_page->mainFrame();
    if (!frame.document())
        return nullptr;

    AXObjectCache::enableAccessibility();
    AXObjectCache* cache = frame.document()->axObjectCache();
    if (!cache)
        return nullptr;

    AXCoreObject* rootObject = cache->rootObject();
    return rootObject ? rootObject->wrapper() : nullptr;
}

void AccessibilityRootAtspi::childAdded(AccessibilityObjectAtspi& child)
{
    AccessibilityAtspi::singleton().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Added);
}

void AccessibilityRootAtspi::childRemoved(AccessibilityObjectAtspi& child)
{
    AccessibilityAtspi::singleton().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Removed);
}

void AccessibilityRootAtspi::serialize(GVariantBuilder* builder) const
{
    g_variant_builder_add(builder, "(so)", AccessibilityAtspi::singleton().uniqueName(), m_path.utf8().data());
    g_variant_builder_add(builder, "@(so)", applicationReference());
    g_variant_builder_add(builder, "@(so)", parentReference());

    g_variant_builder_add(builder, "i", 0);
    // Do not set the children count in cache, because child is handled by children-changed signals.
    g_variant_builder_add(builder, "i", -1);

    GVariantBuilder interfaces = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("as"));
    g_variant_builder_add(&interfaces, "s", webkit_accessible_interface.name);
    g_variant_builder_add(&interfaces, "s", webkit_component_interface.name);
    g_variant_builder_add(builder, "@as", g_variant_new("as", &interfaces));

    g_variant_builder_add(builder, "s", "");

    g_variant_builder_add(builder, "u", Atspi::Role::Filler);

    g_variant_builder_add(builder, "s", "");

    GVariantBuilder states = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("au"));
    g_variant_builder_add(&states, "u", 0);
    g_variant_builder_add(&states, "u", 0);
    g_variant_builder_add(builder, "@au", g_variant_builder_end(&states));
}

GDBusInterfaceVTable AccessibilityRootAtspi::s_componentFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(methodName, "Contains"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetAccessibleAtPoint"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetExtents")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = rootObject.frameRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((iiii))", rect.x(), rect.y(), rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetPosition")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = rootObject.frameRect(static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((ii))", rect.x(), rect.y()));
        } else if (!g_strcmp0(methodName, "GetSize")) {
            auto rect = rootObject.frameRect(Atspi::CoordinateType::ParentCoordinates);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((ii))", rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetLayer"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", static_cast<uint32_t>(Atspi::ComponentLayer::WidgetLayer)));
        else if (!g_strcmp0(methodName, "GetMDIZOrder"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(n)", 0));
        else if (!g_strcmp0(methodName, "GrabFocus"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetAlpha"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(d)", 1.0));
        else if ((!g_strcmp0(methodName, "SetExtents")) || !g_strcmp0(methodName, "SetPosition") || !g_strcmp0(methodName, "SetSize") || !g_strcmp0(methodName, "ScrollTo") || !g_strcmp0(methodName, "ScrollToPoint"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    },
    // get_property
    nullptr,
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

IntRect AccessibilityRootAtspi::frameRect(Atspi::CoordinateType coordinateType) const
{
    if (!m_page)
        return { };

    auto* frameView = m_page->mainFrame().view();
    if (!frameView)
        return { };

    auto frameRect = frameView->frameRect();
    switch (coordinateType) {
    case Atspi::CoordinateType::ScreenCoordinates:
        return frameView->contentsToScreen(frameRect);
    case Atspi::CoordinateType::WindowCoordinates:
        return frameView->contentsToWindow(frameRect);
    case Atspi::CoordinateType::ParentCoordinates:
        return frameRect;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // USE(ATSPI)
