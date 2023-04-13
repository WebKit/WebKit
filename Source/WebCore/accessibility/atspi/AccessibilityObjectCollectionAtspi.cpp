/*
 * Copyright (C) 2022 Igalia S.L.
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

namespace WebCore {

GDBusInterfaceVTable AccessibilityObjectAtspi::s_collectionFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetMatches")) {
            GRefPtr<GVariant> rule;
            uint32_t sortyBy;
            int count;
            gboolean traverse;
            g_variant_get(parameters, "(@(aiia{ss}iaiiasib)uib)", &rule.outPtr(), &sortyBy, &count, &traverse);
            if (sortyBy > static_cast<uint32_t>(Atspi::CollectionSortOrder::SortOrderReverseTab)) {
                g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid sort order: %u", sortyBy);
                return;
            }
            CollectionMatchRule matchRule(rule.get());
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            for (const auto& wrapper : atspiObject->matches(matchRule, static_cast<Atspi::CollectionSortOrder>(sortyBy), std::max<int>(0, count), traverse))
                g_variant_builder_add(&builder, "@(so)", wrapper->reference());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(so))", &builder));
        } else if (!g_strcmp0(methodName, "GetMatchesTo") || !g_strcmp0(methodName, "GetMatchesFrom") || !g_strcmp0(methodName, "GetActiveDescendant"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    },
    // get_property
    nullptr,
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

AccessibilityObjectAtspi::CollectionMatchRule::CollectionMatchRule(GVariant* rule)
{
    GUniqueOutPtr<GVariantIter> statesIter;
    int statesMatchType;
    GUniqueOutPtr<GVariantIter> attributesIter;
    int attributesMatchType;
    GUniqueOutPtr<GVariantIter> rolesIter;
    int rolesMatchType;
    GUniqueOutPtr<GVariantIter> interfacesIter;
    int interfacesMatchType;
    gboolean inverted;
    g_variant_get(rule, "(aiia{ss}iaiiasib)", &statesIter.outPtr(), &statesMatchType, &attributesIter.outPtr(), &attributesMatchType, &rolesIter.outPtr(), &rolesMatchType, &interfacesIter.outPtr(), &interfacesMatchType, &inverted);

    int state;
    int i = 0;
    while (g_variant_iter_next(statesIter.get(), "i", &state)) {
        for (int j = 0; j < 32; ++j) {
            if (state & (1 << j))
                states.value.add(static_cast<Atspi::State>(1 << j << i * 32));
        }
        i++;
    }
    states.type = static_cast<Atspi::CollectionMatchType>(statesMatchType);

    const char* attributeName;
    const char* attributeValue;
    while (g_variant_iter_next(attributesIter.get(), "{&s&s}", &attributeName, &attributeValue)) {
        auto addResult = attributes.value.add(String::fromUTF8(attributeName), Vector<String> { });
        String value = String::fromUTF8(attributeValue);
        unsigned currentPos = 0;
        unsigned startPos = 0;
        size_t endPos;
        while ((endPos = value.find(':', currentPos)) != notFound) {
            if (currentPos != endPos) {
                if (startPos != endPos && ((endPos && value[endPos - 1] != '\\') || (endPos > 1 && value[endPos - 2] == '\\'))) {
                    auto unescapedValue = makeStringByReplacingAll(value.substring(startPos, endPos - startPos), "\\:"_s, ":"_s);
                    addResult.iterator->value.append(makeStringByReplacingAll(unescapedValue, "\\\\"_s, "\\"_s));
                    startPos = endPos + 1;
                }
            }
            currentPos = endPos + 1;
            if (startPos == endPos)
                startPos++;
        }
        if (startPos != value.length()) {
            auto unescapedValue = makeStringByReplacingAll(value.substring(startPos), "\\:"_s, ":"_s);
            addResult.iterator->value.append(makeStringByReplacingAll(unescapedValue, "\\\\"_s, "\\"_s));
        }
    }
    attributes.type = static_cast<Atspi::CollectionMatchType>(attributesMatchType);

    Atspi::Role role;
    i = 0;
    while (g_variant_iter_next(rolesIter.get(), "i", &role)) {
        for (int j = 0; j < 32; ++j) {
            if (static_cast<int>(role) & (1 << j))
                roles.value.append(static_cast<Atspi::Role>(i * 32 + j));
        }
        i++;
    }
    roles.type = static_cast<Atspi::CollectionMatchType>(rolesMatchType);

    const char* interface;
    while (g_variant_iter_next(interfacesIter.get(), "&s", &interface))
        interfaces.value.append(String::fromUTF8(interface));
    interfaces.type = static_cast<Atspi::CollectionMatchType>(interfacesMatchType);
}

bool AccessibilityObjectAtspi::CollectionMatchRule::matchInterfaces(AccessibilityObjectAtspi& axObject)
{
    if (interfaces.value.isEmpty())
        return true;

    auto matchInterface = [&](const String& interface) {
        if (interface == "accessible"_s)
            return axObject.interfaces().contains(Interface::Accessible);
        if (interface == "action"_s)
            return axObject.interfaces().contains(Interface::Action);
        if (interface == "component"_s)
            return axObject.interfaces().contains(Interface::Component);
        if (interface == "text"_s)
            return axObject.interfaces().contains(Interface::Text);
        if (interface == "hypertext"_s)
            return axObject.interfaces().contains(Interface::Hypertext);
        if (interface == "hyperlink"_s)
            return axObject.interfaces().contains(Interface::Hyperlink);
        if (interface == "image"_s)
            return axObject.interfaces().contains(Interface::Image);
        if (interface == "selection"_s)
            return axObject.interfaces().contains(Interface::Selection);
        if (interface == "table"_s)
            return axObject.interfaces().contains(Interface::Table);
        if (interface == "tablecell"_s)
            return axObject.interfaces().contains(Interface::TableCell);
        if (interface == "value"_s)
            return axObject.interfaces().contains(Interface::Value);
        if (interface == "document"_s)
            return axObject.interfaces().contains(Interface::Document);
        if (interface == "collection"_s)
            return axObject.interfaces().contains(Interface::Collection);
        return false;
    };

    switch (interfaces.type) {
    case Atspi::CollectionMatchType::MatchInvalid:
    case Atspi::CollectionMatchType::MatchEmpty:
        return false;
    case Atspi::CollectionMatchType::MatchAll:
        for (const auto& interface : interfaces.value) {
            if (!matchInterface(interface))
                return false;
        }

        return true;
    case Atspi::CollectionMatchType::MatchAny:
        for (const auto& interface : interfaces.value) {
            if (matchInterface(interface))
                return true;
        }

        return false;
    case Atspi::CollectionMatchType::MatchNone:
        for (const auto& interface : interfaces.value) {
            if (matchInterface(interface))
                return false;
        }

        return true;
    }

    return false;
}

bool AccessibilityObjectAtspi::CollectionMatchRule::matchStates(AccessibilityObjectAtspi& axObject)
{
    if (states.value.isEmpty())
        return true;

    switch (states.type) {
    case Atspi::CollectionMatchType::MatchInvalid:
    case Atspi::CollectionMatchType::MatchEmpty:
        return false;
    case Atspi::CollectionMatchType::MatchAll:
        return axObject.states().containsAll(states.value);
    case Atspi::CollectionMatchType::MatchAny:
        return axObject.states().containsAny(states.value);
    case Atspi::CollectionMatchType::MatchNone:
        return !axObject.states().containsAny(states.value);
    }

    return false;
}

bool AccessibilityObjectAtspi::CollectionMatchRule::matchRoles(AccessibilityObjectAtspi& axObject)
{
    if (roles.value.isEmpty())
        return true;

    switch (roles.type) {
    case Atspi::CollectionMatchType::MatchInvalid:
    case Atspi::CollectionMatchType::MatchEmpty:
        return false;
    case Atspi::CollectionMatchType::MatchAll:
        if (roles.value.size() != 1)
            return false;

        return roles.value[0] == axObject.role();
    case Atspi::CollectionMatchType::MatchAny:
        for (auto role : roles.value) {
            if (role == axObject.role())
                return true;
        }

        return false;
    case Atspi::CollectionMatchType::MatchNone:
        for (auto role : roles.value) {
            if (role == axObject.role())
                return false;
        }

        return true;
    }

    return false;
}

bool AccessibilityObjectAtspi::CollectionMatchRule::matchAttributes(AccessibilityObjectAtspi& axObject)
{
    if (attributes.value.isEmpty())
        return true;

    switch (attributes.type) {
    case Atspi::CollectionMatchType::MatchInvalid:
    case Atspi::CollectionMatchType::MatchEmpty:
        return false;
    case Atspi::CollectionMatchType::MatchAll: {
        auto axAttributes = axObject.attributes();
        for (const auto& it : attributes.value) {
            auto value = axAttributes.get(it.key);
            if (value.isNull())
                return false;

            if (it.value.isEmpty() || it.value.size() > 1)
                return false;

            if (!equalIgnoringASCIICase(it.value[0], value))
                return false;
        }

        return true;
    }
    case Atspi::CollectionMatchType::MatchAny: {
        auto axAttributes = axObject.attributes();
        for (const auto& it : attributes.value) {
            auto value = axAttributes.get(it.key);
            if (value.isNull())
                continue;

            bool found = it.value.findIf([&value](auto& item) {
                return equalIgnoringASCIICase(item, value);
            }) != notFound;
            if (found)
                return true;
        }

        return false;
    }

    case Atspi::CollectionMatchType::MatchNone: {
        auto axAttributes = axObject.attributes();
        for (const auto& it : attributes.value) {
            auto value = axAttributes.get(it.key);
            if (value.isNull())
                continue;

            bool found = it.value.findIf([&value](auto& item) {
                return equalIgnoringASCIICase(item, value);
            }) != notFound;
            if (found)
                return false;
        }

        return true;
    }
    }

    return false;
}

bool AccessibilityObjectAtspi::CollectionMatchRule::match(AccessibilityObjectAtspi& axObject)
{
    if (!matchInterfaces(axObject))
        return false;

    if (!matchStates(axObject))
        return false;

    if (!matchRoles(axObject))
        return false;

    if (!matchAttributes(axObject))
        return false;

    return true;
}

void AccessibilityObjectAtspi::addMatchesInCanonicalOrder(Vector<RefPtr<AccessibilityObjectAtspi>>& matchList, CollectionMatchRule& rule, uint32_t maxResultCount, bool traverse)
{
    const auto& children = m_coreObject->children();
    for (auto& child : children) {
        auto* wrapper = child->wrapper();
        if (!wrapper)
            continue;

        if (rule.match(*wrapper)) {
            matchList.append(wrapper);
            if (maxResultCount && matchList.size() >= maxResultCount)
                return;
        }

        if (traverse) {
            wrapper->addMatchesInCanonicalOrder(matchList, rule, maxResultCount, traverse);
            if (maxResultCount && matchList.size() >= maxResultCount)
                return;
        }
    }
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::matches(CollectionMatchRule& rule, Atspi::CollectionSortOrder sortOrder, uint32_t maxResultCount, bool traverse)
{
    if (!m_coreObject)
        return { };

    Vector<RefPtr<AccessibilityObjectAtspi>> matchList;

    switch (sortOrder) {
    case Atspi::CollectionSortOrder::SortOrderInvalid:
        break;
    case Atspi::CollectionSortOrder::SortOrderCanonical:
        addMatchesInCanonicalOrder(matchList, rule, maxResultCount, traverse);
        break;
    case Atspi::CollectionSortOrder::SortOrderReverseCanonical:
        addMatchesInCanonicalOrder(matchList, rule, maxResultCount, traverse);
        matchList.reverse();
        break;
    case Atspi::CollectionSortOrder::SortOrderFlow:
    case Atspi::CollectionSortOrder::SortOrderTab:
    case Atspi::CollectionSortOrder::SortOrderReverseFlow:
    case Atspi::CollectionSortOrder::SortOrderReverseTab:
        g_warning("Atspi collection sort method %u not implemented yet", static_cast<uint32_t>(sortOrder));
        break;

    }

    return matchList;
}

} // namespace WebCore

#endif // USE(ATSPI)
