/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
 * Copyright (C) 2013 Samsung Electronics
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
#include "WebKitAccessibleWrapperAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityList.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityTable.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "HostWindow.h"
#include "RenderObject.h"
#include "SVGElement.h"
#include "Settings.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "WebKitAccessibleHyperlink.h"
#include "WebKitAccessibleInterfaceAction.h"
#include "WebKitAccessibleInterfaceComponent.h"
#include "WebKitAccessibleInterfaceDocument.h"
#include "WebKitAccessibleInterfaceEditableText.h"
#include "WebKitAccessibleInterfaceHyperlinkImpl.h"
#include "WebKitAccessibleInterfaceHypertext.h"
#include "WebKitAccessibleInterfaceImage.h"
#include "WebKitAccessibleInterfaceSelection.h"
#include "WebKitAccessibleInterfaceTable.h"
#include "WebKitAccessibleInterfaceTableCell.h"
#include "WebKitAccessibleInterfaceText.h"
#include "WebKitAccessibleInterfaceValue.h"
#include "WebKitAccessibleUtil.h"
#include "htmlediting.h"
#include <glib/gprintf.h>
#include <wtf/text/CString.h>

using namespace WebCore;

struct _WebKitAccessiblePrivate {
    // Cached data for AtkObject.
    CString accessibleName;
    CString accessibleDescription;

    // Cached data for AtkAction.
    CString actionName;
    CString actionKeyBinding;

    // Cached data for AtkDocument.
    CString documentLocale;
    CString documentType;
    CString documentEncoding;
    CString documentURI;

    // Cached data for AtkImage.
    CString imageDescription;
};

#define WEBKIT_ACCESSIBLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_ACCESSIBLE, WebKitAccessiblePrivate))

static AccessibilityObject* fallbackObject()
{
    static AccessibilityObject* object = &AccessibilityListBoxOption::create().leakRef();
    return object;
}

static AccessibilityObject* core(AtkObject* object)
{
    if (!WEBKIT_IS_ACCESSIBLE(object))
        return 0;

    return webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(object));
}

static const gchar* webkitAccessibleGetName(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AccessibilityObject* coreObject = core(object);
    if (coreObject->isFieldset()) {
        AccessibilityObject* label = coreObject->titleUIElement();
        if (label) {
            AtkObject* atkObject = label->wrapper();
            if (ATK_IS_TEXT(atkObject))
                return atk_text_get_text(ATK_TEXT(atkObject), 0, -1);
        }
    }

    if (coreObject->isControl()) {
        AccessibilityObject* label = coreObject->correspondingLabelForControlElement();
        if (label) {
            AtkObject* atkObject = label->wrapper();
            if (ATK_IS_TEXT(atkObject))
                return atk_text_get_text(ATK_TEXT(atkObject), 0, -1);
        }

        // Try text under the node.
        String textUnder = coreObject->textUnderElement();
        if (textUnder.length())
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, textUnder);
    }

    if (is<SVGElement>(coreObject->element())) {
        Vector<AccessibilityText> textOrder;
        coreObject->accessibilityText(textOrder);

        for (const auto& text : textOrder) {
            if (text.textSource != HelpText && text.textSource != SummaryText)
                return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, text.text);
        }
        // FIXME: This is to keep the next blocks from returning duplicate text.
        // This behavior should be extended to all elements; not just SVG.
        return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, "");
    }

    if (coreObject->isImage() || coreObject->isInputImage() || coreObject->isImageMap() || coreObject->isImageMapLink()) {
        Node* node = coreObject->node();
        if (is<HTMLElement>(node)) {
            // Get the attribute rather than altText String so as not to fall back on title.
            const AtomicString& alt = downcast<HTMLElement>(*node).getAttribute(HTMLNames::altAttr);
            if (!alt.isEmpty())
                return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, alt);
        }
    }

    // Fallback for the webArea object: just return the document's title.
    if (coreObject->isWebArea()) {
        Document* document = coreObject->document();
        if (document)
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, document->title());
    }

    // Nothing worked so far, try with the AccessibilityObject's
    // title() before going ahead with stringValue().
    String axTitle = accessibilityTitle(coreObject);
    if (!axTitle.isEmpty())
        return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, axTitle);

    return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, coreObject->stringValue());
}

static const gchar* webkitAccessibleGetDescription(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AccessibilityObject* coreObject = core(object);
    Node* node = nullptr;
    if (coreObject->isAccessibilityRenderObject())
        node = coreObject->node();

    if (is<SVGElement>(node)) {
        Vector<AccessibilityText> textOrder;
        coreObject->accessibilityText(textOrder);

        for (const auto& text : textOrder) {
            if (text.textSource == HelpText || text.textSource == SummaryText || text.textSource == TitleTagText)
                return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, text.text);
        }
        // FIXME: This is to keep the next blocks from returning duplicate text.
        // This behavior should be extended to all elements; not just SVG.
        return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, "");
    }

    if (!is<HTMLElement>(node) || coreObject->ariaRoleAttribute() != UnknownRole || coreObject->isImage())
        return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, accessibilityDescription(coreObject));

    // atk_table_get_summary returns an AtkObject. We have no summary object, so expose summary here.
    if (coreObject->roleValue() == TableRole) {
        const AtomicString& summary = downcast<HTMLTableElement>(*node).summary();
        if (!summary.isEmpty())
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, summary);
    }

    // The title attribute should be reliably available as the object's descripton.
    // We do not want to fall back on other attributes in its absence. See bug 25524.
    String title = downcast<HTMLElement>(*node).title();
    if (!title.isEmpty())
        return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, title);

    return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, accessibilityDescription(coreObject));
}

static void removeAtkRelationByType(AtkRelationSet* relationSet, AtkRelationType relationType)
{
    int count = atk_relation_set_get_n_relations(relationSet);
    for (int i = 0; i < count; i++) {
        AtkRelation* relation = atk_relation_set_get_relation(relationSet, i);
        if (atk_relation_get_relation_type(relation) == relationType) {
            atk_relation_set_remove(relationSet, relation);
            break;
        }
    }
}

static void setAtkRelationSetFromCoreObject(AccessibilityObject* coreObject, AtkRelationSet* relationSet)
{
    if (coreObject->isFieldset()) {
        AccessibilityObject* label = coreObject->titleUIElement();
        if (label) {
            removeAtkRelationByType(relationSet, ATK_RELATION_LABELLED_BY);
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
        }
        return;
    }

    if (coreObject->roleValue() == LegendRole) {
        for (AccessibilityObject* parent = coreObject->parentObjectUnignored(); parent; parent = parent->parentObjectUnignored()) {
            if (parent->isFieldset()) {
                atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, parent->wrapper());
                break;
            }
        }
        return;
    }

    if (coreObject->isControl()) {
        AccessibilityObject* label = coreObject->correspondingLabelForControlElement();
        if (label) {
            removeAtkRelationByType(relationSet, ATK_RELATION_LABELLED_BY);
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
        }
    } else {
        AccessibilityObject* control = coreObject->correspondingControlForLabelElement();
        if (control)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, control->wrapper());
    }

    // Check whether object supports aria-flowto
    if (coreObject->supportsARIAFlowTo()) {
        removeAtkRelationByType(relationSet, ATK_RELATION_FLOWS_TO);
        AccessibilityObject::AccessibilityChildrenVector ariaFlowToElements;
        coreObject->ariaFlowToElements(ariaFlowToElements);
        for (const auto& accessibilityObject : ariaFlowToElements)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_FLOWS_TO, accessibilityObject->wrapper());
    }

    // Check whether object supports aria-describedby. It provides an additional information for the user.
    if (coreObject->supportsARIADescribedBy()) {
        removeAtkRelationByType(relationSet, ATK_RELATION_DESCRIBED_BY);
        AccessibilityObject::AccessibilityChildrenVector ariaDescribedByElements;
        coreObject->ariaDescribedByElements(ariaDescribedByElements);
        for (const auto& accessibilityObject : ariaDescribedByElements)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DESCRIBED_BY, accessibilityObject->wrapper());
    }

    // Check whether object supports aria-controls. It provides information about elements that are controlled by the current object.
    if (coreObject->supportsARIAControls()) {
        removeAtkRelationByType(relationSet, ATK_RELATION_CONTROLLER_FOR);
        AccessibilityObject::AccessibilityChildrenVector ariaControls;
        coreObject->ariaControlsElements(ariaControls);
        for (const auto& accessibilityObject : ariaControls)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_CONTROLLER_FOR, accessibilityObject->wrapper());
    }
}

static gpointer webkitAccessibleParentClass = nullptr;

static bool isRootObject(AccessibilityObject* coreObject)
{
    // The root accessible object in WebCore is always an object with
    // the ScrolledArea role with one child with the WebArea role.
    if (!coreObject || !coreObject->isScrollView())
        return false;

    AccessibilityObject* firstChild = coreObject->firstChild();
    if (!firstChild || !firstChild->isWebArea())
        return false;

    return true;
}

static AtkObject* atkParentOfRootObject(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* coreParent = coreObject->parentObjectUnignored();

    // The top level object claims to not have a parent. This makes it
    // impossible for assistive technologies to ascend the accessible
    // hierarchy all the way to the application. (Bug 30489)
    if (!coreParent && isRootObject(coreObject)) {
        Document* document = coreObject->document();
        if (!document)
            return 0;
    }

    if (!coreParent)
        return 0;

    return coreParent->wrapper();
}

static AtkObject* webkitAccessibleGetParent(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    // Check first if the parent has been already set.
    AtkObject* accessibleParent = ATK_OBJECT_CLASS(webkitAccessibleParentClass)->get_parent(object);
    if (accessibleParent)
        return accessibleParent;

    // Parent not set yet, so try to find it in the hierarchy.
    AccessibilityObject* coreObject = core(object);
    if (!coreObject)
        return 0;

    AccessibilityObject* coreParent = coreObject->parentObjectUnignored();

    if (!coreParent && isRootObject(coreObject))
        return atkParentOfRootObject(object);

    if (!coreParent)
        return 0;

    return coreParent->wrapper();
}

static gint webkitAccessibleGetNChildren(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AccessibilityObject* coreObject = core(object);

    return coreObject->children().size();
}

static AtkObject* webkitAccessibleRefChild(AtkObject* object, gint index)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    if (index < 0)
        return 0;

    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* coreChild = nullptr;

    const AccessibilityObject::AccessibilityChildrenVector& children = coreObject->children();
    if (static_cast<size_t>(index) >= children.size())
        return 0;
    coreChild = children.at(index).get();

    if (!coreChild)
        return 0;

    AtkObject* child = coreChild->wrapper();
    atk_object_set_parent(child, object);
    g_object_ref(child);

    return child;
}

static gint webkitAccessibleGetIndexInParent(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), -1);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), -1);

    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* parent = coreObject->parentObjectUnignored();

    if (!parent && isRootObject(coreObject)) {
        AtkObject* atkParent = atkParentOfRootObject(object);
        if (!atkParent)
            return -1;

        unsigned count = atk_object_get_n_accessible_children(atkParent);
        for (unsigned i = 0; i < count; ++i) {
            AtkObject* child = atk_object_ref_accessible_child(atkParent, i);
            bool childIsObject = child == object;
            g_object_unref(child);
            if (childIsObject)
                return i;
        }
    }

    if (!parent)
        return -1;

    size_t index = parent->children().find(coreObject);
    return (index == WTF::notFound) ? -1 : index;
}

static AtkAttributeSet* webkitAccessibleGetAttributes(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AtkAttributeSet* attributeSet = nullptr;
#if PLATFORM(GTK)
    attributeSet = addToAtkAttributeSet(attributeSet, "toolkit", "WebKitGtk");
#elif PLATFORM(EFL)
    attributeSet = addToAtkAttributeSet(attributeSet, "toolkit", "WebKitEfl");
#endif

    AccessibilityObject* coreObject = core(object);
    if (!coreObject)
        return attributeSet;

    // Hack needed for WebKit2 tests because obtaining an element by its ID
    // cannot be done from the UIProcess. Assistive technologies have no need
    // for this information.
    Element* element = coreObject->element() ? coreObject->element() : coreObject->actionElement();
    if (element) {
        String tagName = element->tagName();
        if (!tagName.isEmpty())
            attributeSet = addToAtkAttributeSet(attributeSet, "tag", tagName.convertToASCIILowercase().utf8().data());
        String id = element->getIdAttribute().string();
        if (!id.isEmpty())
            attributeSet = addToAtkAttributeSet(attributeSet, "html-id", id.utf8().data());
    }

    int headingLevel = coreObject->headingLevel();
    if (headingLevel) {
        String value = String::number(headingLevel);
        attributeSet = addToAtkAttributeSet(attributeSet, "level", value.utf8().data());
    }

    if (coreObject->roleValue() == MathElementRole) {
        if (coreObject->isMathMultiscriptObject(PreSuperscript) || coreObject->isMathMultiscriptObject(PreSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "pre");
        else if (coreObject->isMathMultiscriptObject(PostSuperscript) || coreObject->isMathMultiscriptObject(PostSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "post");
    }

    // Set the 'layout-guess' attribute to help Assistive
    // Technologies know when an exposed table is not data table.
    if (is<AccessibilityTable>(*coreObject) && downcast<AccessibilityTable>(*coreObject).isExposableThroughAccessibility() && !coreObject->isDataTable())
        attributeSet = addToAtkAttributeSet(attributeSet, "layout-guess", "true");

    String placeholder = coreObject->placeholderValue();
    if (!placeholder.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "placeholder-text", placeholder.utf8().data());

    if (coreObject->ariaHasPopup())
        attributeSet = addToAtkAttributeSet(attributeSet, "haspopup", "true");

    AccessibilitySortDirection sortDirection = coreObject->sortDirection();
    if (sortDirection != SortDirectionNone) {
        // WAI-ARIA spec says to translate the value as is from the attribute.
        const AtomicString& sortAttribute = coreObject->getAttribute(HTMLNames::aria_sortAttr);
        attributeSet = addToAtkAttributeSet(attributeSet, "sort", sortAttribute.string().utf8().data());
    }

    if (coreObject->supportsARIAPosInSet())
        attributeSet = addToAtkAttributeSet(attributeSet, "posinset", String::number(coreObject->ariaPosInSet()).utf8().data());

    if (coreObject->supportsARIASetSize())
        attributeSet = addToAtkAttributeSet(attributeSet, "setsize", String::number(coreObject->ariaSetSize()).utf8().data());

    // According to the W3C Core Accessibility API Mappings 1.1, section 5.4.1 General Rules:
    // "User agents must expose the WAI-ARIA role string if the API supports a mechanism to do so."
    // In the case of ATK, the mechanism to do so is an object attribute pair (xml-roles:"string").
    // The computedRoleString is primarily for testing, and not limited to elements with ARIA roles.
    // Because the computedRoleString currently contains the ARIA role string, we'll use it for
    // both purposes, as the "computed-role" object attribute for all elements which have a value
    // and also via the "xml-roles" attribute for elements with ARIA, as well as for landmarks.
    String roleString = coreObject->computedRoleString();
    if (!roleString.isEmpty()) {
        if (coreObject->ariaRoleAttribute() != UnknownRole || coreObject->isLandmark())
            attributeSet = addToAtkAttributeSet(attributeSet, "xml-roles", roleString.utf8().data());
        attributeSet = addToAtkAttributeSet(attributeSet, "computed-role", roleString.utf8().data());
    }

    return attributeSet;
}

static AtkRole atkRole(AccessibilityObject* coreObject)
{
    AccessibilityRole role = coreObject->roleValue();
    switch (role) {
    case ApplicationAlertDialogRole:
    case ApplicationAlertRole:
        return ATK_ROLE_ALERT;
    case ApplicationDialogRole:
        return ATK_ROLE_DIALOG;
    case ApplicationStatusRole:
        return ATK_ROLE_STATUSBAR;
    case UnknownRole:
        return ATK_ROLE_UNKNOWN;
    case AudioRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        return ATK_ROLE_AUDIO;
#endif
    case VideoRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        return ATK_ROLE_VIDEO;
#endif
        return ATK_ROLE_EMBEDDED;
    case ButtonRole:
        return ATK_ROLE_PUSH_BUTTON;
    case SwitchRole:
    case ToggleButtonRole:
        return ATK_ROLE_TOGGLE_BUTTON;
    case RadioButtonRole:
        return ATK_ROLE_RADIO_BUTTON;
    case CheckBoxRole:
        return ATK_ROLE_CHECK_BOX;
    case SliderRole:
        return ATK_ROLE_SLIDER;
    case TabGroupRole:
    case TabListRole:
        return ATK_ROLE_PAGE_TAB_LIST;
    case TextFieldRole:
    case TextAreaRole:
    case SearchFieldRole:
        return ATK_ROLE_ENTRY;
    case StaticTextRole:
        return ATK_ROLE_TEXT;
    case OutlineRole:
    case TreeRole:
        return ATK_ROLE_TREE;
    case TreeItemRole:
        return ATK_ROLE_TREE_ITEM;
    case MenuBarRole:
        return ATK_ROLE_MENU_BAR;
    case MenuListPopupRole:
    case MenuRole:
        return ATK_ROLE_MENU;
    case MenuListOptionRole:
    case MenuItemRole:
        return ATK_ROLE_MENU_ITEM;
    case MenuItemCheckboxRole:
        return ATK_ROLE_CHECK_MENU_ITEM;
    case MenuItemRadioRole:
        return ATK_ROLE_RADIO_MENU_ITEM;
    case ColumnRole:
        // return ATK_ROLE_TABLE_COLUMN_HEADER; // Is this right?
        return ATK_ROLE_UNKNOWN; // Matches Mozilla
    case RowRole:
        return ATK_ROLE_TABLE_ROW;
    case ToolbarRole:
        return ATK_ROLE_TOOL_BAR;
    case BusyIndicatorRole:
        return ATK_ROLE_PROGRESS_BAR; // Is this right?
    case ProgressIndicatorRole:
        // return ATK_ROLE_SPIN_BUTTON; // Some confusion about this role in AccessibilityRenderObject.cpp
        return ATK_ROLE_PROGRESS_BAR;
    case WindowRole:
        return ATK_ROLE_WINDOW;
    case PopUpButtonRole:
    case ComboBoxRole:
        return ATK_ROLE_COMBO_BOX;
    case SplitGroupRole:
        return ATK_ROLE_SPLIT_PANE;
    case SplitterRole:
        return ATK_ROLE_SEPARATOR;
    case ColorWellRole:
#if PLATFORM(GTK)
        // ATK_ROLE_COLOR_CHOOSER is defined as a dialog (i.e. it's what appears when you push the button).
        return ATK_ROLE_PUSH_BUTTON;
#elif PLATFORM(EFL)
        return ATK_ROLE_COLOR_CHOOSER;
#endif
    case ListRole:
        return ATK_ROLE_LIST;
    case ScrollBarRole:
        return ATK_ROLE_SCROLL_BAR;
    case ScrollAreaRole:
        return ATK_ROLE_SCROLL_PANE;
    case GridRole:
    case TableRole:
        return ATK_ROLE_TABLE;
    case ApplicationRole:
        return ATK_ROLE_APPLICATION;
    case DocumentRegionRole:
    case GroupRole:
    case RadioGroupRole:
    case SVGRootRole:
    case TabPanelRole:
        return ATK_ROLE_PANEL;
    case RowHeaderRole:
        return ATK_ROLE_ROW_HEADER;
    case ColumnHeaderRole:
        return ATK_ROLE_COLUMN_HEADER;
    case CaptionRole:
        return ATK_ROLE_CAPTION;
    case CellRole:
    case GridCellRole:
        return coreObject->inheritsPresentationalRole() ? ATK_ROLE_SECTION : ATK_ROLE_TABLE_CELL;
    case LinkRole:
    case WebCoreLinkRole:
    case ImageMapLinkRole:
        return ATK_ROLE_LINK;
    case ImageMapRole:
        return ATK_ROLE_IMAGE_MAP;
    case ImageRole:
        return ATK_ROLE_IMAGE;
    case ListMarkerRole:
        return ATK_ROLE_TEXT;
    case DocumentArticleRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        return ATK_ROLE_ARTICLE;
#endif
    case DocumentRole:
        return ATK_ROLE_DOCUMENT_FRAME;
    case DocumentNoteRole:
        return ATK_ROLE_COMMENT;
    case HeadingRole:
        return ATK_ROLE_HEADING;
    case ListBoxRole:
        return ATK_ROLE_LIST_BOX;
    case ListItemRole:
        return coreObject->inheritsPresentationalRole() ? ATK_ROLE_SECTION : ATK_ROLE_LIST_ITEM;
    case ListBoxOptionRole:
        return ATK_ROLE_LIST_ITEM;
    case ParagraphRole:
        return ATK_ROLE_PARAGRAPH;
    case LabelRole:
    case LegendRole:
        return ATK_ROLE_LABEL;
    case BlockquoteRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        return ATK_ROLE_BLOCK_QUOTE;
#endif
    case DivRole:
    case PreRole:
    case SVGTextRole:
        return ATK_ROLE_SECTION;
    case FooterRole:
        return ATK_ROLE_FOOTER;
    case FormRole:
        return ATK_ROLE_FORM;
    case CanvasRole:
        return ATK_ROLE_CANVAS;
    case HorizontalRuleRole:
        return ATK_ROLE_SEPARATOR;
    case SpinButtonRole:
        return ATK_ROLE_SPIN_BUTTON;
    case TabRole:
        return ATK_ROLE_PAGE_TAB;
    case UserInterfaceTooltipRole:
        return ATK_ROLE_TOOL_TIP;
    case WebAreaRole:
        return ATK_ROLE_DOCUMENT_WEB;
    case LandmarkApplicationRole:
        return ATK_ROLE_EMBEDDED;
#if ATK_CHECK_VERSION(2, 11, 3)
    case ApplicationLogRole:
        return ATK_ROLE_LOG;
    case ApplicationMarqueeRole:
        return ATK_ROLE_MARQUEE;
    case ApplicationTimerRole:
        return ATK_ROLE_TIMER;
    case DefinitionRole:
        return ATK_ROLE_DEFINITION;
    case DocumentMathRole:
        return ATK_ROLE_MATH;
    case MathElementRole:
        if (coreObject->isMathRow())
            return ATK_ROLE_PANEL;
        if (coreObject->isMathTable())
            return ATK_ROLE_TABLE;
        if (coreObject->isMathTableRow())
            return ATK_ROLE_TABLE_ROW;
        if (coreObject->isMathTableCell())
            return ATK_ROLE_TABLE_CELL;
        if (coreObject->isMathSubscriptSuperscript() || coreObject->isMathMultiscript())
            return ATK_ROLE_SECTION;
#if ATK_CHECK_VERSION(2, 15, 4)
        if (coreObject->isMathFraction())
            return ATK_ROLE_MATH_FRACTION;
        if (coreObject->isMathSquareRoot() || coreObject->isMathRoot())
            return ATK_ROLE_MATH_ROOT;
        if (coreObject->isMathScriptObject(Subscript)
            || coreObject->isMathMultiscriptObject(PreSubscript) || coreObject->isMathMultiscriptObject(PostSubscript))
            return ATK_ROLE_SUBSCRIPT;
        if (coreObject->isMathScriptObject(Superscript)
            || coreObject->isMathMultiscriptObject(PreSuperscript) || coreObject->isMathMultiscriptObject(PostSuperscript))
            return ATK_ROLE_SUPERSCRIPT;
#endif
#if ATK_CHECK_VERSION(2, 15, 2)
        if (coreObject->isMathToken())
            return ATK_ROLE_STATIC;
#endif
        return ATK_ROLE_UNKNOWN;
    case LandmarkBannerRole:
    case LandmarkComplementaryRole:
    case LandmarkContentInfoRole:
    case LandmarkMainRole:
    case LandmarkNavigationRole:
    case LandmarkSearchRole:
        return ATK_ROLE_LANDMARK;
#endif
#if ATK_CHECK_VERSION(2, 11, 4)
    case DescriptionListRole:
        return ATK_ROLE_DESCRIPTION_LIST;
    case DescriptionListTermRole:
        return ATK_ROLE_DESCRIPTION_TERM;
    case DescriptionListDetailRole:
        return ATK_ROLE_DESCRIPTION_VALUE;
#endif
#if ATK_CHECK_VERSION(2, 15, 2)
    case InlineRole:
    case SVGTextPathRole:
    case SVGTSpanRole:
        return ATK_ROLE_STATIC;
#endif
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkitAccessibleGetRole(AtkObject* object)
{
    // ATK_ROLE_UNKNOWN should only be applied in cases where there is a valid
    // WebCore accessible object for which the platform role mapping is unknown.
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), ATK_ROLE_INVALID);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), ATK_ROLE_INVALID);

    AccessibilityObject* coreObject = core(object);

    if (!coreObject)
        return ATK_ROLE_INVALID;

    // Note: Why doesn't WebCore have a password field for this
    if (coreObject->isPasswordField())
        return ATK_ROLE_PASSWORD_TEXT;

    return atkRole(coreObject);
}

static bool isTextWithCaret(AccessibilityObject* coreObject)
{
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return false;

    Document* document = coreObject->document();
    if (!document)
        return false;

    Frame* frame = document->frame();
    if (!frame)
        return false;

    if (!frame->settings().caretBrowsingEnabled())
        return false;

    // Check text objects and paragraphs only.
    AtkObject* axObject = coreObject->wrapper();
    AtkRole role = axObject ? atk_object_get_role(axObject) : ATK_ROLE_INVALID;
    if (role != ATK_ROLE_TEXT && role != ATK_ROLE_PARAGRAPH)
        return false;

    // Finally, check whether the caret is set in the current object.
    VisibleSelection selection = coreObject->selection();
    if (!selection.isCaret())
        return false;

    return selectionBelongsToObject(coreObject, selection);
}

static void setAtkStateSetFromCoreObject(AccessibilityObject* coreObject, AtkStateSet* stateSet)
{
    AccessibilityObject* parent = coreObject->parentObject();
    bool isListBoxOption = parent && parent->isListBox();

    // Please keep the state list in alphabetical order
    if (isListBoxOption && coreObject->isSelectedOptionActive())
        atk_state_set_add_state(stateSet, ATK_STATE_ACTIVE);

    if (coreObject->isChecked())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKED);

    // FIXME: isReadOnly does not seem to do the right thing for
    // controls, so check explicitly for them. In addition, because
    // isReadOnly is false for listBoxOptions, we need to add one
    // more check so that we do not present them as being "editable".
    if ((!coreObject->isReadOnly()
        || (coreObject->isControl() && coreObject->canSetValueAttribute()))
        && !isListBoxOption)
        atk_state_set_add_state(stateSet, ATK_STATE_EDITABLE);

    // FIXME: Put both ENABLED and SENSITIVE together here for now
    if (coreObject->isEnabled()) {
        atk_state_set_add_state(stateSet, ATK_STATE_ENABLED);
        atk_state_set_add_state(stateSet, ATK_STATE_SENSITIVE);
    }

    if (coreObject->canSetExpandedAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_EXPANDABLE);

    if (coreObject->isExpanded())
        atk_state_set_add_state(stateSet, ATK_STATE_EXPANDED);

    if (coreObject->canSetFocusAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    if (coreObject->isFocused() || isTextWithCaret(coreObject))
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);

    if (coreObject->orientation() == AccessibilityOrientationHorizontal)
        atk_state_set_add_state(stateSet, ATK_STATE_HORIZONTAL);
    else if (coreObject->orientation() == AccessibilityOrientationVertical)
        atk_state_set_add_state(stateSet, ATK_STATE_VERTICAL);

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);

    if (coreObject->isCheckboxOrRadio() || coreObject->isMenuItem()) {
        if (coreObject->checkboxOrRadioValue() == ButtonStateMixed)
            atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);
    }

    if (coreObject->invalidStatus() != "false")
        atk_state_set_add_state(stateSet, ATK_STATE_INVALID_ENTRY);

    if (coreObject->isMultiSelectable())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

    if (coreObject->isRequired())
        atk_state_set_add_state(stateSet, ATK_STATE_REQUIRED);

    // TODO: ATK_STATE_SELECTABLE_TEXT

    if (coreObject->canSetSelectedAttribute()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTABLE);
        // Items in focusable lists have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on
        // the former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);
    }

    if (coreObject->isSelected()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTED);
        // Items in focusable lists have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on the
        // former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);
    }

    // FIXME: Group both SHOWING and VISIBLE here for now
    // Not sure how to handle this in WebKit, see bug
    // http://bugzilla.gnome.org/show_bug.cgi?id=509650 for other
    // issues with SHOWING vs VISIBLE.
    if (!coreObject->isOffScreen()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
        atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
    }

    // Mutually exclusive, so we group these two
    if (coreObject->roleValue() == TextFieldRole)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);
    else if (coreObject->roleValue() == TextAreaRole)
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);

    // TODO: ATK_STATE_SENSITIVE

    if (coreObject->isVisited())
        atk_state_set_add_state(stateSet, ATK_STATE_VISITED);
}

static AtkStateSet* webkitAccessibleRefStateSet(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);

    AtkStateSet* stateSet = ATK_OBJECT_CLASS(webkitAccessibleParentClass)->ref_state_set(object);
    AccessibilityObject* coreObject = core(object);

    // Make sure the layout is updated to really know whether the object
    // is defunct or not, so we can return the proper state.
    coreObject->updateBackingStore();

    if (coreObject == fallbackObject()) {
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    // Text objects must be focusable.
    AtkRole role = atk_object_get_role(object);
    if (role == ATK_ROLE_TEXT || role == ATK_ROLE_PARAGRAPH)
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    setAtkStateSetFromCoreObject(coreObject, stateSet);
    return stateSet;
}

static AtkRelationSet* webkitAccessibleRefRelationSet(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AtkRelationSet* relationSet = ATK_OBJECT_CLASS(webkitAccessibleParentClass)->ref_relation_set(object);
    AccessibilityObject* coreObject = core(object);

    setAtkRelationSetFromCoreObject(coreObject, relationSet);

    return relationSet;
}

static void webkitAccessibleInit(AtkObject* object, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkitAccessibleParentClass)->initialize)
        ATK_OBJECT_CLASS(webkitAccessibleParentClass)->initialize(object, data);

    WebKitAccessible* accessible = WEBKIT_ACCESSIBLE(object);
    accessible->m_object = reinterpret_cast<AccessibilityObject*>(data);
    accessible->priv = WEBKIT_ACCESSIBLE_GET_PRIVATE(accessible);
}

static const gchar* webkitAccessibleGetObjectLocale(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    AccessibilityObject* coreObject = core(object);
    if (!coreObject)
        return 0;

    if (ATK_IS_DOCUMENT(object)) {
        // TODO: Should we fall back on lang xml:lang when the following comes up empty?
        String language = coreObject->language();
        if (!language.isEmpty())
            return cacheAndReturnAtkProperty(object, AtkCachedDocumentLocale, language);

    } else if (ATK_IS_TEXT(object)) {
        const gchar* locale = nullptr;

        AtkAttributeSet* textAttributes = atk_text_get_default_attributes(ATK_TEXT(object));
        for (AtkAttributeSet* attributes = textAttributes; attributes; attributes = attributes->next) {
            AtkAttribute* atkAttribute = static_cast<AtkAttribute*>(attributes->data);
            if (!strcmp(atkAttribute->name, atk_text_attribute_get_name(ATK_TEXT_ATTR_LANGUAGE))) {
                locale = cacheAndReturnAtkProperty(object, AtkCachedDocumentLocale, String::fromUTF8(atkAttribute->value));
                break;
            }
        }
        atk_attribute_set_free(textAttributes);

        return locale;
    }

    return 0;
}

static void webkitAccessibleFinalize(GObject* object)
{
    G_OBJECT_CLASS(webkitAccessibleParentClass)->finalize(object);
}

static void webkitAccessibleClassInit(AtkObjectClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    webkitAccessibleParentClass = g_type_class_peek_parent(klass);

    gobjectClass->finalize = webkitAccessibleFinalize;

    klass->initialize = webkitAccessibleInit;
    klass->get_name = webkitAccessibleGetName;
    klass->get_description = webkitAccessibleGetDescription;
    klass->get_parent = webkitAccessibleGetParent;
    klass->get_n_children = webkitAccessibleGetNChildren;
    klass->ref_child = webkitAccessibleRefChild;
    klass->get_role = webkitAccessibleGetRole;
    klass->ref_state_set = webkitAccessibleRefStateSet;
    klass->get_index_in_parent = webkitAccessibleGetIndexInParent;
    klass->get_attributes = webkitAccessibleGetAttributes;
    klass->ref_relation_set = webkitAccessibleRefRelationSet;
    klass->get_object_locale = webkitAccessibleGetObjectLocale;

    g_type_class_add_private(klass, sizeof(WebKitAccessiblePrivate));
}

GType
webkitAccessibleGetType(void)
{
    static volatile gsize typeVolatile = 0;

    if (g_once_init_enter(&typeVolatile)) {
        static const GTypeInfo tinfo = {
            sizeof(WebKitAccessibleClass),
            (GBaseInitFunc) 0,
            (GBaseFinalizeFunc) 0,
            (GClassInitFunc) webkitAccessibleClassInit,
            (GClassFinalizeFunc) 0,
            0, /* class data */
            sizeof(WebKitAccessible), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) 0,
            0 /* value table */
        };

        GType type = g_type_register_static(ATK_TYPE_OBJECT, "WebKitAccessible", &tinfo, GTypeFlags(0));
        g_once_init_leave(&typeVolatile, type);
    }

    return typeVolatile;
}

static const GInterfaceInfo AtkInterfacesInitFunctions[] = {
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleActionInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleSelectionInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleEditableTextInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleTextInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleComponentInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleImageInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleTableInterfaceInit), 0, 0},
#if ATK_CHECK_VERSION(2,11,90)
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleTableCellInterfaceInit), 0, 0},
#endif
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleHypertextInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleHyperlinkImplInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleDocumentInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleValueInterfaceInit), 0, 0}
};

enum WAIType {
    WAIAction,
    WAISelection,
    WAIEditableText,
    WAIText,
    WAIComponent,
    WAIImage,
    WAITable,
#if ATK_CHECK_VERSION(2,11,90)
    WAITableCell,
#endif
    WAIHypertext,
    WAIHyperlink,
    WAIDocument,
    WAIValue,
};

static GType GetAtkInterfaceTypeFromWAIType(WAIType type)
{
    switch (type) {
    case WAIAction:
        return ATK_TYPE_ACTION;
    case WAISelection:
        return ATK_TYPE_SELECTION;
    case WAIEditableText:
        return ATK_TYPE_EDITABLE_TEXT;
    case WAIText:
        return ATK_TYPE_TEXT;
    case WAIComponent:
        return ATK_TYPE_COMPONENT;
    case WAIImage:
        return ATK_TYPE_IMAGE;
    case WAITable:
        return ATK_TYPE_TABLE;
#if ATK_CHECK_VERSION(2,11,90)
    case WAITableCell:
        return ATK_TYPE_TABLE_CELL;
#endif
    case WAIHypertext:
        return ATK_TYPE_HYPERTEXT;
    case WAIHyperlink:
        return ATK_TYPE_HYPERLINK_IMPL;
    case WAIDocument:
        return ATK_TYPE_DOCUMENT;
    case WAIValue:
        return ATK_TYPE_VALUE;
    }

    return G_TYPE_INVALID;
}

static bool roleIsTextType(AccessibilityRole role)
{
    return role == ParagraphRole || role == HeadingRole || role == DivRole || role == CellRole
        || role == LinkRole || role == WebCoreLinkRole || role == ListItemRole || role == PreRole
        || role == GridCellRole;
}

static guint16 getInterfaceMaskFromObject(AccessibilityObject* coreObject)
{
    guint16 interfaceMask = 0;

    // Component interface is always supported
    interfaceMask |= 1 << WAIComponent;

    AccessibilityRole role = coreObject->roleValue();

    // Action
    // As the implementation of the AtkAction interface is a very
    // basic one (just relays in executing the default action for each
    // object, and only supports having one action per object), it is
    // better just to implement this interface for every instance of
    // the WebKitAccessible class and let WebCore decide what to do.
    interfaceMask |= 1 << WAIAction;

    // Selection
    if (coreObject->isListBox() || coreObject->isMenuList())
        interfaceMask |= 1 << WAISelection;

    // Get renderer if available.
    RenderObject* renderer = nullptr;
    if (coreObject->isAccessibilityRenderObject())
        renderer = coreObject->renderer();

    // Hyperlink (links and embedded objects).
    if (coreObject->isLink() || (renderer && renderer->isReplaced()))
        interfaceMask |= 1 << WAIHyperlink;

    // Text, Editable Text & Hypertext
    if (role == StaticTextRole || coreObject->isMenuListOption())
        interfaceMask |= 1 << WAIText;
    else if (coreObject->isTextControl()) {
        interfaceMask |= 1 << WAIText;
        if (!coreObject->isReadOnly())
            interfaceMask |= 1 << WAIEditableText;
    } else if (!coreObject->isWebArea()) {
        if (role != TableRole) {
            interfaceMask |= 1 << WAIHypertext;
            if ((renderer && renderer->childrenInline()) || roleIsTextType(role) || coreObject->isMathToken())
                interfaceMask |= 1 << WAIText;
        }

        // Add the TEXT interface for list items whose
        // first accessible child has a text renderer
        if (role == ListItemRole) {
            const AccessibilityObject::AccessibilityChildrenVector& children = coreObject->children();
            if (children.size()) {
                AccessibilityObject* axRenderChild = children.at(0).get();
                interfaceMask |= getInterfaceMaskFromObject(axRenderChild);
            }
        }
    }

    // Image
    if (coreObject->isImage())
        interfaceMask |= 1 << WAIImage;

    // Table
    if (role == TableRole || role == GridRole)
        interfaceMask |= 1 << WAITable;

#if ATK_CHECK_VERSION(2,11,90)
    if (role == CellRole || role == ColumnHeaderRole || role == RowHeaderRole)
        interfaceMask |= 1 << WAITableCell;
#endif

    // Document
    if (role == WebAreaRole)
        interfaceMask |= 1 << WAIDocument;

    // Value
    if (role == SliderRole || role == SpinButtonRole || role == ScrollBarRole || role == ProgressIndicatorRole)
        interfaceMask |= 1 << WAIValue;

#if ENABLE(INPUT_TYPE_COLOR)
    // Color type.
    if (role == ColorWellRole)
        interfaceMask |= 1 << WAIText;
#endif

    return interfaceMask;
}

static const char* getUniqueAccessibilityTypeName(guint16 interfaceMask)
{
#define WAI_TYPE_NAME_LEN (30) /* Enough for prefix + 5 hex characters (max) */
    static char name[WAI_TYPE_NAME_LEN + 1];

    g_sprintf(name, "WAIType%x", interfaceMask);
    name[WAI_TYPE_NAME_LEN] = '\0';

    return name;
}

static GType getAccessibilityTypeFromObject(AccessibilityObject* coreObject)
{
    static const GTypeInfo typeInfo = {
        sizeof(WebKitAccessibleClass),
        (GBaseInitFunc) 0,
        (GBaseFinalizeFunc) 0,
        (GClassInitFunc) 0,
        (GClassFinalizeFunc) 0,
        0, /* class data */
        sizeof(WebKitAccessible), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) 0,
        0 /* value table */
    };

    guint16 interfaceMask = getInterfaceMaskFromObject(coreObject);
    const char* atkTypeName = getUniqueAccessibilityTypeName(interfaceMask);
    GType type = g_type_from_name(atkTypeName);
    if (type)
        return type;

    type = g_type_register_static(WEBKIT_TYPE_ACCESSIBLE, atkTypeName, &typeInfo, GTypeFlags(0));
    for (guint i = 0; i < G_N_ELEMENTS(AtkInterfacesInitFunctions); i++) {
        if (interfaceMask & (1 << i))
            g_type_add_interface_static(type,
                GetAtkInterfaceTypeFromWAIType(static_cast<WAIType>(i)),
                &AtkInterfacesInitFunctions[i]);
    }

    return type;
}

WebKitAccessible* webkitAccessibleNew(AccessibilityObject* coreObject)
{
    GType type = getAccessibilityTypeFromObject(coreObject);
    AtkObject* object = static_cast<AtkObject*>(g_object_new(type, 0));

    atk_object_initialize(object, coreObject);

    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject* webkitAccessibleGetAccessibilityObject(WebKitAccessible* accessible)
{
    return accessible->m_object;
}

void webkitAccessibleDetach(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);

    if (accessible->m_object->roleValue() == WebAreaRole)
        atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_DEFUNCT, true);

    // We replace the WebCore AccessibilityObject with a fallback object that
    // provides default implementations to avoid repetitive null-checking after
    // detachment.
    accessible->m_object = fallbackObject();
}

bool webkitAccessibleIsDetached(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);
    return accessible->m_object == fallbackObject();
}

AccessibilityObject* objectFocusedAndCaretOffsetUnignored(AccessibilityObject* referenceObject, int& offset)
{
    // Indication that something bogus has transpired.
    offset = -1;

    Document* document = referenceObject->document();
    if (!document)
        return 0;

    Node* focusedNode = referenceObject->selection().end().containerNode();
    if (!focusedNode)
        return 0;

    RenderObject* focusedRenderer = focusedNode->renderer();
    if (!focusedRenderer)
        return 0;

    AccessibilityObject* focusedObject = document->axObjectCache()->getOrCreate(focusedRenderer);
    if (!focusedObject)
        return 0;

    // Look for the actual (not ignoring accessibility) selected object.
    AccessibilityObject* firstUnignoredParent = focusedObject;
    if (firstUnignoredParent->accessibilityIsIgnored())
        firstUnignoredParent = firstUnignoredParent->parentObjectUnignored();
    if (!firstUnignoredParent)
        return 0;

    // Don't ignore links if the offset is being requested for a link
    // or if the link is a block.
    if (!referenceObject->isLink() && firstUnignoredParent->isLink()
        && !(firstUnignoredParent->renderer() && !firstUnignoredParent->renderer()->isInline()))
        firstUnignoredParent = firstUnignoredParent->parentObjectUnignored();
    if (!firstUnignoredParent)
        return 0;

    // The reference object must either coincide with the focused
    // object being considered, or be a descendant of it.
    if (referenceObject->isDescendantOfObject(firstUnignoredParent))
        referenceObject = firstUnignoredParent;

    Node* startNode = nullptr;
    if (firstUnignoredParent != referenceObject || firstUnignoredParent->isTextControl()) {
        // We need to use the first child's node of the reference
        // object as the start point to calculate the caret offset
        // because we want it to be relative to the object of
        // reference, not just to the focused object (which could have
        // previous siblings which should be taken into account too).
        AccessibilityObject* axFirstChild = referenceObject->firstChild();
        if (axFirstChild)
            startNode = axFirstChild->node();
    }
    // Getting the Position of a PseudoElement now triggers an assertion.
    // This can occur when clicking on empty space in a render block.
    if (!startNode || startNode->isPseudoElement())
        startNode = firstUnignoredParent->node();

    // Check if the node for the first parent object not ignoring
    // accessibility is null again before using it. This might happen
    // with certain kind of accessibility objects, such as the root
    // one (the scroller containing the webArea object).
    if (!startNode)
        return 0;

    VisiblePosition startPosition = VisiblePosition(positionBeforeNode(startNode), DOWNSTREAM);
    VisiblePosition endPosition = firstUnignoredParent->selection().visibleEnd();

    if (startPosition == endPosition)
        offset = 0;
    else if (!isStartOfLine(endPosition)) {
        RefPtr<Range> range = makeRange(startPosition, endPosition.previous());
        offset = TextIterator::rangeLength(range.get(), true) + 1;
    } else {
        RefPtr<Range> range = makeRange(startPosition, endPosition);
        offset = TextIterator::rangeLength(range.get(), true);
    }

    return firstUnignoredParent;
}

const char* cacheAndReturnAtkProperty(AtkObject* object, AtkCachedProperty property, String value)
{
    WebKitAccessiblePrivate* priv = WEBKIT_ACCESSIBLE(object)->priv;
    CString* propertyPtr = nullptr;

    switch (property) {
    case AtkCachedAccessibleName:
        propertyPtr = &priv->accessibleName;
        break;

    case AtkCachedAccessibleDescription:
        propertyPtr = &priv->accessibleDescription;
        break;

    case AtkCachedActionName:
        propertyPtr = &priv->actionName;
        break;

    case AtkCachedActionKeyBinding:
        propertyPtr = &priv->actionKeyBinding;
        break;

    case AtkCachedDocumentLocale:
        propertyPtr = &priv->documentLocale;
        break;

    case AtkCachedDocumentType:
        propertyPtr = &priv->documentType;
        break;

    case AtkCachedDocumentEncoding:
        propertyPtr = &priv->documentEncoding;
        break;

    case AtkCachedDocumentURI:
        propertyPtr = &priv->documentURI;
        break;

    case AtkCachedImageDescription:
        propertyPtr = &priv->imageDescription;
        break;

    default:
        ASSERT_NOT_REACHED();
    }

    // Don't invalidate old memory if not stricly needed, since other
    // callers might be still holding on to it.
    if (*propertyPtr != value.utf8())
        *propertyPtr = value.utf8();

    return (*propertyPtr).data();
}

#endif // HAVE(ACCESSIBILITY)
