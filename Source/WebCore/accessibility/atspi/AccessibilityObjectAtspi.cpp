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

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
#include "AXIsolatedObject.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityObjectInterface.h"
#include "AccessibilityRootAtspi.h"
#include "AccessibilityTableCell.h"
#include "ElementInlines.h"
#include "RenderAncestorIterator.h"
#include "RenderBlock.h"
#include "RenderObject.h"
#include <glib/gi18n-lib.h>
#include <wtf/MainThread.h>
#include <wtf/UUID.h>

namespace WebCore {

Ref<AccessibilityObjectAtspi> AccessibilityObjectAtspi::create(AXCoreObject* coreObject)
{
    return adoptRef(*new AccessibilityObjectAtspi(coreObject));
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
    OptionSet<Interface> interfaces = { Interface::Accessible, Interface::Component };

    RenderObject* renderer = coreObject.isAccessibilityRenderObject() ? coreObject.renderer() : nullptr;
    if (coreObject.roleValue() == AccessibilityRole::StaticText || coreObject.roleValue() == AccessibilityRole::ColorWell)
        interfaces.add(Interface::Text);
    else if (coreObject.isTextControl() || coreObject.isNonNativeTextControl())
        interfaces.add(Interface::Text);
    else if (!coreObject.isWebArea()) {
        if (coreObject.roleValue() != AccessibilityRole::Table) {
            if ((renderer && renderer->childrenInline()) || roleIsTextType(coreObject.roleValue()) || coreObject.isMathToken())
                interfaces.add(Interface::Text);
        }

        // Add the Text interface for list items whose first accessible child has a text renderer.
        if (coreObject.roleValue() == AccessibilityRole::ListItem) {
            const auto& children = coreObject.children();
            if (!children.isEmpty()) {
                auto childInterfaces = interfacesForObject(*children[0]);
                if (childInterfaces.contains(Interface::Text))
                    interfaces.add(Interface::Text);
            }
        }
    }

    return interfaces;
}

AccessibilityObjectAtspi::AccessibilityObjectAtspi(AXCoreObject* coreObject)
    : m_coreObject(coreObject)
    , m_interfaces(interfacesForObject(*m_coreObject))
{
    RELEASE_ASSERT(isMainThread());
}

void AccessibilityObjectAtspi::attach(AXCoreObject* axObject)
{
    RELEASE_ASSERT(is<AXIsolatedObject>(*axObject));
    m_axObject = axObject;
}

void AccessibilityObjectAtspi::detach()
{
    m_axObject = nullptr;
}

void AccessibilityObjectAtspi::cacheDestroyed()
{
    RELEASE_ASSERT(isMainThread());
    m_coreObject = nullptr;
    if (!m_isRegistered.load())
        return;

    root()->atspi().unregisterObject(*this);
}

void AccessibilityObjectAtspi::elementDestroyed()
{
    RELEASE_ASSERT(isMainThread());
    m_coreObject = nullptr;
    if (!m_isRegistered.load())
        return;

    if (m_parent && *m_parent)
        m_parent.value()->childRemoved(*this);

    root()->atspi().unregisterObject(*this);
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
    case AccessibilityRole::ListMarker:
        return Atspi::Role::Text;
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
    case AccessibilityRole::MathElement:
        return Atspi::Role::MathFraction;
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
        return Atspi::Role::Unknown;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GDBusInterfaceVTable AccessibilityObjectAtspi::s_accessibleFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        RELEASE_ASSERT(!isMainThread());
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
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", atspiObject->root()->applicationReference()));
        else if (!g_strcmp0(methodName, "GetChildAtIndex")) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            auto* wrapper = index >= 0 ? atspiObject->childAt(index) : nullptr;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@(so))", wrapper ? wrapper->reference() : atspiObject->root()->atspi().nullReference()));
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
        RELEASE_ASSERT(!isMainThread());
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "Name"))
            return g_variant_new_string(atspiObject->name().data());
        if (!g_strcmp0(propertyName, "Description"))
            return g_variant_new_string(atspiObject->description().data());
        if (!g_strcmp0(propertyName, "Locale"))
            return g_variant_new_string(setlocale(LC_MESSAGES, nullptr));
        if (!g_strcmp0(propertyName, "AccessibleId"))
            return g_variant_new_string(atspiObject->m_axObject ? String::number(atspiObject->m_axObject->objectID()).utf8().data() : "");
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
    nullptr
};

const String& AccessibilityObjectAtspi::path()
{
    RELEASE_ASSERT(!isMainThread());
    if (m_path.isNull()) {
        auto* atspiRoot = root();
        RELEASE_ASSERT(atspiRoot);
        m_isRegistered.store(true);

        Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>> interfaces;
        if (m_interfaces.contains(Interface::Accessible))
            interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_accessible_interface), &s_accessibleFunctions });
        if (m_interfaces.contains(Interface::Component))
            interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_component_interface), &s_componentFunctions });
        if (m_interfaces.contains(Interface::Text))
            interfaces.append({ const_cast<GDBusInterfaceInfo*>(&webkit_text_interface), &s_textFunctions });
        m_path = atspiRoot->atspi().registerObject(*this, WTFMove(interfaces));
    }

    return m_path;
}

GVariant* AccessibilityObjectAtspi::reference()
{
    RELEASE_ASSERT(!isMainThread());
    return g_variant_new("(so)", root()->atspi().uniqueName(), path().utf8().data());
}

void AccessibilityObjectAtspi::setRoot(AccessibilityRootAtspi* root)
{
    Locker locker { m_rootLock };
    m_root = root;
}

AccessibilityRootAtspi* AccessibilityObjectAtspi::root() const
{
    Locker locker { m_rootLock };
    return m_root;
}

void AccessibilityObjectAtspi::setParent(std::optional<AccessibilityObjectAtspi*> atspiParent)
{
    RELEASE_ASSERT(isMainThread());
    if (m_parent == atspiParent)
        return;

    m_parent = atspiParent;
    if (m_parent && *m_parent)
        m_parent.value()->childAdded(*this);
}

std::optional<AccessibilityObjectAtspi*> AccessibilityObjectAtspi::parent() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return std::nullopt;

    auto* axParent = m_axObject->parentObjectUnignored();
    if (!axParent)
        return nullptr;

    if (auto* atspiParent = axParent->wrapper())
        return atspiParent;

    return std::nullopt;
}

GVariant* AccessibilityObjectAtspi::parentReference() const
{
    auto parentAtspi = parent();
    if (!parentAtspi)
        return root()->atspi().nullReference();

    if (!parentAtspi.value())
        return root()->reference();

    return parentAtspi.value()->reference();
}

unsigned AccessibilityObjectAtspi::childCount() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return 0;

    return m_axObject->children().size();
}

AccessibilityObjectAtspi* AccessibilityObjectAtspi::childAt(unsigned index) const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return nullptr;

    const auto& children = m_axObject->children();
    if (index >= children.size())
        return nullptr;

    auto* wrapper = children[index]->wrapper();
    wrapper->setRoot(root());
    return wrapper;
}

Vector<RefPtr<AccessibilityObjectAtspi>> AccessibilityObjectAtspi::children() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return { };

    const auto& children = m_axObject->children();
    Vector<RefPtr<AccessibilityObjectAtspi>> wrappers;
    wrappers.reserveInitialCapacity(children.size());
    auto* root = this->root();
    for (const auto& child : children) {
        if (auto* wrapper = child->wrapper()) {
            wrapper->setRoot(root);
            wrappers.uncheckedAppend(wrapper);
        }
    }
    return wrappers;
}

int AccessibilityObjectAtspi::indexInParent() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject) {
        m_indexInParent = -1;
        return m_indexInParent;
    }

    m_axObject->updateBackingStore();

    auto* axParent = m_axObject->parentObjectUnignored();
    if (!axParent) {
        m_indexInParent = 0;
        return m_indexInParent;
    }

    const auto& children = axParent->children();
    unsigned index = 0;
    for (const auto& child : children) {
        if (child.get() == m_axObject) {
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
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return "";

    Vector<AccessibilityText> textOrder;
    m_axObject->accessibilityText(textOrder);

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
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return "";

    Vector<AccessibilityText> textOrder;
    m_axObject->accessibilityText(textOrder);

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
    return Accessibility::retrieveValueFromMainThread<uint64_t>([this]() -> uint64_t {
        uint64_t states = 0;

        auto addState = [&](Atspi::State atspiState) {
            states |= (G_GUINT64_CONSTANT(1) << atspiState);
        };

        if (m_coreObject)
            m_coreObject->updateBackingStore();

        if (!m_coreObject) {
            addState(Atspi::State::Defunct);
            return states;
        }

        if (m_coreObject->isEnabled()) {
            addState(Atspi::State::Enabled);
            addState(Atspi::State::Sensitive);
        }

        if (m_coreObject->isVisible()) {
            addState(Atspi::State::Visible);
            if (!m_coreObject->isOffScreen())
                addState(Atspi::State::Showing);
        }

        if (m_coreObject->canSetFocusAttribute())
            addState(Atspi::State::Focusable);

        if (m_coreObject->isFocused())
            addState(Atspi::State::Focused);

        if (m_coreObject->canSetValueAttribute()) {
            if (m_coreObject->supportsChecked())
                addState(Atspi::State::Checkable);

            if (m_coreObject->isTextControl() || m_coreObject->isNonNativeTextControl())
                addState(Atspi::State::Editable);
        } else if (m_coreObject->supportsReadOnly())
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

        if (m_coreObject->invalidStatus() != "false")
            addState(Atspi::State::InvalidEntry);

        if (m_coreObject->supportsAutoComplete() && m_coreObject->autoCompleteValue() != "none")
            addState(Atspi::State::SupportsAutocompletion);

        return states;
    });
}

String AccessibilityObjectAtspi::id() const
{
    RELEASE_ASSERT(isMainThread());
    if (m_coreObject)
        m_coreObject->updateBackingStore();

    if (!m_coreObject)
        return { };

    if (auto* element = m_coreObject->element())
        return element->getIdAttribute().string();

    return { };
}

HashMap<String, String> AccessibilityObjectAtspi::attributes() const
{
    RELEASE_ASSERT(isMainThread());
    HashMap<String, String> map;
#if PLATFORM(GTK)
    map.add("toolkit", "WebKitGTK");
#elif PLATFORM(WPE)
    map.add("toolkit", "WPEWebKit");
#endif
    if (m_coreObject)
        m_coreObject->updateBackingStore();

    if (!m_coreObject)
        return map;

    String tagName = m_coreObject->tagName();
    if (!tagName.isEmpty())
        map.add("tag", tagName);

    if (auto* element = m_coreObject->element()) {
        String id = element->getIdAttribute().string();
        if (!id.isEmpty())
            map.add("id", id);
    }

    int level = m_coreObject->isHeading() ? m_coreObject->headingLevel() : m_coreObject->hierarchicalLevel();
    if (level)
        map.add("level", String::number(level));

    int rowCount = m_coreObject->axRowCount();
    if (rowCount)
        map.add("rowcount", String::number(rowCount));

    int columnCount = m_coreObject->axColumnCount();
    if (columnCount)
        map.add("colcount", String::number(columnCount));

    int rowIndex = m_coreObject->axRowIndex();
    if (rowIndex != -1)
        map.add("rowindex", String::number(rowIndex));

    int columnIndex = m_coreObject->axColumnIndex();
    if (columnIndex != -1)
        map.add("colindex", String::number(columnIndex));

    if (is<AccessibilityTableCell>(m_coreObject)) {
        auto& cell = downcast<AccessibilityTableCell>(*m_coreObject);
        int rowSpan = cell.axRowSpan();
        if (rowSpan != -1)
            map.add("rowspan", String::number(rowSpan));

        int columnSpan = cell.axColumnSpan();
        if (columnSpan != -1)
            map.add("colspan", String::number(columnSpan));
    }

    String placeholder = m_coreObject->placeholderValue();
    if (!placeholder.isEmpty())
        map.add("placeholder-text", placeholder);

    if (m_coreObject->supportsAutoComplete())
        map.add("autocomplete", m_coreObject->autoCompleteValue());

    if (m_coreObject->supportsHasPopup())
        map.add("haspopup", m_coreObject->popupValue());

    if (m_coreObject->supportsCurrent())
        map.add("current", m_coreObject->currentValue());

    if (m_coreObject->supportsPosInSet())
        map.add("posinset", String::number(m_coreObject->posInSet()));

    if (m_coreObject->supportsSetSize())
        map.add("setsize", String::number(m_coreObject->setSize()));

    // The Core AAM states that an explicitly-set value should be exposed, including "none".
    if (static_cast<AccessibilityObject*>(m_coreObject)->hasAttribute(HTMLNames::aria_sortAttr)) {
        switch (m_coreObject->sortDirection()) {
        case AccessibilitySortDirection::Invalid:
            break;
        case AccessibilitySortDirection::Ascending:
            map.add("sort", "ascending");
            break;
        case AccessibilitySortDirection::Descending:
            map.add("sort", "descending");
            break;
        case AccessibilitySortDirection::Other:
            map.add("sort", "other");
            break;
        case AccessibilitySortDirection::None:
            map.add("sort", "none");
            break;
        }
    }

    String isReadOnly = m_coreObject->readOnlyValue();
    if (!isReadOnly.isEmpty())
        map.add("readonly", isReadOnly);

    String valueDescription = m_coreObject->valueDescription();
    if (!valueDescription.isEmpty())
        map.add("valuetext", valueDescription);

    // According to the W3C Core Accessibility API Mappings 1.1, section 5.4.1 General Rules:
    // "User agents must expose the WAI-ARIA role string if the API supports a mechanism to do so."
    // In the case of ATK, the mechanism to do so is an object attribute pair (xml-roles:"string").
    // We cannot use the computedRoleString for this purpose because it is not limited to elements
    // with ARIA roles, and it might not contain the actual ARIA role value (e.g. DPub ARIA).
    String roleString = static_cast<AccessibilityObject*>(m_coreObject)->getAttribute(HTMLNames::roleAttr);
    if (!roleString.isEmpty())
        map.add("xml-roles", roleString);

    String computedRoleString = m_coreObject->computedRoleString();
    if (!computedRoleString.isEmpty()) {
        map.add("computed-role", computedRoleString);

        // The HTML AAM maps several elements to ARIA landmark roles. In order for the type of landmark
        // to be obtainable in the same fashion as an ARIA landmark, fall back on the computedRoleString.
        // We also want to do this for the style-format-group element types so that the type of format
        // group it is doesn't get lost to a generic platform role.
        if (m_coreObject->ariaRoleAttribute() == AccessibilityRole::Unknown && (m_coreObject->isLandmark() || m_coreObject->isStyleFormatGroup()))
            map.set("xml-roles", computedRoleString);
    }

    String roleDescription = m_coreObject->roleDescription();
    if (!roleDescription.isEmpty())
        map.add("roledescription", roleDescription);

    String dropEffect = static_cast<AccessibilityObject*>(m_coreObject)->getAttribute(HTMLNames::aria_dropeffectAttr);
    if (!dropEffect.isEmpty())
        map.add("dropeffect", dropEffect);

    if (m_coreObject->supportsDragging())
        map.add("grabbed", m_coreObject->isGrabbed() ? "true" : "false");

    String keyShortcuts = m_coreObject->keyShortcutsValue();
    if (!keyShortcuts.isEmpty())
        map.add("keyshortcuts", keyShortcuts);

    return map;
}

void AccessibilityObjectAtspi::buildAttributes(GVariantBuilder* builder) const
{
    auto attributes = Accessibility::retrieveValueFromMainThread<HashMap<String, String>>([this]() -> HashMap<String, String> {
        return this->attributes();
    });

    for (const auto& it : attributes)
        g_variant_builder_add(builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
}

HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> AccessibilityObjectAtspi::relationMap() const
{
    RELEASE_ASSERT(isMainThread());

    HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> map;
    if (m_coreObject)
        m_coreObject->updateBackingStore();

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
        m_coreObject->ariaLabelledByElements(ariaLabelledByElements);
    addRelation(Atspi::LabelledBy, ariaLabelledByElements);

    AccessibilityObject::AccessibilityChildrenVector ariaLabelledByReferencingElements;
    if (auto* control = m_coreObject->correspondingControlForLabelElement())
        ariaLabelledByReferencingElements.append(control);
    else
        m_coreObject->ariaLabelledByReferencingElements(ariaLabelledByReferencingElements);
    addRelation(Atspi::LabelFor, ariaLabelledByReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaFlowToElements;
    m_coreObject->ariaFlowToElements(ariaFlowToElements);
    addRelation(Atspi::FlowsTo, ariaFlowToElements);

    AccessibilityObject::AccessibilityChildrenVector ariaFlowToReferencingElements;
    m_coreObject->ariaFlowToReferencingElements(ariaFlowToReferencingElements);
    addRelation(Atspi::FlowsFrom, ariaFlowToReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaDescribedByElements;
    m_coreObject->ariaDescribedByElements(ariaDescribedByElements);
    addRelation(Atspi::DescribedBy, ariaDescribedByElements);

    AccessibilityObject::AccessibilityChildrenVector ariaDescribedByReferencingElements;
    m_coreObject->ariaDescribedByReferencingElements(ariaDescribedByReferencingElements);
    addRelation(Atspi::DescriptionFor, ariaDescribedByReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaControlsElements;
    m_coreObject->ariaControlsElements(ariaControlsElements);
    addRelation(Atspi::ControllerFor, ariaControlsElements);

    AccessibilityObject::AccessibilityChildrenVector ariaControlsReferencingElements;
    m_coreObject->ariaControlsReferencingElements(ariaControlsReferencingElements);
    addRelation(Atspi::ControlledBy, ariaControlsReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaOwnsElements;
    m_coreObject->ariaOwnsElements(ariaOwnsElements);
    addRelation(Atspi::NodeParentOf, ariaOwnsElements);

    AccessibilityObject::AccessibilityChildrenVector ariaOwnsReferencingElements;
    m_coreObject->ariaOwnsReferencingElements(ariaOwnsReferencingElements);
    addRelation(Atspi::NodeChildOf, ariaOwnsReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaDetailsElements;
    m_coreObject->ariaDetailsElements(ariaDetailsElements);
    addRelation(Atspi::Details, ariaDetailsElements);

    AccessibilityObject::AccessibilityChildrenVector ariaDetailsReferencingElements;
    m_coreObject->ariaDetailsReferencingElements(ariaDetailsReferencingElements);
    addRelation(Atspi::DetailsFor, ariaDetailsReferencingElements);

    AccessibilityObject::AccessibilityChildrenVector ariaErrorMessageElements;
    m_coreObject->ariaErrorMessageElements(ariaErrorMessageElements);
    addRelation(Atspi::ErrorMessage, ariaErrorMessageElements);

    AccessibilityObject::AccessibilityChildrenVector ariaErrorMessageReferencingElements;
    m_coreObject->ariaErrorMessageReferencingElements(ariaErrorMessageReferencingElements);
    addRelation(Atspi::ErrorFor, ariaErrorMessageReferencingElements);

    return map;
}

void AccessibilityObjectAtspi::buildRelationSet(GVariantBuilder* builder) const
{
    auto relationMap = Accessibility::retrieveValueFromMainThread<HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>>>([this]() -> HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> {
        return this->relationMap();
    });

    for (const auto& it : relationMap) {
        GVariantBuilder arrayBuilder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a(so)"));
        for (const auto& atspiObject : it.value) {
            atspiObject->setRoot(root());
            g_variant_builder_add(&arrayBuilder, "@(so)", atspiObject->reference());
        }
        g_variant_builder_add(builder, "(ua(so))", it.key, &arrayBuilder);
    }
}

void AccessibilityObjectAtspi::buildInterfaces(GVariantBuilder* builder) const
{
    RELEASE_ASSERT(!isMainThread());
    if (m_interfaces.contains(Interface::Accessible))
        g_variant_builder_add(builder, "s", webkit_accessible_interface.name);
    if (m_interfaces.contains(Interface::Component))
        g_variant_builder_add(builder, "s", webkit_component_interface.name);
    if (m_interfaces.contains(Interface::Text))
        g_variant_builder_add(builder, "s", webkit_text_interface.name);
}

void AccessibilityObjectAtspi::serialize(GVariantBuilder* builder) const
{
    RELEASE_ASSERT(!isMainThread());
    auto* atspiRoot = root();
    g_variant_builder_add(builder, "(so)", atspiRoot->atspi().uniqueName(), m_path.utf8().data());
    g_variant_builder_add(builder, "(so)", atspiRoot->parentUniqueName().utf8().data(), "/org/a11y/atspi/accessible/root");
    g_variant_builder_add(builder, "@(so)", parentReference());

    g_variant_builder_add(builder, "i", indexInParent());
    g_variant_builder_add(builder, "i", childCount());

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
    RELEASE_ASSERT(isMainThread());
    if (!m_isRegistered.load())
        return;

    RunLoop::main().dispatch([this, protectedThis = Ref { *this }, child = Ref { child }] {
        if (!m_coreObject)
            return;
        root()->atspi().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Added);
    });
}

void AccessibilityObjectAtspi::childRemoved(AccessibilityObjectAtspi& child)
{
    RELEASE_ASSERT(isMainThread());
    if (!m_isRegistered.load())
        return;

    root()->atspi().childrenChanged(*this, child, AccessibilityAtspi::ChildrenChanged::Removed);
}

void AccessibilityObjectAtspi::stateChanged(const char* name, bool value)
{
    RELEASE_ASSERT(isMainThread());
    if (!m_isRegistered.load())
        return;

    root()->atspi().stateChanged(*this, name, value);
}

unsigned AccessibilityObjectAtspi::role() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return Atspi::Role::Unknown;

    return atspiRole(m_axObject->roleValue());
}

String AccessibilityObjectAtspi::roleName() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return "invalid";

    return m_axObject->rolePlatformString();
}

const char* AccessibilityObjectAtspi::localizedRoleName() const
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_axObject)
        return _("invalid");

    return AccessibilityAtspi::localizedRoleName(m_axObject->roleValue());
}

void AccessibilityObjectAtspi::updateBackingStore()
{
    if (isMainThread()) {
        if (m_coreObject)
            m_coreObject->updateBackingStore();
        return;
    }

    if (m_axObject)
        m_axObject->updateBackingStore();
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
    RELEASE_ASSERT(isMainThread());

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

    return AccessibilityObjectInclusion::DefaultBehavior;
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
