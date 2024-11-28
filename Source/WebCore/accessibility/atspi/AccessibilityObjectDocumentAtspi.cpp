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

#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentType.h"
#include <gio/gio.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_documentFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetAttributeValue")) {
            const char* name;
            g_variant_get(parameters, "(&s)", &name);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", atspiObject->documentAttribute(String::fromUTF8(name)).utf8().data()));
        } else if (!g_strcmp0(methodName, "GetAttributes")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(a{ss})"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE("a{ss}"));
            auto attributes = atspiObject->documentAttributes();
            for (const auto& it : attributes)
                g_variant_builder_add(&builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
            g_variant_builder_close(&builder);
            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        } else if (!g_strcmp0(methodName, "GetLocale"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", atspiObject->documentLocale().utf8().data()));
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "CurrentPageNumber"))
            return g_variant_new_int32(-1);
        if (!g_strcmp0(propertyName, "PageCount"))
            return g_variant_new_int32(-1);

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

String AccessibilityObjectAtspi::documentAttribute(const String& name) const
{
    if (!m_coreObject)
        return { };

    auto* document = m_coreObject->document();
    if (!document)
        return { };

    if (name == "DocType"_s)
        return document->doctype() ? document->doctype()->name() : String();
    if (name == "Encoding"_s)
        return document->charset();
    if (name == "URI"_s)
        return document->documentURI();
    if (name == "MimeType"_s)
        return document->contentType();
    if (name == "Title"_s)
        return document->title();

    return { };
}

UncheckedKeyHashMap<String, String> AccessibilityObjectAtspi::documentAttributes() const
{
    UncheckedKeyHashMap<String, String> map;
    if (!m_coreObject)
        return map;

    auto* document = m_coreObject->document();
    if (!document)
        return map;

    if (auto* doctype = document->doctype())
        map.add("DocType"_s, doctype->name());

    auto charset = document->charset();
    if (!charset.isEmpty())
        map.add("Encoding"_s, WTFMove(charset));

    auto uri = document->documentURI();
    if (!uri.isEmpty())
        map.add("URI"_s, WTFMove(uri));

    auto contentType = document->contentType();
    if (!contentType.isEmpty())
        map.add("MimeType"_s, WTFMove(contentType));

    const auto& title = document->title();
    if (!title.isEmpty())
        map.add("Title"_s, title);

    return map;
}

String AccessibilityObjectAtspi::documentLocale() const
{
    if (!m_coreObject)
        return { };

    auto* document = m_coreObject->document();
    if (!document)
        return { };

    return document->contentLanguage();
}

} // namespace WebCore

#endif // USE(ATSPI)
