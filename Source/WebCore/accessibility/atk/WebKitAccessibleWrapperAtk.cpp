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
#include "AccessibilityTableCell.h"
#include "AccessibilityTableRow.h"
#include "Document.h"
#include "Editing.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "HostWindow.h"
#include "RenderAncestorIterator.h"
#include "RenderBlock.h"
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

    Vector<AccessibilityText> textOrder;
    core(object)->accessibilityText(textOrder);

    for (const auto& text : textOrder) {
        // FIXME: This check is here because AccessibilityNodeObject::titleElementText()
        // appends an empty String for the LabelByElementText source when there is a
        // titleUIElement(). Removing this check makes some fieldsets lose their name.
        if (text.text.isEmpty())
            continue;

        // WebCore Accessibility should provide us with the text alternative computation
        // in the order defined by that spec. So take the first thing that our platform
        // does not expose via the AtkObject description.
        if (text.textSource != HelpText && text.textSource != SummaryText)
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, text.text);
    }

    return cacheAndReturnAtkProperty(object, AtkCachedAccessibleName, "");
}

static const gchar* webkitAccessibleGetDescription(AtkObject* object)
{
    g_return_val_if_fail(WEBKIT_IS_ACCESSIBLE(object), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(object), 0);

    Vector<AccessibilityText> textOrder;
    core(object)->accessibilityText(textOrder);

    bool nameTextAvailable = false;
    for (const auto& text : textOrder) {
        // WebCore Accessibility should provide us with the text alternative computation
        // in the order defined by that spec. So take the first thing that our platform
        // does not expose via the AtkObject name.
        if (text.textSource == HelpText || text.textSource == SummaryText)
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, text.text);

        // If there is no other text alternative, the title tag contents will have been
        // used for the AtkObject name. We don't want to duplicate it here.
        if (text.textSource == TitleTagText && nameTextAvailable)
            return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, text.text);

        nameTextAvailable = true;
    }

    return cacheAndReturnAtkProperty(object, AtkCachedAccessibleDescription, "");
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
    // Elements with aria-labelledby should have the labelled-by relation as per the ARIA AAM spec.
    // Controls with a label element and fieldsets with a legend element should also use this relation
    // as per the HTML AAM spec. The reciprocal label-for relation should also be used.
    removeAtkRelationByType(relationSet, ATK_RELATION_LABELLED_BY);
    removeAtkRelationByType(relationSet, ATK_RELATION_LABEL_FOR);
    if (coreObject->isControl()) {
        if (AccessibilityObject* label = coreObject->correspondingLabelForControlElement())
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
    } else if (coreObject->isFieldset()) {
        if (AccessibilityObject* label = coreObject->titleUIElement())
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
    } else if (coreObject->roleValue() == LegendRole) {
        if (RenderBlock* renderFieldset = ancestorsOfType<RenderBlock>(*coreObject->renderer()).first()) {
            if (renderFieldset->isFieldset()) {
                AccessibilityObject* fieldset = coreObject->axObjectCache()->getOrCreate(renderFieldset);
                atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, fieldset->wrapper());
            }
        }
    } else if (AccessibilityObject* control = coreObject->correspondingControlForLabelElement()) {
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, control->wrapper());
    } else {
        AccessibilityObject::AccessibilityChildrenVector ariaLabelledByElements;
        coreObject->ariaLabelledByElements(ariaLabelledByElements);
        for (const auto& accessibilityObject : ariaLabelledByElements)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, accessibilityObject->wrapper());
    }

    // Elements referenced by aria-labelledby should have the label-for relation as per the ARIA AAM spec.
    AccessibilityObject::AccessibilityChildrenVector labels;
    coreObject->ariaLabelledByReferencingElements(labels);
    for (const auto& accessibilityObject : labels)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, accessibilityObject->wrapper());

    // Elements with aria-flowto should have the flows-to relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_FLOWS_TO);
    AccessibilityObject::AccessibilityChildrenVector ariaFlowToElements;
    coreObject->ariaFlowToElements(ariaFlowToElements);
    for (const auto& accessibilityObject : ariaFlowToElements)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_FLOWS_TO, accessibilityObject->wrapper());

    // Elements referenced by aria-flowto should have the flows-from relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_FLOWS_FROM);
    AccessibilityObject::AccessibilityChildrenVector flowFrom;
    coreObject->ariaFlowToReferencingElements(flowFrom);
    for (const auto& accessibilityObject : flowFrom)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_FLOWS_FROM, accessibilityObject->wrapper());

    // Elements with aria-describedby should have the described-by relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DESCRIBED_BY);
    AccessibilityObject::AccessibilityChildrenVector ariaDescribedByElements;
    coreObject->ariaDescribedByElements(ariaDescribedByElements);
    for (const auto& accessibilityObject : ariaDescribedByElements)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DESCRIBED_BY, accessibilityObject->wrapper());

    // Elements referenced by aria-describedby should have the description-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DESCRIPTION_FOR);
    AccessibilityObject::AccessibilityChildrenVector describers;
    coreObject->ariaDescribedByReferencingElements(describers);
    for (const auto& accessibilityObject : describers)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DESCRIPTION_FOR, accessibilityObject->wrapper());

    // Elements with aria-controls should have the controller-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_CONTROLLER_FOR);
    AccessibilityObject::AccessibilityChildrenVector ariaControls;
    coreObject->ariaControlsElements(ariaControls);
    for (const auto& accessibilityObject : ariaControls)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_CONTROLLER_FOR, accessibilityObject->wrapper());

    // Elements referenced by aria-controls should have the controlled-by relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_CONTROLLED_BY);
    AccessibilityObject::AccessibilityChildrenVector controllers;
    coreObject->ariaControlsReferencingElements(controllers);
    for (const auto& accessibilityObject : controllers)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_CONTROLLED_BY, accessibilityObject->wrapper());

    // Elements with aria-owns should have the node-parent-of relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_NODE_PARENT_OF);
    AccessibilityObject::AccessibilityChildrenVector ariaOwns;
    coreObject->ariaOwnsElements(ariaOwns);
    for (const auto& accessibilityObject : ariaOwns)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_NODE_PARENT_OF, accessibilityObject->wrapper());

    // Elements referenced by aria-owns should have the node-child-of relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_NODE_CHILD_OF);
    AccessibilityObject::AccessibilityChildrenVector owners;
    coreObject->ariaOwnsReferencingElements(owners);
    for (const auto& accessibilityObject : owners)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_NODE_CHILD_OF, accessibilityObject->wrapper());

#if ATK_CHECK_VERSION(2, 25, 2)
    // Elements with aria-details should have the details relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DETAILS);
    AccessibilityObject::AccessibilityChildrenVector ariaDetails;
    coreObject->ariaDetailsElements(ariaDetails);
    for (const auto& accessibilityObject : ariaDetails)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DETAILS, accessibilityObject->wrapper());

    // Elements referenced by aria-details should have the details-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DETAILS_FOR);
    AccessibilityObject::AccessibilityChildrenVector details;
    coreObject->ariaDetailsReferencingElements(details);
    for (const auto& accessibilityObject : details)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DETAILS_FOR, accessibilityObject->wrapper());

    // Elements with aria-errormessage should have the error-message relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_ERROR_MESSAGE);
    AccessibilityObject::AccessibilityChildrenVector ariaErrorMessage;
    coreObject->ariaErrorMessageElements(ariaErrorMessage);
    for (const auto& accessibilityObject : ariaErrorMessage)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_ERROR_MESSAGE, accessibilityObject->wrapper());

    // Elements referenced by aria-errormessage should have the error-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_ERROR_FOR);
    AccessibilityObject::AccessibilityChildrenVector errors;
    coreObject->ariaErrorMessageReferencingElements(errors);
    for (const auto& accessibilityObject : errors)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_ERROR_FOR, accessibilityObject->wrapper());
#endif
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

    int level = coreObject->isHeading() ? coreObject->headingLevel() : coreObject->hierarchicalLevel();
    if (level) {
        String value = String::number(level);
        attributeSet = addToAtkAttributeSet(attributeSet, "level", value.utf8().data());
    }

    if (coreObject->roleValue() == MathElementRole) {
        if (coreObject->isMathMultiscriptObject(PreSuperscript) || coreObject->isMathMultiscriptObject(PreSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "pre");
        else if (coreObject->isMathMultiscriptObject(PostSuperscript) || coreObject->isMathMultiscriptObject(PostSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "post");
    }

    if (is<AccessibilityTable>(*coreObject) && downcast<AccessibilityTable>(*coreObject).isExposableThroughAccessibility()) {
        auto& table = downcast<AccessibilityTable>(*coreObject);
        int rowCount = table.ariaRowCount();
        if (rowCount)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowcount", String::number(rowCount).utf8().data());

        int columnCount = table.ariaColumnCount();
        if (columnCount)
            attributeSet = addToAtkAttributeSet(attributeSet, "colcount", String::number(columnCount).utf8().data());
    } else if (is<AccessibilityTableRow>(*coreObject)) {
        auto& row = downcast<AccessibilityTableRow>(*coreObject);
        int rowIndex = row.ariaRowIndex();
        if (rowIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowindex", String::number(rowIndex).utf8().data());
    } else if (is<AccessibilityTableCell>(*coreObject)) {
        auto& cell = downcast<AccessibilityTableCell>(*coreObject);
        int rowIndex = cell.ariaRowIndex();
        if (rowIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowindex", String::number(rowIndex).utf8().data());

        int columnIndex = cell.ariaColumnIndex();
        if (columnIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "colindex", String::number(columnIndex).utf8().data());

        int rowSpan = cell.ariaRowSpan();
        if (rowSpan != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowspan", String::number(rowSpan).utf8().data());

        int columnSpan = cell.ariaColumnSpan();
        if (columnSpan != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "colspan", String::number(columnSpan).utf8().data());
    }

    String placeholder = coreObject->placeholderValue();
    if (!placeholder.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "placeholder-text", placeholder.utf8().data());

    if (coreObject->supportsARIAAutoComplete())
        attributeSet = addToAtkAttributeSet(attributeSet, "autocomplete", coreObject->ariaAutoCompleteValue().utf8().data());

    if (coreObject->supportsARIAHasPopup())
        attributeSet = addToAtkAttributeSet(attributeSet, "haspopup", coreObject->ariaPopupValue().utf8().data());

    if (coreObject->supportsARIACurrent())
        attributeSet = addToAtkAttributeSet(attributeSet, "current", coreObject->ariaCurrentValue().utf8().data());

    // The Core AAM states that an explicitly-set value should be exposed, including "none".
    if (coreObject->hasAttribute(HTMLNames::aria_sortAttr)) {
        switch (coreObject->sortDirection()) {
        case SortDirectionInvalid:
            break;
        case SortDirectionAscending:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "ascending");
            break;
        case SortDirectionDescending:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "descending");
            break;
        case SortDirectionOther:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "other");
            break;
        default:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "none");
        }
    }

    if (coreObject->supportsARIAPosInSet())
        attributeSet = addToAtkAttributeSet(attributeSet, "posinset", String::number(coreObject->ariaPosInSet()).utf8().data());

    if (coreObject->supportsARIASetSize())
        attributeSet = addToAtkAttributeSet(attributeSet, "setsize", String::number(coreObject->ariaSetSize()).utf8().data());

    String isReadOnly = coreObject->ariaReadOnlyValue();
    if (!isReadOnly.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "readonly", isReadOnly.utf8().data());

    String valueDescription = coreObject->valueDescription();
    if (!valueDescription.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "valuetext", valueDescription.utf8().data());

    // According to the W3C Core Accessibility API Mappings 1.1, section 5.4.1 General Rules:
    // "User agents must expose the WAI-ARIA role string if the API supports a mechanism to do so."
    // In the case of ATK, the mechanism to do so is an object attribute pair (xml-roles:"string").
    // We cannot use the computedRoleString for this purpose because it is not limited to elements
    // with ARIA roles, and it might not contain the actual ARIA role value (e.g. DPub ARIA).
    String roleString = coreObject->getAttribute(HTMLNames::roleAttr);
    if (!roleString.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "xml-roles", roleString.utf8().data());

    String computedRoleString = coreObject->computedRoleString();
    if (!computedRoleString.isEmpty()) {
        attributeSet = addToAtkAttributeSet(attributeSet, "computed-role", computedRoleString.utf8().data());

        // The HTML AAM maps several elements to ARIA landmark roles. In order for the type of landmark
        // to be obtainable in the same fashion as an ARIA landmark, fall back on the computedRoleString.
        if (coreObject->ariaRoleAttribute() == UnknownRole && coreObject->isLandmark())
            attributeSet = addToAtkAttributeSet(attributeSet, "xml-roles", computedRoleString.utf8().data());
    }

    String roleDescription = coreObject->roleDescription();
    if (!roleDescription.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "roledescription", roleDescription.utf8().data());

    // We need to expose the live region attributes even if the live region is currently disabled/off.
    if (auto liveContainer = coreObject->ariaLiveRegionAncestor(false)) {
        String liveStatus = liveContainer->ariaLiveRegionStatus();
        String relevant = liveContainer->ariaLiveRegionRelevant();
        bool isAtomic = liveContainer->ariaLiveRegionAtomic();
        String liveRole = roleString.isEmpty() ? computedRoleString : roleString;

        // According to the Core AAM, we need to expose the above properties with "container-" prefixed
        // object attributes regardless of whether the container is this object, or an ancestor of it.
        attributeSet = addToAtkAttributeSet(attributeSet, "container-live", liveStatus.utf8().data());
        attributeSet = addToAtkAttributeSet(attributeSet, "container-relevant", relevant.utf8().data());
        if (isAtomic)
            attributeSet = addToAtkAttributeSet(attributeSet, "container-atomic", "true");
        if (!liveRole.isEmpty())
            attributeSet = addToAtkAttributeSet(attributeSet, "container-live-role", liveRole.utf8().data());

        // According to the Core AAM, if this object is the live region (rather than its descendant),
        // we must expose the above properties on the object without a "container-" prefix.
        if (liveContainer == coreObject) {
            attributeSet = addToAtkAttributeSet(attributeSet, "live", liveStatus.utf8().data());
            attributeSet = addToAtkAttributeSet(attributeSet, "relevant", relevant.utf8().data());
            if (isAtomic)
                attributeSet = addToAtkAttributeSet(attributeSet, "atomic", "true");
        } else if (!isAtomic && coreObject->ariaLiveRegionAtomic())
            attributeSet = addToAtkAttributeSet(attributeSet, "atomic", "true");
    }

    // The Core AAM states the author-provided value should be exposed as-is.
    String dropEffect = coreObject->getAttribute(HTMLNames::aria_dropeffectAttr);
    if (!dropEffect.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "dropeffect", dropEffect.utf8().data());

    if (coreObject->isARIAGrabbed())
        attributeSet = addToAtkAttributeSet(attributeSet, "grabbed", "true");
    else if (coreObject->supportsARIADragging())
        attributeSet = addToAtkAttributeSet(attributeSet, "grabbed", "false");

    // The Core AAM states the author-provided value should be exposed as-is.
    const AtomicString& keyShortcuts = coreObject->ariaKeyShortcutsValue();
    if (!keyShortcuts.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "keyshortcuts", keyShortcuts.string().utf8().data());

    return attributeSet;
}

static AtkRole atkRole(AccessibilityObject* coreObject)
{
    AccessibilityRole role = coreObject->roleValue();
    switch (role) {
    case ApplicationAlertRole:
        return ATK_ROLE_ALERT;
    case ApplicationAlertDialogRole:
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
#if ATK_CHECK_VERSION(2, 15, 2)
        return ATK_ROLE_STATIC;
#else
        return ATK_ROLE_TEXT;
#endif
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
    case MenuButtonRole:
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
        return coreObject->isMeter() ? ATK_ROLE_LEVEL_BAR : ATK_ROLE_PROGRESS_BAR;
    case WindowRole:
        return ATK_ROLE_WINDOW;
    case PopUpButtonRole:
        return coreObject->ariaHasPopup() ? ATK_ROLE_PUSH_BUTTON : ATK_ROLE_COMBO_BOX;
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
#endif
    case ListRole:
        return ATK_ROLE_LIST;
    case ScrollBarRole:
        return ATK_ROLE_SCROLL_BAR;
    case ScrollAreaRole:
    case TabPanelRole:
        return ATK_ROLE_SCROLL_PANE;
    case GridRole:
    case TableRole:
        return ATK_ROLE_TABLE;
    case TreeGridRole:
        return ATK_ROLE_TREE_TABLE;
    case ApplicationRole:
        return ATK_ROLE_APPLICATION;
    case ApplicationGroupRole:
    case FeedRole:
    case FigureRole:
    case GroupRole:
    case RadioGroupRole:
    case SVGRootRole:
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
        // https://rawgit.com/w3c/aria/master/core-aam/core-aam.html#role-map-listbox
        return coreObject->isDescendantOfRole(ComboBoxRole) ? ATK_ROLE_MENU : ATK_ROLE_LIST_BOX;
    case ListItemRole:
        return coreObject->inheritsPresentationalRole() ? ATK_ROLE_SECTION : ATK_ROLE_LIST_ITEM;
    case ListBoxOptionRole:
        return coreObject->isDescendantOfRole(ComboBoxRole) ? ATK_ROLE_MENU_ITEM : ATK_ROLE_LIST_ITEM;
    case ParagraphRole:
        return ATK_ROLE_PARAGRAPH;
    case LabelRole:
    case LegendRole:
        return ATK_ROLE_LABEL;
    case BlockquoteRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        return ATK_ROLE_BLOCK_QUOTE;
#endif
    case FootnoteRole:
#if ATK_CHECK_VERSION(2, 25, 2)
        return ATK_ROLE_FOOTNOTE;
#endif
    case ApplicationTextGroupRole:
    case DivRole:
    case PreRole:
    case SVGTextRole:
    case TextGroupRole:
        return ATK_ROLE_SECTION;
    case FooterRole:
        return ATK_ROLE_FOOTER;
    case FormRole:
#if ATK_CHECK_VERSION(2, 11, 3)
        if (coreObject->ariaRoleAttribute() != UnknownRole)
            return ATK_ROLE_LANDMARK;
#endif
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
    case WebApplicationRole:
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
    case LandmarkDocRegionRole:
    case LandmarkMainRole:
    case LandmarkNavigationRole:
    case LandmarkRegionRole:
    case LandmarkSearchRole:
        return ATK_ROLE_LANDMARK;
#endif
#if ATK_CHECK_VERSION(2, 11, 4)
    case DescriptionListRole:
        return ATK_ROLE_DESCRIPTION_LIST;
    case TermRole:
    case DescriptionListTermRole:
        return ATK_ROLE_DESCRIPTION_TERM;
    case DescriptionListDetailRole:
        return ATK_ROLE_DESCRIPTION_VALUE;
#endif
    case InlineRole:
#if ATK_CHECK_VERSION(2, 15, 4)
        if (coreObject->isSubscriptStyleGroup())
            return ATK_ROLE_SUBSCRIPT;
        if (coreObject->isSuperscriptStyleGroup())
            return ATK_ROLE_SUPERSCRIPT;
#endif
#if ATK_CHECK_VERSION(2, 15, 2)
        return ATK_ROLE_STATIC;
    case SVGTextPathRole:
    case SVGTSpanRole:
    case TimeRole:
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
    if ((isListBoxOption && coreObject->isSelectedOptionActive())
        || coreObject->ariaCurrentState() != ARIACurrentFalse)
        atk_state_set_add_state(stateSet, ATK_STATE_ACTIVE);

    if (coreObject->isBusy())
        atk_state_set_add_state(stateSet, ATK_STATE_BUSY);

#if ATK_CHECK_VERSION(2,11,2)
    if (coreObject->supportsChecked() && coreObject->canSetValueAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKABLE);
#endif

    if (coreObject->isChecked())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKED);

    if ((coreObject->isTextControl() || coreObject->isNonNativeTextControl()) && coreObject->canSetValueAttribute())
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

    if (coreObject->ariaHasPopup())
        atk_state_set_add_state(stateSet, ATK_STATE_HAS_POPUP);

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);
    else if (coreObject->isCheckboxOrRadio() || coreObject->isMenuItem() || coreObject->isToggleButton()) {
        if (coreObject->checkboxOrRadioValue() == ButtonStateMixed)
            atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);
    }

    if (coreObject->isAriaModalNode())
        atk_state_set_add_state(stateSet, ATK_STATE_MODAL);

    if (coreObject->invalidStatus() != "false")
        atk_state_set_add_state(stateSet, ATK_STATE_INVALID_ENTRY);

    if (coreObject->isMultiSelectable())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

#if ATK_CHECK_VERSION(2,15,3)
    if (!coreObject->canSetValueAttribute() && (coreObject->supportsARIAReadOnly()))
        atk_state_set_add_state(stateSet, ATK_STATE_READ_ONLY);
#endif

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
    if (coreObject->roleValue() == TextAreaRole || coreObject->ariaIsMultiline())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);
    else if (coreObject->roleValue() == TextFieldRole || coreObject->roleValue() == SearchFieldRole)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);

    // TODO: ATK_STATE_SENSITIVE

    if (coreObject->supportsARIAAutoComplete() && coreObject->ariaAutoCompleteValue() != "none")
        atk_state_set_add_state(stateSet, ATK_STATE_SUPPORTS_AUTOCOMPLETION);

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
        || role == GridCellRole || role == TextGroupRole || role == ApplicationTextGroupRole;
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
    if (coreObject->canHaveSelectedChildren() || coreObject->isMenuList())
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
    else if (coreObject->isTextControl() || coreObject->isNonNativeTextControl()) {
        interfaceMask |= 1 << WAIText;
        if (coreObject->canSetValueAttribute())
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
    if (coreObject->isTable())
        interfaceMask |= 1 << WAITable;

#if ATK_CHECK_VERSION(2,11,90)
    if (role == CellRole || role == GridCellRole || role == ColumnHeaderRole || role == RowHeaderRole)
        interfaceMask |= 1 << WAITableCell;
#endif

    // Document
    if (role == WebAreaRole)
        interfaceMask |= 1 << WAIDocument;

    // Value
    if (coreObject->supportsRangeValue())
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
