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
#include "AXObjectCache.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityAtspiInterfaces.h"
#include "AccessibilityObjectInterface.h"
#include "AccessibilityRootAtspi.h"
#include "AccessibilityTableCell.h"
#include "ElementInlines.h"
#include "HTMLSpanElement.h"
#include "RenderAncestorIterator.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderObject.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include <glib/gi18n-lib.h>
#include <wtf/UUID.h>

namespace WebCore {

Ref<AccessibilityObjectAtspi> AccessibilityObjectAtspi::create(AXCoreObject* coreObject, AccessibilityRootAtspi* root)
{
    return adoptRef(*new AccessibilityObjectAtspi(coreObject, root));
}

static inline bool roleIsTextType(AccessibilityRole role)
{
    return role == AccessibilityRole::Paragraph
        || role == AccessibilityRole::Heading
        || role == AccessibilityRole::Div
        || role == AccessibilityRole::Cell
        || role == AccessibilityRole::Link
        || role == AccessibilityRole::WebCoreLink
        || role == AccessibilityRole::ListItem
        || role == AccessibilityRole::Pre
        || role == AccessibilityRole::GridCell
        || role == AccessibilityRole::TextGroup
        || role == AccessibilityRole::ApplicationTextGroup
        || role == AccessibilityRole::ApplicationGroup;
}

OptionSet<AccessibilityObjectAtspi::Interface> AccessibilityObjectAtspi::interfacesForObject(AXCoreObject& coreObject)
{
    OptionSet<Interface> interfaces = { Interface::Accessible, Interface::Component, Interface::Action, Interface::Collection };

    RenderObject* renderer = coreObject.isAccessibilityRenderObject() ? coreObject.renderer() : nullptr;
    if (coreObject.roleValue() == AccessibilityRole::StaticText || coreObject.roleValue() == AccessibilityRole::ColorWell)
        interfaces.add(Interface::Text);
    else if (coreObject.isTextControl() || coreObject.isNonNativeTextControl())
        interfaces.add(Interface::Text);
    else if (!coreObject.isWebArea()) {
        if (coreObject.roleValue() != AccessibilityRole::Table) {
            interfaces.add(Interface::Hypertext);
            if ((renderer && renderer->childrenInline()) || roleIsTextType(coreObject.roleValue()) || coreObject.isMathToken())
                interfaces.add(Interface::Text);
        }
    }

    if (coreObject.supportsRangeValue())
        interfaces.add(Interface::Value);

    if (coreObject.isLink() || (isRendererReplacedElement(renderer)))
        interfaces.add(Interface::Hyperlink);

    if (coreObject.roleValue() == AccessibilityRole::WebArea)
        interfaces.add(Interface::Document);

    if (coreObject.isImage())
        interfaces.add(Interface::Image);

    if (coreObject.canHaveSelectedChildren())
        interfaces.add(Interface::Selection);

    if (coreObject.isTable())
        interfaces.add(Interface::Table);

    if (coreObject.roleValue() == AccessibilityRole::Cell
        || coreObject.roleValue() == AccessibilityRole::GridCell
        || coreObject.roleValue() == AccessibilityRole::ColumnHeader
        || coreObject.roleValue() == AccessibilityRole::RowHeader)
        interfaces.add(Interface::TableCell);

    if (coreObject.roleValue() == AccessibilityRole::ListMarker && renderer) {
        if (renderer->isImage())
            interfaces.add(Interface::Image);
        else
            interfaces.add(Interface::Text);
        interfaces.add(Interface::Hyperlink);
    }

    return interfaces;
}

AccessibilityObjectAtspi::AccessibilityObjectAtspi(AXCoreObject* coreObject, AccessibilityRootAtspi* root)
    : m_coreObject(coreObject)
    , m_interfaces(interfacesForObject(*m_coreObject))
    , m_root(root)
{
}

void AccessibilityObjectAtspi::cacheDestroyed()
{
    m_coreObject = nullptr;
    if (!m_isRegistered)
        return;

    if (m_parent && !*m_parent && m_root)
        m_root->childRemoved(*this);

    AccessibilityAtspi::singleton().unregisterObject(*this);
}

void AccessibilityObjectAtspi::elementDestroyed()
{
    m_coreObject = nullptr;
    if (!m_isRegistered)
        return;

    if (m_parent) {
        if (*m_parent)
            m_parent.value()->childRemoved(*this);
        else if (m_root)
            m_root->childRemoved(*this);
    }

    AccessibilityAtspi::singleton().unregisterObject(*this);
}

static unsigned atspiRole(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Annotation:
    case AccessibilityRole::ApplicationAlert:
        return Atspi::Role::Notification;
    case AccessibilityRole::ApplicationAlertDialog:
        return Atspi::Role::Alert;
    case AccessibilityRole::ApplicationDialog:
        return Atspi::Role::Dialog;
    case AccessibilityRole::ApplicationStatus:
        return Atspi::Role::StatusBar;
    case AccessibilityRole::Unknown:
        return Atspi::Role::Unknown;
    case AccessibilityRole::Audio:
        return Atspi::Role::Audio;
    case AccessibilityRole::Video:
        return Atspi::Role::Video;
    case AccessibilityRole::Button:
        return Atspi::Role::PushButton;
    case AccessibilityRole::Switch:
    case AccessibilityRole::ToggleButton:
        return Atspi::Role::ToggleButton;
    case AccessibilityRole::RadioButton:
        return Atspi::Role::RadioButton;
    case AccessibilityRole::CheckBox:
        return Atspi::Role::CheckBox;
    case AccessibilityRole::Slider:
        return Atspi::Role::Slider;
    case AccessibilityRole::TabGroup:
    case AccessibilityRole::TabList:
        return Atspi::Role::PageTabList;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::SearchField:
        return Atspi::Role::Entry;
    case AccessibilityRole::StaticText:
        return Atspi::Role::Static;
    case AccessibilityRole::Outline:
    case AccessibilityRole::Tree:
        return Atspi::Role::Tree;
    case AccessibilityRole::TreeItem:
        return Atspi::Role::TreeItem;
    case AccessibilityRole::MenuBar:
        return Atspi::Role::MenuBar;
    case AccessibilityRole::MenuListPopup:
    case AccessibilityRole::Menu:
        return Atspi::Role::Menu;
    case AccessibilityRole::MenuListOption:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuButton:
        return Atspi::Role::MenuItem;
    case AccessibilityRole::MenuItemCheckbox:
        return Atspi::Role::CheckMenuItem;
    case AccessibilityRole::MenuItemRadio:
        return Atspi::Role::RadioMenuItem;
    case AccessibilityRole::Column:
        return Atspi::Role::Unknown; // There's no TableColumn in atspi.
    case AccessibilityRole::Row:
        return Atspi::Role::TableRow;
    case AccessibilityRole::Toolbar:
        return Atspi::Role::ToolBar;
    case AccessibilityRole::Meter:
        return Atspi::Role::LevelBar;
    case AccessibilityRole::BusyIndicator:
    case AccessibilityRole::ProgressIndicator:
        return Atspi::Role::ProgressBar;
    case AccessibilityRole::Window:
        return Atspi::Role::Window;
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::ComboBox:
        return Atspi::Role::ComboBox;
    case AccessibilityRole::SplitGroup:
        return Atspi::Role::SplitPane;
    case AccessibilityRole::Splitter:
        return Atspi::Role::Separator;
    case AccessibilityRole::ColorWell:
        return Atspi::Role::PushButton;
    case AccessibilityRole::List:
        return Atspi::Role::List;
    case AccessibilityRole::ScrollBar:
        return Atspi::Role::ScrollBar;
    case AccessibilityRole::ScrollArea:
    case AccessibilityRole::TabPanel:
        return Atspi::Role::ScrollPane;
    case AccessibilityRole::Grid:
    case AccessibilityRole::Table:
        return Atspi::Role::Table;
    case AccessibilityRole::TreeGrid:
        return Atspi::Role::TreeTable;
    case AccessibilityRole::Application:
        return Atspi::Role::Application;
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::Feed:
    case AccessibilityRole::Figure:
    case AccessibilityRole::GraphicsObject:
    case AccessibilityRole::Group:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::SVGRoot:
        return Atspi::Role::Panel;
    case AccessibilityRole::RowHeader:
        return Atspi::Role::RowHeader;
    case AccessibilityRole::ColumnHeader:
        return Atspi::Role::ColumnHeader;
    case AccessibilityRole::Caption:
        return Atspi::Role::Caption;
    case AccessibilityRole::Cell:
    case AccessibilityRole::GridCell:
        return Atspi::Role::TableCell;
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
    case AccessibilityRole::ImageMapLink:
        return Atspi::Role::Link;
    case AccessibilityRole::ImageMap:
        return Atspi::Role::ImageMap;
    case AccessibilityRole::GraphicsSymbol:
    case AccessibilityRole::Image:
        return Atspi::Role::Image;
    case AccessibilityRole::DocumentArticle:
        return Atspi::Role::Article;
    case AccessibilityRole::Document:
    case AccessibilityRole::GraphicsDocument:
        return Atspi::Role::DocumentFrame;
    case AccessibilityRole::DocumentNote:
        return Atspi::Role::Comment;
    case AccessibilityRole::Heading:
        return Atspi::Role::Heading;
    case AccessibilityRole::ListBox:
        return Atspi::Role::ListBox;
    case AccessibilityRole::ListItem:
    case AccessibilityRole::ListBoxOption:
        return Atspi::Role::ListItem;
    case AccessibilityRole::Paragraph:
        return Atspi::Role::Paragraph;
    case AccessibilityRole::Label:
    case AccessibilityRole::Legend:
        return Atspi::Role::Label;
    case AccessibilityRole::Blockquote:
        return Atspi::Role::BlockQuote;
    case AccessibilityRole::Footnote:
        return Atspi::Role::Footnote;
    case AccessibilityRole::ApplicationTextGroup:
    case AccessibilityRole::Div:
    case AccessibilityRole::Pre:
    case AccessibilityRole::SVGText:
    case AccessibilityRole::TextGroup:
        return Atspi::Role::Section;
    case AccessibilityRole::Footer:
        return Atspi::Role::Footer;
    case AccessibilityRole::Form:
        return Atspi::Role::Form;
    case AccessibilityRole::Canvas:
        return Atspi::Role::Canvas;
    case AccessibilityRole::HorizontalRule:
        return Atspi::Role::Separator;
    case AccessibilityRole::SpinButton:
        return Atspi::Role::SpinButton;
    case AccessibilityRole::Tab:
        return Atspi::Role::PageTab;
    case AccessibilityRole::UserInterfaceTooltip:
        return Atspi::Role::ToolTip;
    case AccessibilityRole::WebArea:
        return Atspi::Role::DocumentWeb;
    case AccessibilityRole::WebApplication:
        return Atspi::Role::Embedded;
    case AccessibilityRole::ApplicationLog:
        return Atspi::Role::Log;
    case AccessibilityRole::ApplicationMarquee:
        return Atspi::Role::Marquee;
    case AccessibilityRole::ApplicationTimer:
        return Atspi::Role::Timer;
    case AccessibilityRole::Definition:
        return Atspi::Role::Definition;
    case AccessibilityRole::DocumentMath:
        return Atspi::Role::Math;
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
        return Atspi::Role::Landmark;
    case AccessibilityRole::DescriptionList:
        return Atspi::Role::DescriptionList;
    case AccessibilityRole::Term:
    case AccessibilityRole::DescriptionListTerm:
        return Atspi::Role::DescriptionTerm;
    case AccessibilityRole::DescriptionListDetail:
        return Atspi::Role::DescriptionValue;
    case AccessibilityRole::Deletion:
        return Atspi::Role::ContentDeletion;
    case AccessibilityRole::Insertion:
        return Atspi::Role::ContentInsertion;
    case AccessibilityRole::Subscript:
        return Atspi::Role::Subscript;
    case AccessibilityRole::Superscript:
        return Atspi::Role::Superscript;
    case AccessibilityRole::Inline:
    case AccessibilityRole::SVGTextPath:
    case AccessibilityRole::SVGTSpan:
    case AccessibilityRole::Time:
        return Atspi::Role::Static;
    case AccessibilityRole::Directory:
        return Atspi::Role::DirectoryPane;
    case AccessibilityRole::Mark:
        return Atspi::Role::Mark;
    case AccessibilityRole::Browser:
    case AccessibilityRole::Details:
    case AccessibilityRole::DisclosureTriangle:
    case AccessibilityRole::Drawer:
    case AccessibilityRole::EditableText:
    case AccessibilityRole::GrowArea:
    case AccessibilityRole::HelpTag:
    case AccessibilityRole::Ignored:
    case AccessibilityRole::Incrementor:
    case AccessibilityRole::Matte:
    case AccessibilityRole::Model:
    case AccessibilityRole::Presentational:
    case AccessibilityRole::RowGroup:
    case AccessibilityRole::RubyBase:
    case AccessibilityRole::RubyBlock:
    case AccessibilityRole::RubyInline:
    case AccessibilityRole::RubyRun:
    case AccessibilityRole::RubyText:
    case AccessibilityRole::Ruler:
    case AccessibilityRole::RulerMarker:
    case AccessibilityRole::Sheet:
    case AccessibilityRole::SliderThumb:
    case AccessibilityRole::SpinButtonPart:
    case AccessibilityRole::Summary:
    case AccessibilityRole::SystemWide:
    case AccessibilityRole::TableHeaderContainer:
    case AccessibilityRole::ValueIndicator:
    case AccessibilityRole::Suggestion:
        return Atspi::Role::Unknown;
    // Add most new roles above. The release assert is for roles that are handled specially.
    case AccessibilityRole::ListMarker:
    case AccessibilityRole::MathElement:
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GDBusInterfaceVTable AccessibilityObjectAtspi::s_accessibleFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetRole"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", atspiObject->role()));
        else if (!g_strcmp0(methodName, "GetRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", atspiObject->roleName().utf8().data()));
        else if (!g_strcmp0(methodName, "GetLocalizedRoleName"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", atspiObject->localizedRoleName()));
        else if (!g_strcmp0(methodName, "GetState")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(au)"));

            auto states = atspiObject->state();
            g_variant_builder_open(&builder, G_VARIANT_TYPE("au"));
            g_variant_builder_add(&builder, "u", static_cast<uint32_t>(states & 0xffffffff));
            g_variant_builder_add(&builder, "u", static_cast<uint32_t>(states >> 32));
            g_variant_builder_close(&builder);

            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        } else if (!g_strcmp0(methodName, "GetAttributes")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(a{ss})"));

            g_variant_builder_open(&builder, G_VARIANT_TYPE("a{ss}"));
            atspiObject->buildAttributes(&builder);
            g_variant_builder_close(&builder);

            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        } else if (!g_strcmp0(methodName, "GetApplication"))
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", AccessibilityAtspi::singleton().applicationReference()));
        else if (!g_strcmp0(methodName, "GetChildAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            auto* wrapper = index >= 0 ? atspiObject->childAt(index) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", wrapper ? wrapper->reference() : AccessibilityAtspi::singleton().nullReference()));
        } else if (!g_strcmp0(methodName, "GetChildren")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
            for (const auto& wrapper : atspiObject->children())
                g_variant_builder_add(&builder, "@(so)", wrapper->reference());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(so))", &builder));
        } else if (!g_strcmp0(methodName, "GetIndexInParent")) {
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", atspiObject->indexInParent()));
        } else if (!g_strcmp0(methodName, "GetRelationSet")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(ua(so))"));
            atspiObject->buildRelationSet(&builder);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a(ua(so)))", &builder));
        } else if (!g_strcmp0(methodName, "GetInterfaces")) {
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("as"));
            atspiObject->buildInterfaces(&builder);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", &builder));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "Name"))
            return g_variant_new_string(atspiObject->name().data());
        if (!g_strcmp0(propertyName, "Description"))
            return g_variant_new_string(atspiObject->description().data());
        if (!g_strcmp0(propertyName, "Locale"))
            return g_variant_new_string(atspiObject->locale().utf8().data());
        if (!g_strcmp0(propertyName, "AccessibleId"))
            return g_variant_new_string(atspiObject->m_coreObject ? String::number(atspiObject->m_coreObject->objectID().toUInt64()).utf8().data() : "");
        if (!g_strcmp0(propertyName, "Parent"))
            return atspiObject->parentReference();
        if (!g_strcmp0(propertyName, "ChildCount"))
            return g_variant_new_int32(atspiObject->childCount());

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

bool AccessibilityObjectAtspi::registerObject()
{
    if (!m_path.isNull())
        return false;

    m_isRegistered = true;
    Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>> interfaces;
    if (m_interfaces.contains(Interface::Accessible))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_accessible_interface), &s_accessibleFunctions });
    if (m_interfaces.contains(Interface::Component))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_component_interface), &s_componentFunctions });
    if (m_interfaces.contains(Interface::Text))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_text_interface), &s_textFunctions });
    if (m_interfaces.contains(Interface::Value))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_value_interface), &s_valueFunctions });
    if (m_interfaces.contains(Interface::Hyperlink))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_hyperlink_interface), &s_hyperlinkFunctions });
    if (m_interfaces.contains(Interface::Hypertext))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_hypertext_interface), &s_hypertextFunctions });
    if (m_interfaces.contains(Interface::Action))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_action_interface), &s_actionFunctions });
    if (m_interfaces.contains(Interface::Document))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_document_interface), &s_documentFunctions });
    if (m_interfaces.contains(Interface::Image))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_image_interface), &s_imageFunctions });
    if (m_interfaces.contains(Interface::Selection))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_selection_interface), &s_selectionFunctions });
    if (m_interfaces.contains(Interface::Table))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_table_interface), &s_tableFunctions });
    if (m_interfaces.contains(Interface::TableCell))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_table_cell_interface), &s_tableCellFunctions });
    if (m_interfaces.contains(Interface::Collection))
        interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_collection_interface), &s_collectionFunctions });

    m_path = AccessibilityAtspi::singleton().registerObject(*this, WTFMove(interfaces));
    return true;
}

void AccessibilityObjectAtspi::didUnregisterObject()
{
    m_isRegistered = false;
    m_path = { };
}

const String& AccessibilityObjectAtspi::path()
{
    registerObject();
    return m_path;
}

GVariant* AccessibilityObjectAtspi::reference()
{
    return g_variant_new("(so)", AccessibilityAtspi::singleton().uniqueName(), path().utf8().data());
}

GVariant* AccessibilityObjectAtspi::hyperlinkReference()
{
    if (m_hyperlinkPath.isNull()) {
        registerObject();
        m_hyperlinkPath = AccessibilityAtspi::singleton().registerHyperlink(*this, { { const_cast<GDBusInterfaceInfo*>(&webkit_hyperlink_interface), &s_hyperlinkFunctions } });
    }

    return g_variant_new("(so)", AccessibilityAtspi::singleton().uniqueName(), m_hyperlinkPath.utf8().data());
}

void AccessibilityObjectAtspi::setParent(std::optional<AccessibilityObjectAtspi*> atspiParent)
{
    if (m_parent == atspiParent)
        return;

    m_parent = atspiParent;
    if (!m_coreObject || m_coreObject->accessibilityIsIgnored())
        return;

    AccessibilityAtspi::singleton().parentChanged(*this);
    if (m_parent) {
        if (*m_parent)
            m_parent.value()->childAdded(*this);
        else if (m_root)
            m_root->childAdded(*this);
    }
}

std::optional<AccessibilityObjectAtspi*> AccessibilityObjectAtspi::parent() const
{
    if (!m_coreObject)
        return std::nullopt;

    auto* axParent = m_coreObject->parentObjectUnignored();
    if (!axParent)
        return nullptr;

    if (auto* atspiParent = axParent->wrapper())
        return atspiParent;

    return std::nullopt;
}

GVariant* AccessibilityObjectAtspi::parentReference() const
{
    if (m_parent) {
        if (*m_parent)
            return m_parent.value()->reference();

        if (m_root)
            return m_root->reference();
    }

    return AccessibilityAtspi::singleton().nullReference();
}

unsigned AccessibilityObjectAtspi::childCount() const
{
    return m_coreObject ? m_coreObject->children().size() : 0;
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::childAt(unsigned index) const
{
    if (!m_coreObject)
        return nullptr;

    const auto& children = m_coreObject->children();
    if (index >= children.size())
        return nullptr;

    return children[index]->wrapper();
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::wrapperVector(const Vector<RefPtr<AXCoreObject>>& elements) const
{
    Vector<RefPtr<AccessibilityObjectAtspi>> wrappers;
    wrappers.reserveInitialCapacity(elements.size());
    for (const auto& element : elements) {
        if (auto* wrapper = element->wrapper())
            wrappers.uncheckedAppend(wrapper);
    }
    return wrappers;
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::children() const
{
    if (!m_coreObject)
        return { };

    return wrapperVector(m_coreObject->children());
}

int AccessibilityObjectAtspi::indexInParent() const
{
    if (!m_coreObject) {
        m_indexInParent = -1;
        return m_indexInParent;
    }

    auto* axParent = m_coreObject->parentObjectUnignored();
    if (!axParent) {
        m_indexInParent = 0;
        return m_indexInParent;
    }

    const auto& children = axParent->children();
    unsigned index = 0;
    for (const auto& child : children) {
        if (child.get() == m_coreObject) {
            m_indexInParent = index;
            return m_indexInParent;
        }
        index++;
    }

    m_indexInParent = -1;
    return m_indexInParent;
}

int AccessibilityObjectAtspi::indexInParentForChildrenChanged(AccessibilityAtspi::ChildrenChanged change)
{
    if (change == AccessibilityAtspi::ChildrenChanged::Removed)
        return m_indexInParent;

    updateBackingStore();
    return indexInParent();
}

CString AccessibilityObjectAtspi::name() const
{
    if (!m_coreObject)
        return "";

    if (m_coreObject->roleValue() == AccessibilityRole::ListBoxOption || m_coreObject->roleValue() == AccessibilityRole::MenuListOption) {
        auto value = m_coreObject->stringValue();
        if (!value.isEmpty())
            return value.utf8();
    }

    Vector<AccessibilityText> textOrder;
    m_coreObject->accessibilityText(textOrder);

    for (const auto& text : textOrder) {
        // FIXME: This check is here because AccessibilityNodeObject::titleElementText()
        // appends an empty String for the LabelByElementText source when there is a
        // titleUIElement(). Removing this check makes some fieldsets lose their name.
        if (text.text.isEmpty())
            continue;

        // WebCore Accessibility should provide us with the text alternative computation
        // in the order defined by that spec. So take the first thing that our platform
        // does not expose via the AtkObject description.
        if (text.textSource != AccessibilityTextSource::Help && text.textSource != AccessibilityTextSource::Summary)
            return text.text.utf8();
    }

    return "";
}

CString AccessibilityObjectAtspi::description() const
{
    if (!m_coreObject)
        return "";

    Vector<AccessibilityText> textOrder;
    m_coreObject->accessibilityText(textOrder);

    bool nameTextAvailable = false;
    for (const auto& text : textOrder) {
        // WebCore Accessibility should provide us with the text alternative computation
        // in the order defined by that spec. So take the first thing that our platform
        // does not expose via the AtkObject name.
        if (text.textSource == AccessibilityTextSource::Help || text.textSource == AccessibilityTextSource::Summary)
            return text.text.utf8();

        // If there is no other text alternative, the title tag contents will have been
        // used for the AtkObject name. We don't want to duplicate it here.
        if (text.textSource == AccessibilityTextSource::TitleTag && nameTextAvailable)
            return text.text.utf8();

        nameTextAvailable = true;
    }

    return "";
}

String AccessibilityObjectAtspi::locale() const
{
    return m_coreObject ? m_coreObject->language() : String();
}

static bool shouldIncludeOrientationState(const AXCoreObject& coreObject)
{
    return coreObject.isComboBox()
        || coreObject.isRadioGroup()
        || coreObject.isTreeGrid()
        || coreObject.isScrollbar()
        || coreObject.isListBox()
        || coreObject.isMenu()
        || coreObject.isTree()
        || coreObject.isMenuBar()
        || coreObject.isSplitter()
        || coreObject.isTabList()
        || coreObject.isToolbar()
        || coreObject.isSlider();
}

uint64_t AccessibilityObjectAtspi::state() const
{
    uint64_t states = 0;

    auto addState = [&](Atspi::State atspiState) {
        states |= (G_GUINT64_CONSTANT(1) << atspiState);
    };

    if (!m_coreObject) {
        addState(Atspi::State::Defunct);
        return states;
    }

    auto* liveObject = dynamicDowncast<AccessibilityObject>(m_coreObject);

    if (m_coreObject->isEnabled()) {
        addState(Atspi::State::Enabled);
        addState(Atspi::State::Sensitive);
    }

    if (m_coreObject->isVisible()) {
        addState(Atspi::State::Visible);
        if (!m_coreObject->isOffScreen())
            addState(Atspi::State::Showing);
    }

    if (m_coreObject->isSelectedOptionActive() || m_coreObject->currentState() != AccessibilityCurrentState::False)
        addState(Atspi::State::Active);

    if (m_coreObject->canSetFocusAttribute())
        addState(Atspi::State::Focusable);

    if (m_coreObject->isFocused() && !m_coreObject->activeDescendant())
        addState(Atspi::State::Focused);
    else if (liveObject->isActiveDescendantOfFocusedContainer()) {
        addState(Atspi::State::Focusable);
        addState(Atspi::State::Focused);
    }

    if (m_coreObject->canSetValueAttribute()) {
        if (m_coreObject->supportsChecked())
            addState(Atspi::State::Checkable);

        if (m_coreObject->isTextControl() || m_coreObject->isNonNativeTextControl())
            addState(Atspi::State::Editable);
    } else if (liveObject && liveObject->supportsReadOnly())
        addState(Atspi::State::ReadOnly);

    if (m_coreObject->isChecked())
        addState(Atspi::State::Checked);

    if (m_coreObject->isPressed())
        addState(Atspi::State::Pressed);

    if (m_coreObject->isRequired())
        addState(Atspi::State::Required);

    if (m_coreObject->roleValue() == AccessibilityRole::TextArea || m_coreObject->ariaIsMultiline())
        addState(Atspi::State::MultiLine);
    else if (m_coreObject->roleValue() == AccessibilityRole::TextField || m_coreObject->roleValue() == AccessibilityRole::SearchField)
        addState(Atspi::State::SingleLine);

    if (m_coreObject->isTextControl())
        addState(Atspi::State::SelectableText);

    if (m_coreObject->canSetSelectedAttribute())
        addState(Atspi::State::Selectable);

    if (m_coreObject->isMultiSelectable())
        addState(Atspi::State::Multiselectable);

    if (m_coreObject->isSelected())
        addState(Atspi::State::Selected);

    if (m_coreObject->canSetExpandedAttribute())
        addState(Atspi::State::Expandable);

    if (m_coreObject->isExpanded())
        addState(Atspi::State::Expanded);

    if (m_coreObject->hasPopup())
        addState(Atspi::State::HasPopup);

    if (shouldIncludeOrientationState(*m_coreObject)) {
        switch (m_coreObject->orientation()) {
        case AccessibilityOrientation::Horizontal:
            addState(Atspi::State::Horizontal);
            break;
        case AccessibilityOrientation::Vertical:
            addState(Atspi::State::Vertical);
            break;
        case AccessibilityOrientation::Undefined:
            break;
        }
    }

    if (m_coreObject->isIndeterminate())
        addState(Atspi::State::Indeterminate);
    else if ((m_coreObject->isCheckboxOrRadio() || m_coreObject->isMenuItem() || m_coreObject->isToggleButton()) && m_coreObject->checkboxOrRadioValue() == AccessibilityButtonState::Mixed)
        addState(Atspi::State::Indeterminate);

    if (m_coreObject->isModalNode())
        addState(Atspi::State::Modal);

    if (m_coreObject->isBusy())
        addState(Atspi::State::Busy);

    if (m_coreObject->invalidStatus() != "false"_s)
        addState(Atspi::State::InvalidEntry);

    if (liveObject && liveObject->supportsAutoComplete() && liveObject->autoCompleteValue() != "none"_s)
        addState(Atspi::State::SupportsAutocompletion);

    return states;
}

bool AccessibilityObjectAtspi::isDefunct() const
{
    return !m_coreObject;
}

String AccessibilityObjectAtspi::id() const
{
    if (!m_coreObject)
        return { };

    if (auto* element = m_coreObject->element())
        return element->getIdAttribute().string();

    return { };
}

HashMap<String, String> AccessibilityObjectAtspi::attributes() const
{
    HashMap<String, String> map;
#if PLATFORM(GTK)
    map.add("toolkit"_s, "WebKitGTK"_s);
#elif PLATFORM(WPE)
    map.add("toolkit"_s, "WPEWebKit"_s);
#endif
    if (!m_coreObject)
        return map;

    auto* liveObject = dynamicDowncast<AccessibilityObject>(m_coreObject);

    String tagName = m_coreObject->tagName();
    if (!tagName.isEmpty())
        map.add("tag"_s, tagName);

    if (auto* element = m_coreObject->element()) {
        String id = element->getIdAttribute().string();
        if (!id.isEmpty())
            map.add("id"_s, id);
    }

    int level = m_coreObject->isHeading() ? m_coreObject->headingLevel() : m_coreObject->hierarchicalLevel();
    if (level)
        map.add("level"_s, String::number(level));

    int rowCount = m_coreObject->axRowCount();
    if (rowCount)
        map.add("rowcount"_s, String::number(rowCount));

    int columnCount = m_coreObject->axColumnCount();
    if (columnCount)
        map.add("colcount"_s, String::number(columnCount));

    int rowIndex = m_coreObject->axRowIndex();
    if (rowIndex != -1)
        map.add("rowindex"_s, String::number(rowIndex));

    int columnIndex = m_coreObject->axColumnIndex();
    if (columnIndex != -1)
        map.add("colindex"_s, String::number(columnIndex));

    if (is<AccessibilityTableCell>(m_coreObject)) {
        auto& cell = downcast<AccessibilityTableCell>(*m_coreObject);
        int rowSpan = cell.axRowSpan();
        if (rowSpan != -1)
            map.add("rowspan"_s, String::number(rowSpan));

        int columnSpan = cell.axColumnSpan();
        if (columnSpan != -1)
            map.add("colspan"_s, String::number(columnSpan));
    }

    String placeholder = m_coreObject->placeholderValue();
    if (!placeholder.isEmpty())
        map.add("placeholder-text"_s, placeholder);

    if (liveObject && liveObject->supportsAutoComplete())
        map.add("autocomplete"_s, m_coreObject->autoCompleteValue());

    if (m_coreObject->supportsHasPopup())
        map.add("haspopup"_s, m_coreObject->popupValue());

    if (m_coreObject->supportsCurrent())
        map.add("current"_s, m_coreObject->currentValue());

    if (m_coreObject->supportsPosInSet())
        map.add("posinset"_s, String::number(m_coreObject->posInSet()));

    if (m_coreObject->supportsSetSize())
        map.add("setsize"_s, String::number(m_coreObject->setSize()));

    // The Core AAM states that an explicitly-set value should be exposed, including "none".
    if (static_cast<AccessibilityObject*>(m_coreObject)->hasAttribute(HTMLNames::aria_sortAttr)) {
        switch (m_coreObject->sortDirection()) {
        case AccessibilitySortDirection::Invalid:
            break;
        case AccessibilitySortDirection::Ascending:
            map.add("sort"_s, "ascending"_s);
            break;
        case AccessibilitySortDirection::Descending:
            map.add("sort"_s, "descending"_s);
            break;
        case AccessibilitySortDirection::Other:
            map.add("sort"_s, "other"_s);
            break;
        case AccessibilitySortDirection::None:
            map.add("sort"_s, "none"_s);
            break;
        }
    }

    String isReadOnly = m_coreObject->readOnlyValue();
    if (!isReadOnly.isEmpty())
        map.add("readonly"_s, isReadOnly);

    String valueDescription = m_coreObject->valueDescription();
    if (!valueDescription.isEmpty())
        map.add("valuetext"_s, valueDescription);

    // According to the W3C Core Accessibility API Mappings 1.1, section 5.4.1 General Rules:
    // "User agents must expose the WAI-ARIA role string if the API supports a mechanism to do so."
    // In the case of ATSPI, the mechanism to do so is an object attribute pair (xml-roles:"string").
    // We cannot use the computedRoleString for this purpose because it is not limited to elements
    // with ARIA roles, and it might not contain the actual ARIA role value (e.g. DPub ARIA).
    String roleString = static_cast<AccessibilityObject*>(m_coreObject)->getAttribute(HTMLNames::roleAttr);
    if (!roleString.isEmpty())
        map.add("xml-roles"_s, roleString);

    String computedRoleString = m_coreObject->computedRoleString();
    if (!computedRoleString.isEmpty()) {
        map.add("computed-role"_s, computedRoleString);

        // The HTML AAM maps several elements to ARIA landmark roles. In order for the type of landmark
        // to be obtainable in the same fashion as an ARIA landmark, fall back on the computedRoleString.
        // We also want to do this for the style-format-group element types so that the type of format
        // group it is doesn't get lost to a generic platform role.
        if (m_coreObject->ariaRoleAttribute() == AccessibilityRole::Unknown && (m_coreObject->isLandmark() || m_coreObject->isStyleFormatGroup()))
            map.set("xml-roles"_s, computedRoleString);
    }

    String roleDescription = m_coreObject->roleDescription();
    if (!roleDescription.isEmpty())
        map.add("roledescription"_s, roleDescription);

    String dropEffect = static_cast<AccessibilityObject*>(m_coreObject)->getAttribute(HTMLNames::aria_dropeffectAttr);
    if (!dropEffect.isEmpty())
        map.add("dropeffect"_s, dropEffect);

    if (m_coreObject->supportsDragging())
        map.add("grabbed"_s, m_coreObject->isGrabbed() ? "true"_s : "false"_s);

    String keyShortcuts = m_coreObject->keyShortcuts();
    if (!keyShortcuts.isEmpty())
        map.add("keyshortcuts"_s, keyShortcuts);

    if (liveObject && (liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSuperscript) || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSubscript)))
        map.add("multiscript-type"_s, "pre"_s);
    else if (liveObject && (liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSuperscript) || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSubscript)))
        map.add("multiscript-type"_s, "post"_s);

    auto* liveContainer = liveObject ? liveObject->liveRegionAncestor(false) : nullptr;
    if (liveContainer) {
        auto liveStatus = liveContainer->liveRegionStatus();
        map.add("container-live"_s, liveStatus);
        auto relevant = liveContainer->liveRegionRelevant();
        map.add("container-relevant"_s, relevant);
        bool isAtomic = liveContainer->liveRegionAtomic();
        if (isAtomic)
            map.add("container-atomic"_s, "true"_s);
        const String& liveRole = roleString.isEmpty() ? computedRoleString : roleString;
        if (!liveRole.isEmpty())
            map.add("container-live-role"_s, liveRole);

        if (liveContainer == m_coreObject) {
            map.add("live"_s, liveStatus);
            map.add("relevant"_s, relevant);
            if (isAtomic)
                map.add("atomic"_s, "true"_s);
        } else if (!isAtomic && m_coreObject->liveRegionAtomic())
            map.add("atomic"_s, "true"_s);
    }

    return map;
}

void AccessibilityObjectAtspi::buildAttributes(GVariantBuilder* builder) const
{
    for (const auto& it : attributes())
        g_variant_builder_add(builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
}

HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> AccessibilityObjectAtspi::relationMap() const
{
    HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> map;
    if (!m_coreObject)
        return map;

    auto addRelation = [&](Atspi::Relation relation, const AccessibilityObject::AccessibilityChildrenVector& children) {
        Vector<RefPtr<AccessibilityObjectAtspi>> wrappers;
        for (const auto& child : children) {
            if (auto* wrapper = child->wrapper())
                wrappers.append(wrapper);
        }
        if (!wrappers.isEmpty())
            map.add(relation, WTFMove(wrappers));
    };

    AccessibilityObject::AccessibilityChildrenVector ariaLabelledByElements;
    if (m_coreObject->isControl()) {
        if (auto* label = m_coreObject->correspondingLabelForControlElement())
            ariaLabelledByElements.append(label);
    } else if (m_coreObject->isFieldset()) {
        if (auto* label = m_coreObject->titleUIElement())
            ariaLabelledByElements.append(label);
    } else if (m_coreObject->roleValue() == AccessibilityRole::Legend) {
        if (auto* renderFieldset = ancestorsOfType<RenderBlock>(*m_coreObject->renderer()).first()) {
            if (renderFieldset->isFieldset())
                ariaLabelledByElements.append(m_coreObject->axObjectCache()->getOrCreate(renderFieldset));
        }
    } else if (!m_coreObject->correspondingControlForLabelElement())
        ariaLabelledByElements = m_coreObject->labelledByObjects();
    addRelation(Atspi::LabelledBy, ariaLabelledByElements);

    AccessibilityObject::AccessibilityChildrenVector labelForObjects;
    if (auto* control = m_coreObject->correspondingControlForLabelElement())
        labelForObjects.append(control);
    else
        labelForObjects = m_coreObject->labelForObjects();
    addRelation(Atspi::LabelFor, labelForObjects);

    addRelation(Atspi::FlowsTo, m_coreObject->flowToObjects());
    addRelation(Atspi::FlowsFrom, m_coreObject->flowFromObjects());

    addRelation(Atspi::DescribedBy, m_coreObject->describedByObjects());
    addRelation(Atspi::DescriptionFor, m_coreObject->descriptionForObjects());

    addRelation(Atspi::ControllerFor, m_coreObject->controlledObjects());
    addRelation(Atspi::ControlledBy, m_coreObject->controllers());

    addRelation(Atspi::NodeParentOf, m_coreObject->ownedObjects());
    addRelation(Atspi::NodeChildOf, m_coreObject->owners());

    addRelation(Atspi::Details, m_coreObject->detailedByObjects());
    addRelation(Atspi::DetailsFor, m_coreObject->detailsForObjects());

    addRelation(Atspi::ErrorMessage, m_coreObject->errorMessageObjects());
    addRelation(Atspi::ErrorFor, m_coreObject->errorMessageForObjects());

    return map;
}

void AccessibilityObjectAtspi::buildRelationSet(GVariantBuilder* builder) const
{
    for (const auto& it : relationMap()) {
        GVariantBuilder arrayBuilder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
        for (const auto& atspiObject : it.value)
            g_variant_builder_add(&arrayBuilder, "@(so)", atspiObject->reference());
        g_variant_builder_add(builder, "(ua(so))", it.key, &arrayBuilder);
    }
}

void AccessibilityObjectAtspi::buildInterfaces(GVariantBuilder* builder) const
{
    if (m_interfaces.contains(Interface::Accessible))
        g_variant_builder_add(builder, "s", webkit_accessible_interface.name);
    if (m_interfaces.contains(Interface::Component))
        g_variant_builder_add(builder, "s", webkit_component_interface.name);
    if (m_interfaces.contains(Interface::Text))
        g_variant_builder_add(builder, "s", webkit_text_interface.name);
    if (m_interfaces.contains(Interface::Value))
        g_variant_builder_add(builder, "s", webkit_value_interface.name);
    if (m_interfaces.contains(Interface::Hyperlink))
        g_variant_builder_add(builder, "s", webkit_hyperlink_interface.name);
    if (m_interfaces.contains(Interface::Hypertext))
        g_variant_builder_add(builder, "s", webkit_hypertext_interface.name);
    if (m_interfaces.contains(Interface::Action))
        g_variant_builder_add(builder, "s", webkit_action_interface.name);
    if (m_interfaces.contains(Interface::Document))
        g_variant_builder_add(builder, "s", webkit_document_interface.name);
    if (m_interfaces.contains(Interface::Image))
        g_variant_builder_add(builder, "s", webkit_image_interface.name);
    if (m_interfaces.contains(Interface::Selection))
        g_variant_builder_add(builder, "s", webkit_selection_interface.name);
    if (m_interfaces.contains(Interface::Table))
        g_variant_builder_add(builder, "s", webkit_table_interface.name);
    if (m_interfaces.contains(Interface::TableCell))
        g_variant_builder_add(builder, "s", webkit_table_cell_interface.name);
    if (m_interfaces.contains(Interface::Collection))
        g_variant_builder_add(builder, "s", webkit_collection_interface.name);
}

void AccessibilityObjectAtspi::serialize(GVariantBuilder* builder) const
{
    g_variant_builder_add(builder, "(so)", AccessibilityAtspi::singleton().uniqueName(), m_path.utf8().data());
    g_variant_builder_add(builder, "@(so)", AccessibilityAtspi::singleton().applicationReference());
    g_variant_builder_add(builder, "@(so)", parentReference());

    g_variant_builder_add(builder, "i", indexInParent());
    // Do not set the children count in cache, because children are handled by children-changed signals.
    g_variant_builder_add(builder, "i", -1);

    GVariantBuilder interfaces = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("as"));
    buildInterfaces(&interfaces);
    g_variant_builder_add(builder, "@as", g_variant_new("as", &interfaces));

    g_variant_builder_add(builder, "s", name().data());

    g_variant_builder_add(builder, "u", role());

    g_variant_builder_add(builder, "s", description().data());

    GVariantBuilder states = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("au"));
    auto atspiStates = state();
    g_variant_builder_add(&states, "u", static_cast<uint32_t>(atspiStates & 0xffffffff));
    g_variant_builder_add(&states, "u", static_cast<uint32_t>(atspiStates >> 32));
    g_variant_builder_add(builder, "@au", g_variant_builder_end(&states));
}

void AccessibilityObjectAtspi::childAdded(AccessibilityObjectAtspi& child)
{
    if (!m_isRegistered)
        return;

    if (!m_coreObject || m_coreObject->accessibilityIsIgnored())
        return;

    AccessibilityAtspi::singleton().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Added);
}

void AccessibilityObjectAtspi::childRemoved(AccessibilityObjectAtspi& child)
{
    if (!m_isRegistered)
        return;

    AccessibilityAtspi::singleton().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Removed);
}

void AccessibilityObjectAtspi::stateChanged(const char* name, bool value)
{
    AccessibilityAtspi::singleton().stateChanged(*this, name, value);
}

void AccessibilityObjectAtspi::loadEvent(const char* event)
{
    AccessibilityAtspi::singleton().loadEvent(*this, event);
}

std::optional<unsigned> AccessibilityObjectAtspi::effectiveRole() const
{
    if (m_coreObject->isPasswordField())
        return Atspi::Role::PasswordText;

    auto* liveObject = dynamicDowncast<AccessibilityObject>(m_coreObject);

    switch (m_coreObject->roleValue()) {
    case AccessibilityRole::Form:
        if (m_coreObject->ariaRoleAttribute() != AccessibilityRole::Unknown)
            return Atspi::Role::Landmark;
        break;
    case AccessibilityRole::ListMarker: {
        auto* renderer = m_coreObject->renderer();
        return renderer && renderer->isImage() ? Atspi::Role::Image : Atspi::Role::Text;
    }
    case AccessibilityRole::MathElement:
        if (m_coreObject->isMathRow())
            return Atspi::Role::Panel;
        if (m_coreObject->isMathTable())
            return Atspi::Role::Table;
        if (m_coreObject->isMathTableRow())
            return Atspi::Role::TableRow;
        if (m_coreObject->isMathTableCell())
            return Atspi::Role::TableCell;
        if (m_coreObject->isMathSubscriptSuperscript() || m_coreObject->isMathMultiscript())
            return Atspi::Role::Section;
        if (m_coreObject->isMathFraction())
            return Atspi::Role::MathFraction;
        if (m_coreObject->isMathSquareRoot() || m_coreObject->isMathRoot())
            return Atspi::Role::MathRoot;
        if (liveObject && (liveObject->isMathScriptObject(AccessibilityMathScriptObjectType::Subscript)
            || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSubscript)
            || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSubscript)))
            return Atspi::Role::Subscript;
        if (liveObject && (liveObject->isMathScriptObject(AccessibilityMathScriptObjectType::Superscript)
            || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSuperscript)
            || liveObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSuperscript)))
            return Atspi::Role::Superscript;
        if (m_coreObject->isMathToken())
            return Atspi::Role::Static;
        break;
    case AccessibilityRole::ListItem: {
        if (m_coreObject->inheritsPresentationalRole())
            return Atspi::Role::Section;
        break;
    }
    default:
        break;
    }

    return std::nullopt;
}

unsigned AccessibilityObjectAtspi::role() const
{
    if (!m_coreObject)
        return Atspi::Role::InvalidRole;

    if (auto effective = effectiveRole())
        return *effective;

    return atspiRole(m_coreObject->roleValue());
}

String AccessibilityObjectAtspi::effectiveRoleName() const
{
    auto effective = effectiveRole();
    if (!effective)
        return { };

    switch (*effective) {
    case Atspi::Role::Image:
        return "image"_s;
    case Atspi::Role::Text:
    case Atspi::Role::Static:
        return "text"_s;
    case Atspi::Role::InvalidRole:
        return "invalid"_s;
    case Atspi::Role::Panel:
        return "panel"_s;
    case Atspi::Role::PasswordText:
        return "password text"_s;
    case Atspi::Role::Table:
        return "table"_s;
    case Atspi::Role::TableRow:
        return "table row"_s;
    case Atspi::Role::TableCell:
        return "table cell"_s;
    case Atspi::Role::Section:
        return "section"_s;
    case Atspi::Role::MathFraction:
        return "math fraction"_s;
    case Atspi::Role::MathRoot:
        return "math root"_s;
    case Atspi::Role::Subscript:
        return "subscript"_s;
    case Atspi::Role::Superscript:
        return "superscript"_s;
    case Atspi::Role::Landmark:
        return "landmark"_s;
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

String AccessibilityObjectAtspi::roleName() const
{
    if (!m_coreObject)
        return "invalid"_s;

    auto effective = effectiveRoleName();
    if (!effective.isEmpty())
        return effective;

    return m_coreObject->rolePlatformString();
}

const char* AccessibilityObjectAtspi::effectiveLocalizedRoleName() const
{
    auto effective = effectiveRole();
    if (!effective)
        return nullptr;

    switch (*effective) {
    case Atspi::Role::Image:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Image);
    case Atspi::Role::Text:
    case Atspi::Role::Static:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::StaticText);
    case Atspi::Role::InvalidRole:
        return _("invalid");
    case Atspi::Role::Panel:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Group);
    case Atspi::Role::PasswordText:
        return _("password text");
    case Atspi::Role::Table:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Table);
    case Atspi::Role::TableRow:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Row);
    case Atspi::Role::TableCell:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Cell);
    case Atspi::Role::Section:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Div);
    case Atspi::Role::MathFraction:
        return _("math fraction");
    case Atspi::Role::MathRoot:
        return _("math root");
    case Atspi::Role::Subscript:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Subscript);
    case Atspi::Role::Superscript:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::Superscript);
    case Atspi::Role::Landmark:
        return AccessibilityAtspi::localizedRoleName(AccessibilityRole::LandmarkMain);
    default:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

const char* AccessibilityObjectAtspi::localizedRoleName() const
{
    if (!m_coreObject)
        return _("invalid");

    if (const auto* effective = effectiveLocalizedRoleName())
        return effective;

    return AccessibilityAtspi::localizedRoleName(m_coreObject->roleValue());
}

void AccessibilityObjectAtspi::updateBackingStore()
{
    if (m_coreObject)
        m_coreObject->updateBackingStore();
}

bool AccessibilityObjectAtspi::isIgnored() const
{
    return m_coreObject ? m_coreObject->accessibilityIsIgnored() : true;
}

void AccessibilityObject::detachPlatformWrapper(AccessibilityDetachmentType detachmentType)
{
    switch (detachmentType) {
    case AccessibilityDetachmentType::CacheDestroyed:
        wrapper()->cacheDestroyed();
        break;
    case AccessibilityDetachmentType::ElementDestroyed:
        wrapper()->elementDestroyed();
        break;
    case AccessibilityDetachmentType::ElementChanged:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    return false;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    auto* parent = parentObject();
    if (!parent)
        return AccessibilityObjectInclusion::DefaultBehavior;

    // If the author has provided a role, platform-specific inclusion likely doesn't apply.
    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return AccessibilityObjectInclusion::DefaultBehavior;

    // Never expose an unknown object, since AT's won't know what to
    // do with them. This is what is done on the Mac as well.
    if (roleValue() == AccessibilityRole::Unknown)
        return AccessibilityObjectInclusion::IgnoreObject;

    // The object containing the text should implement org.a11y.atspi.Text itself.
    if (roleValue() == AccessibilityRole::StaticText)
        return AccessibilityObjectInclusion::IgnoreObject;

    // Entries and password fields have extraneous children which we want to ignore.
    if (parent->isPasswordField() || parent->isTextControl())
        return AccessibilityObjectInclusion::IgnoreObject;

    // We expose the slider as a whole but not its value indicator.
    if (roleValue() == AccessibilityRole::SliderThumb)
        return AccessibilityObjectInclusion::IgnoreObject;

    // List items inheriting presentational are ignored, but their content exposed.
    // Since we expose text in the parent, we need to expose presentational list items
    // with a different role (section).
    if (roleValue() == AccessibilityRole::ListItem && inheritsPresentationalRole())
        return AccessibilityObjectInclusion::IncludeObject;

    RenderObject* renderObject = renderer();
    if (!renderObject)
        return AccessibilityObjectInclusion::DefaultBehavior;

    // We always want to include paragraphs that have rendered content.
    // WebCore Accessibility does so unless there is a RenderBlock child.
    if (roleValue() == AccessibilityRole::Paragraph) {
        auto child = childrenOfType<RenderBlock>(downcast<RenderElement>(*renderObject)).first();
        return child ? AccessibilityObjectInclusion::IncludeObject : AccessibilityObjectInclusion::DefaultBehavior;
    }

    // We always want to include table cells (layout and CSS) that have rendered text content.
    if (is<RenderTableCell>(renderObject)) {
        for (const auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(*renderObject))) {
            if (is<RenderInline>(child) || is<RenderText>(child) || is<HTMLSpanElement>(child.node()))
                return AccessibilityObjectInclusion::IncludeObject;
        }
        return AccessibilityObjectInclusion::DefaultBehavior;
    }

    if (renderObject->isAnonymousBlock()) {
        // The text displayed by an ARIA menu item is exposed through the accessible name.
        if (parent->isMenuItem())
            return AccessibilityObjectInclusion::IgnoreObject;

        // The text displayed in headings is typically exposed in the heading itself.
        if (parent->isHeading())
            return AccessibilityObjectInclusion::IgnoreObject;

        // The text displayed in list items is typically exposed in the list item itself.
        if (parent->isListItem())
            return AccessibilityObjectInclusion::IgnoreObject;

        // The text displayed in links is typically exposed in the link itself.
        if (parent->isLink())
            return AccessibilityObjectInclusion::IgnoreObject;

        // FIXME: This next one needs some further consideration. But paragraphs are not
        // typically huge (like divs). And ignoring anonymous block children of paragraphs
        // will preserve existing behavior.
        if (parent->roleValue() == AccessibilityRole::Paragraph)
            return AccessibilityObjectInclusion::IgnoreObject;

        return AccessibilityObjectInclusion::DefaultBehavior;
    }

    Node* node = renderObject->node();
    if (!node)
        return AccessibilityObjectInclusion::DefaultBehavior;

    // We don't want <span> elements to show up in the accessibility hierarchy unless
    // we have good reasons for that (e.g. focusable or visible because of containing
    // a meaningful accessible name, maybe set through ARIA).
    if (is<HTMLSpanElement>(node) && !canSetFocusAttribute() && !hasAttributesRequiredForInclusion() && !supportsARIAAttributes())
        return AccessibilityObjectInclusion::IgnoreObject;

    if (is<TextControlInnerTextElement>(node))
        return AccessibilityObjectInclusion::IgnoreObject;

    return AccessibilityObjectInclusion::DefaultBehavior;
}

} // namespace WebCore

#endif // USE(ATSPI)
