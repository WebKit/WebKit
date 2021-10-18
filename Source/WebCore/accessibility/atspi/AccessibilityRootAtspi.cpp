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

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
#include "AXObjectCache.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityAtspiInterfaces.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include <glib/gi18n-lib.h>
#include <wtf/MainThread.h>

namespace WebCore {

Ref<AccessibilityRootAtspi> AccessibilityRootAtspi::create(Page& page, AccessibilityAtspi& atspi)
{
    return adoptRef(*new AccessibilityRootAtspi(page, atspi));
}

AccessibilityRootAtspi::AccessibilityRootAtspi(Page& page, AccessibilityAtspi& atspi)
    : m_atspi(atspi)
    , m_page(page)
{
    RELEASE_ASSERT(isMainThread());
}

GDBusInterfaceVTable AccessibilityRootAtspi::s_accessibleFunctions = {
    // method_call
    [](GDBusConnection*, const gchar* sender, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        RELEASE_ASSERT(!isMainThread());
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(methodName, "GetRole"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", Atspi::Role::Filler));
        else if (!g_strcmp0(methodName, "GetRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", "filler"));
        else if (!g_strcmp0(methodName, "GetLocalizedRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", _("filler")));
        else if (!g_strcmp0(methodName, "GetState")) {
#if USE(GTK4)
            // FIXME: we need a way to get the parent atspi reference in GTK4.
#else
            // Since we don't have a way to know the unique name of the UI process, right after calling
            // atk_socket_embed() the UI process calls atk_object_ref_state_set() to force a GetState message.
            // We use this first GetState message to set the sender as the parent unique name.
            if (rootObject.m_parentUniqueName.isNull())
                rootObject.m_parentUniqueName = sender;
#endif
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(au)"));

            uint64_t atspiStates = (G_GUINT64_CONSTANT(1) << Atspi::State::ManagesDescendants);
            g_variant_builder_open(&builder, G_VARIANT_TYPE("au"));
            g_variant_builder_add(&builder, "u", static_cast<uint32_t>(atspiStates & 0xffffffff));
            g_variant_builder_add(&builder, "u", static_cast<uint32_t>(atspiStates >> 32));
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
                auto* child = Accessibility::retrieveValueFromMainThread<AccessibilityObjectAtspi*>([&rootObject]() -> AccessibilityObjectAtspi* {
                    return rootObject.child();
                });
                if (child) {
                    g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", child->reference()));
                    return;
                }
            }
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", rootObject.atspi().nullReference()));
        } else if (!g_strcmp0(methodName, "GetChildren")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            auto* child = Accessibility::retrieveValueFromMainThread<AccessibilityObjectAtspi*>([&rootObject]() -> AccessibilityObjectAtspi* {
                return rootObject.child();
            });
            if (child)
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
        RELEASE_ASSERT(!isMainThread());
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
            return g_variant_new("(so)", rootObject.m_parentUniqueName.utf8().data(), rootObject.m_parentPath.utf8().data());
        if (!g_strcmp0(propertyName, "ChildCount")) {
            auto childCount = Accessibility::retrieveValueFromMainThread<int32_t>([&rootObject]() -> int32_t {
                return rootObject.child() ? 1 : 0;
            });
            return g_variant_new_int32(childCount);
        }

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    nullptr
};

void AccessibilityRootAtspi::registerObject(CompletionHandler<void(const String&)>&& completionHandler)
{
    RELEASE_ASSERT(isMainThread());
    Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>> interfaces;
    interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_accessible_interface), &s_accessibleFunctions });
    interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_component_interface), &s_componentFunctions });
    m_atspi.registerRoot(*this, WTFMove(interfaces), WTFMove(completionHandler));
}

void AccessibilityRootAtspi::unregisterObject()
{
    RELEASE_ASSERT(isMainThread());
    m_atspi.unregisterRoot(*this);
}

void AccessibilityRootAtspi::setPath(String&& path)
{
    RELEASE_ASSERT(!isMainThread());
    m_path = WTFMove(path);
}

void AccessibilityRootAtspi::setParentPath(String&& path)
{
    RELEASE_ASSERT(isMainThread());
    m_parentPath = WTFMove(path);
}

GVariant* AccessibilityRootAtspi::applicationReference() const
{
    RELEASE_ASSERT(!isMainThread());
    if (m_parentUniqueName.isNull())
        return m_atspi.nullReference();
    return g_variant_new("(so)", m_parentUniqueName.utf8().data(), "/org/a11y/atspi/accessible/root");
}

GVariant* AccessibilityRootAtspi::reference() const
{
    RELEASE_ASSERT(!isMainThread());
    return g_variant_new("(so)", m_atspi.uniqueName(), m_path.utf8().data());
}

AccessibilityObjectAtspi* AccessibilityRootAtspi::child() const
{
    RELEASE_ASSERT(isMainThread());
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    if (!m_page)
        return nullptr;

    Frame& frame = m_page->mainFrame();
    if (!frame.document())
        return nullptr;

    AXObjectCache* cache = frame.document()->axObjectCache();
    if (!cache)
        return nullptr;

    AXCoreObject* rootObject = cache->rootObject();
    if (!rootObject)
        return nullptr;

    auto* wrapper = rootObject->wrapper();
    if (!wrapper)
        return nullptr;

    wrapper->setRoot(const_cast<AccessibilityRootAtspi*>(this));
    wrapper->setParent(nullptr); // nullptr parent means root.

    return wrapper;
}

void AccessibilityRootAtspi::serialize(GVariantBuilder* builder) const
{
    RELEASE_ASSERT(!isMainThread());
    g_variant_builder_add(builder, "(so)", m_atspi.uniqueName(), m_path.utf8().data());
    g_variant_builder_add(builder, "(so)", m_parentUniqueName.utf8().data(), "/org/a11y/atspi/accessible/root");
    g_variant_builder_add(builder, "(so)", m_parentUniqueName.utf8().data(), m_parentPath.utf8().data());

    g_variant_builder_add(builder, "i", 0);
    g_variant_builder_add(builder, "i", Accessibility::retrieveValueFromMainThread<int32_t>([this]() -> int32_t {
        return child() ? 1 : 0;
    }));

    GVariantBuilder interfaces = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("as"));
    g_variant_builder_add(&interfaces, "s", webkit_accessible_interface.name);
    g_variant_builder_add(&interfaces, "s", webkit_component_interface.name);
    g_variant_builder_add(builder, "@as", g_variant_new("as", &interfaces));

    g_variant_builder_add(builder, "s", "");

    g_variant_builder_add(builder, "u", Atspi::Role::Filler);

    g_variant_builder_add(builder, "s", "");

    GVariantBuilder states = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("au"));
    uint64_t atspiStates = (G_GUINT64_CONSTANT(1) << Atspi::State::ManagesDescendants);
    g_variant_builder_add(&states, "u", static_cast<uint32_t>(atspiStates & 0xffffffff));
    g_variant_builder_add(&states, "u", static_cast<uint32_t>(atspiStates >> 32));
    g_variant_builder_add(builder, "@au", g_variant_builder_end(&states));
}

GDBusInterfaceVTable AccessibilityRootAtspi::s_componentFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        RELEASE_ASSERT(!isMainThread());
        auto& rootObject = *static_cast<AccessibilityRootAtspi*>(userData);
        if (!g_strcmp0(methodName, "Contains"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetAccessibleAtPoint"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetExtents")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = rootObject.frameRect(coordinateType);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((iiii))", rect.x(), rect.y(), rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetPosition")) {
            uint32_t coordinateType;
            g_variant_get(parameters, "(u)", &coordinateType);
            auto rect = rootObject.frameRect(coordinateType);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((ii))", rect.x(), rect.y()));
        } else if (!g_strcmp0(methodName, "GetSize")) {
            auto rect = rootObject.frameRect(Atspi::CoordinateType::ParentCoordinates);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("((ii))", rect.width(), rect.height()));
        } else if (!g_strcmp0(methodName, "GetLayer"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", Atspi::ComponentLayer::WidgetLayer));
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
    nullptr
};

IntRect AccessibilityRootAtspi::frameRect(uint32_t coordinateType) const
{
    RELEASE_ASSERT(!isMainThread());
    return Accessibility::retrieveValueFromMainThread<IntRect>([this, coordinateType]() -> IntRect {
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
    });
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
