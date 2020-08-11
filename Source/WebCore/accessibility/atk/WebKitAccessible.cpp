/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012, 2019 Igalia S.L.
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
#include "WebKitAccessible.h"

#if ENABLE(ACCESSIBILITY)

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
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebCore;

struct _WebKitAccessiblePrivate {
    AccessibilityObject* object;

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

WEBKIT_DEFINE_TYPE(WebKitAccessible, webkit_accessible, ATK_TYPE_OBJECT)

static const gchar* webkitAccessibleGetName(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    if (!accessible->priv->object)
        return "";

    Vector<AccessibilityText> textOrder;
    accessible->priv->object->accessibilityText(textOrder);

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
            return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedAccessibleName, text.text.utf8());
    }

    return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedAccessibleName, "");
}

static const gchar* webkitAccessibleGetDescription(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    if (!accessible->priv->object)
        return "";

    Vector<AccessibilityText> textOrder;
    accessible->priv->object->accessibilityText(textOrder);

    bool nameTextAvailable = false;
    for (const auto& text : textOrder) {
        // WebCore Accessibility should provide us with the text alternative computation
        // in the order defined by that spec. So take the first thing that our platform
        // does not expose via the AtkObject name.
        if (text.textSource == AccessibilityTextSource::Help || text.textSource == AccessibilityTextSource::Summary)
            return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedAccessibleDescription, text.text.utf8());

        // If there is no other text alternative, the title tag contents will have been
        // used for the AtkObject name. We don't want to duplicate it here.
        if (text.textSource == AccessibilityTextSource::TitleTag && nameTextAvailable)
            return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedAccessibleDescription, text.text.utf8());

        nameTextAvailable = true;
    }

    return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedAccessibleDescription, "");
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
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, ATK_OBJECT(label->wrapper()));
    } else if (coreObject->isFieldset()) {
        if (AccessibilityObject* label = coreObject->titleUIElement())
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, ATK_OBJECT(label->wrapper()));
    } else if (coreObject->roleValue() == AccessibilityRole::Legend) {
        if (RenderBlock* renderFieldset = ancestorsOfType<RenderBlock>(*coreObject->renderer()).first()) {
            if (renderFieldset->isFieldset()) {
                AccessibilityObject* fieldset = coreObject->axObjectCache()->getOrCreate(renderFieldset);
                atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, ATK_OBJECT(fieldset->wrapper()));
            }
        }
    } else if (AccessibilityObject* control = coreObject->correspondingControlForLabelElement())
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, ATK_OBJECT(control->wrapper()));
    else {
        AccessibilityObject::AccessibilityChildrenVector ariaLabelledByElements;
        coreObject->ariaLabelledByElements(ariaLabelledByElements);
        for (const auto& accessibilityObject : ariaLabelledByElements)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, ATK_OBJECT(accessibilityObject->wrapper()));
    }

    // Elements referenced by aria-labelledby should have the label-for relation as per the ARIA AAM spec.
    AccessibilityObject::AccessibilityChildrenVector labels;
    coreObject->ariaLabelledByReferencingElements(labels);
    for (const auto& accessibilityObject : labels)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements with aria-flowto should have the flows-to relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_FLOWS_TO);
    AccessibilityObject::AccessibilityChildrenVector ariaFlowToElements;
    coreObject->ariaFlowToElements(ariaFlowToElements);
    for (const auto& accessibilityObject : ariaFlowToElements)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_FLOWS_TO, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-flowto should have the flows-from relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_FLOWS_FROM);
    AccessibilityObject::AccessibilityChildrenVector flowFrom;
    coreObject->ariaFlowToReferencingElements(flowFrom);
    for (const auto& accessibilityObject : flowFrom)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_FLOWS_FROM, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements with aria-describedby should have the described-by relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DESCRIBED_BY);
    AccessibilityObject::AccessibilityChildrenVector ariaDescribedByElements;
    coreObject->ariaDescribedByElements(ariaDescribedByElements);
    for (const auto& accessibilityObject : ariaDescribedByElements)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DESCRIBED_BY, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-describedby should have the description-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DESCRIPTION_FOR);
    AccessibilityObject::AccessibilityChildrenVector describers;
    coreObject->ariaDescribedByReferencingElements(describers);
    for (const auto& accessibilityObject : describers)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DESCRIPTION_FOR, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements with aria-controls should have the controller-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_CONTROLLER_FOR);
    AccessibilityObject::AccessibilityChildrenVector ariaControls;
    coreObject->ariaControlsElements(ariaControls);
    for (const auto& accessibilityObject : ariaControls)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_CONTROLLER_FOR, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-controls should have the controlled-by relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_CONTROLLED_BY);
    AccessibilityObject::AccessibilityChildrenVector controllers;
    coreObject->ariaControlsReferencingElements(controllers);
    for (const auto& accessibilityObject : controllers)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_CONTROLLED_BY, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements with aria-owns should have the node-parent-of relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_NODE_PARENT_OF);
    AccessibilityObject::AccessibilityChildrenVector ariaOwns;
    coreObject->ariaOwnsElements(ariaOwns);
    for (const auto& accessibilityObject : ariaOwns)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_NODE_PARENT_OF, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-owns should have the node-child-of relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_NODE_CHILD_OF);
    AccessibilityObject::AccessibilityChildrenVector owners;
    coreObject->ariaOwnsReferencingElements(owners);
    for (const auto& accessibilityObject : owners)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_NODE_CHILD_OF, ATK_OBJECT(accessibilityObject->wrapper()));

#if ATK_CHECK_VERSION(2, 25, 2)
    // Elements with aria-details should have the details relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DETAILS);
    AccessibilityObject::AccessibilityChildrenVector ariaDetails;
    coreObject->ariaDetailsElements(ariaDetails);
    for (const auto& accessibilityObject : ariaDetails)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DETAILS, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-details should have the details-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_DETAILS_FOR);
    AccessibilityObject::AccessibilityChildrenVector details;
    coreObject->ariaDetailsReferencingElements(details);
    for (const auto& accessibilityObject : details)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_DETAILS_FOR, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements with aria-errormessage should have the error-message relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_ERROR_MESSAGE);
    AccessibilityObject::AccessibilityChildrenVector ariaErrorMessage;
    coreObject->ariaErrorMessageElements(ariaErrorMessage);
    for (const auto& accessibilityObject : ariaErrorMessage)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_ERROR_MESSAGE, ATK_OBJECT(accessibilityObject->wrapper()));

    // Elements referenced by aria-errormessage should have the error-for relation as per the ARIA AAM spec.
    removeAtkRelationByType(relationSet, ATK_RELATION_ERROR_FOR);
    AccessibilityObject::AccessibilityChildrenVector errors;
    coreObject->ariaErrorMessageReferencingElements(errors);
    for (const auto& accessibilityObject : errors)
        atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_ERROR_FOR, ATK_OBJECT(accessibilityObject->wrapper()));
#endif
}

static bool isRootObject(AccessibilityObject* coreObject)
{
    // The root accessible object in WebCore is always an object with
    // the ScrolledArea role with one child with the WebArea role.
    if (!coreObject || !coreObject->isScrollView())
        return false;

    AccessibilityObject* firstChild = coreObject->firstChild();
    return firstChild && firstChild->isWebArea();
}

static AtkObject* webkitAccessibleGetParent(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    // Check first if the parent has been already set.
    AtkObject* accessibleParent = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->get_parent(object);
    if (accessibleParent)
        return accessibleParent;

    // Parent not set yet, so try to find it in the hierarchy.
    auto* coreObject = accessible->priv->object;
    if (!coreObject)
        return nullptr;
    auto* coreParent = coreObject->parentObjectUnignored();
    if (!coreParent && isRootObject(coreObject)) {
        // The top level object claims to not have a parent. This makes it
        // impossible for assistive technologies to ascend the accessible
        // hierarchy all the way to the application. (Bug 30489)
        if (!coreObject->document())
            return nullptr;
    }

    return coreParent ? ATK_OBJECT(coreParent->wrapper()) : nullptr;
}

static gint webkitAccessibleGetNChildren(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, 0);

    if (!accessible->priv->object)
        return 0;
    return accessible->priv->object->children().size();
}

static AtkObject* webkitAccessibleRefChild(AtkObject* object, gint index)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    if (!accessible->priv->object || index < 0)
        return nullptr;

    const auto& children = accessible->priv->object->children();
    if (static_cast<size_t>(index) >= children.size())
        return nullptr;

    auto& coreChild = children[index];
    if (!coreChild)
        return nullptr;

    auto* child = coreChild->wrapper();
    if (!child)
        return nullptr;

    atk_object_set_parent(ATK_OBJECT(child), object);
    return ATK_OBJECT(g_object_ref(child));
}

static gint webkitAccessibleGetIndexInParent(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, -1);

    auto* coreObject = accessible->priv->object;
    if (!coreObject)
        return -1;
    auto* parent = coreObject->parentObjectUnignored();
    if (!parent && isRootObject(coreObject)) {
        if (!coreObject->document())
            return -1;

        auto* atkParent = parent ? ATK_OBJECT(parent->wrapper()) : nullptr;
        if (!atkParent)
            return -1;

        unsigned count = atk_object_get_n_accessible_children(atkParent);
        for (unsigned i = 0; i < count; ++i) {
            GRefPtr<AtkObject> child = adoptGRef(atk_object_ref_accessible_child(atkParent, i));
            if (child.get() == object)
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
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    AtkAttributeSet* attributeSet = nullptr;
#if PLATFORM(GTK)
    attributeSet = addToAtkAttributeSet(attributeSet, "toolkit", "WebKitGtk");
#elif PLATFORM(WPE)
    attributeSet = addToAtkAttributeSet(attributeSet, "toolkit", "WPEWebKit");
#endif

    auto* coreObject = accessible->priv->object;
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

    if (coreObject->roleValue() == AccessibilityRole::MathElement) {
        if (coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSuperscript) || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "pre");
        else if (coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSuperscript) || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSubscript))
            attributeSet = addToAtkAttributeSet(attributeSet, "multiscript-type", "post");
    }

    if (is<AccessibilityTable>(*coreObject) && downcast<AccessibilityTable>(*coreObject).isExposable()) {
        auto& table = downcast<AccessibilityTable>(*coreObject);
        int rowCount = table.axRowCount();
        if (rowCount)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowcount", String::number(rowCount).utf8().data());

        int columnCount = table.axColumnCount();
        if (columnCount)
            attributeSet = addToAtkAttributeSet(attributeSet, "colcount", String::number(columnCount).utf8().data());
    } else if (is<AccessibilityTableRow>(*coreObject)) {
        auto& row = downcast<AccessibilityTableRow>(*coreObject);
        int rowIndex = row.axRowIndex();
        if (rowIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowindex", String::number(rowIndex).utf8().data());
    } else if (is<AccessibilityTableCell>(*coreObject)) {
        auto& cell = downcast<AccessibilityTableCell>(*coreObject);
        int rowIndex = cell.axRowIndex();
        if (rowIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowindex", String::number(rowIndex).utf8().data());

        int columnIndex = cell.axColumnIndex();
        if (columnIndex != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "colindex", String::number(columnIndex).utf8().data());

        int rowSpan = cell.axRowSpan();
        if (rowSpan != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "rowspan", String::number(rowSpan).utf8().data());

        int columnSpan = cell.axColumnSpan();
        if (columnSpan != -1)
            attributeSet = addToAtkAttributeSet(attributeSet, "colspan", String::number(columnSpan).utf8().data());
    }

    String placeholder = coreObject->placeholderValue();
    if (!placeholder.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "placeholder-text", placeholder.utf8().data());

    if (coreObject->supportsAutoComplete())
        attributeSet = addToAtkAttributeSet(attributeSet, "autocomplete", coreObject->autoCompleteValue().utf8().data());

    if (coreObject->supportsHasPopup())
        attributeSet = addToAtkAttributeSet(attributeSet, "haspopup", coreObject->popupValue().utf8().data());

    if (coreObject->supportsCurrent())
        attributeSet = addToAtkAttributeSet(attributeSet, "current", coreObject->currentValue().utf8().data());

    // The Core AAM states that an explicitly-set value should be exposed, including "none".
    if (coreObject->hasAttribute(HTMLNames::aria_sortAttr)) {
        switch (coreObject->sortDirection()) {
        case AccessibilitySortDirection::Invalid:
            break;
        case AccessibilitySortDirection::Ascending:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "ascending");
            break;
        case AccessibilitySortDirection::Descending:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "descending");
            break;
        case AccessibilitySortDirection::Other:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "other");
            break;
        case AccessibilitySortDirection::None:
            attributeSet = addToAtkAttributeSet(attributeSet, "sort", "none");
        }
    }

    if (coreObject->supportsPosInSet())
        attributeSet = addToAtkAttributeSet(attributeSet, "posinset", String::number(coreObject->posInSet()).utf8().data());

    if (coreObject->supportsSetSize())
        attributeSet = addToAtkAttributeSet(attributeSet, "setsize", String::number(coreObject->setSize()).utf8().data());

    String isReadOnly = coreObject->readOnlyValue();
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
        // We also want to do this for the style-format-group element types so that the type of format
        // group it is doesn't get lost to a generic platform role.
        if (coreObject->ariaRoleAttribute() == AccessibilityRole::Unknown
            && (coreObject->isLandmark() || coreObject->isStyleFormatGroup()))
            attributeSet = addToAtkAttributeSet(attributeSet, "xml-roles", computedRoleString.utf8().data());
    }

    String roleDescription = coreObject->roleDescription();
    if (!roleDescription.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "roledescription", roleDescription.utf8().data());

    // We need to expose the live region attributes even if the live region is currently disabled/off.
    if (auto liveContainer = coreObject->liveRegionAncestor(false)) {
        String liveStatus = liveContainer->liveRegionStatus();
        String relevant = liveContainer->liveRegionRelevant();
        bool isAtom = liveContainer->liveRegionAtomic();
        String liveRole = roleString.isEmpty() ? computedRoleString : roleString;

        // According to the Core AAM, we need to expose the above properties with "container-" prefixed
        // object attributes regardless of whether the container is this object, or an ancestor of it.
        attributeSet = addToAtkAttributeSet(attributeSet, "container-live", liveStatus.utf8().data());
        attributeSet = addToAtkAttributeSet(attributeSet, "container-relevant", relevant.utf8().data());
        if (isAtom)
            attributeSet = addToAtkAttributeSet(attributeSet, "container-atomic", "true");
        if (!liveRole.isEmpty())
            attributeSet = addToAtkAttributeSet(attributeSet, "container-live-role", liveRole.utf8().data());

        // According to the Core AAM, if this object is the live region (rather than its descendant),
        // we must expose the above properties on the object without a "container-" prefix.
        if (liveContainer == coreObject) {
            attributeSet = addToAtkAttributeSet(attributeSet, "live", liveStatus.utf8().data());
            attributeSet = addToAtkAttributeSet(attributeSet, "relevant", relevant.utf8().data());
            if (isAtom)
                attributeSet = addToAtkAttributeSet(attributeSet, "atomic", "true");
        } else if (!isAtom && coreObject->liveRegionAtomic())
            attributeSet = addToAtkAttributeSet(attributeSet, "atomic", "true");
    }

    // The Core AAM states the author-provided value should be exposed as-is.
    String dropEffect = coreObject->getAttribute(HTMLNames::aria_dropeffectAttr);
    if (!dropEffect.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "dropeffect", dropEffect.utf8().data());

    if (coreObject->isGrabbed())
        attributeSet = addToAtkAttributeSet(attributeSet, "grabbed", "true");
    else if (coreObject->supportsDragging())
        attributeSet = addToAtkAttributeSet(attributeSet, "grabbed", "false");

    // The Core AAM states the author-provided value should be exposed as-is.
    const AtomString& keyShortcuts = coreObject->keyShortcutsValue();
    if (!keyShortcuts.isEmpty())
        attributeSet = addToAtkAttributeSet(attributeSet, "keyshortcuts", keyShortcuts.string().utf8().data());

    return attributeSet;
}

static AtkRole atkRole(AccessibilityObject* coreObject)
{
    switch (coreObject->roleValue()) {
    case AccessibilityRole::ApplicationAlert:
        return ATK_ROLE_NOTIFICATION;
    case AccessibilityRole::ApplicationAlertDialog:
        return ATK_ROLE_ALERT;
    case AccessibilityRole::ApplicationDialog:
        return ATK_ROLE_DIALOG;
    case AccessibilityRole::ApplicationStatus:
        return ATK_ROLE_STATUSBAR;
    case AccessibilityRole::Unknown:
        return ATK_ROLE_UNKNOWN;
    case AccessibilityRole::Audio:
        return ATK_ROLE_AUDIO;
    case AccessibilityRole::Video:
        return ATK_ROLE_VIDEO;
    case AccessibilityRole::Button:
        return ATK_ROLE_PUSH_BUTTON;
    case AccessibilityRole::Switch:
    case AccessibilityRole::ToggleButton:
        return ATK_ROLE_TOGGLE_BUTTON;
    case AccessibilityRole::RadioButton:
        return ATK_ROLE_RADIO_BUTTON;
    case AccessibilityRole::CheckBox:
        return ATK_ROLE_CHECK_BOX;
    case AccessibilityRole::Slider:
        return ATK_ROLE_SLIDER;
    case AccessibilityRole::TabGroup:
    case AccessibilityRole::TabList:
        return ATK_ROLE_PAGE_TAB_LIST;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::SearchField:
        return ATK_ROLE_ENTRY;
    case AccessibilityRole::StaticText:
        return ATK_ROLE_STATIC;
    case AccessibilityRole::Outline:
    case AccessibilityRole::Tree:
        return ATK_ROLE_TREE;
    case AccessibilityRole::TreeItem:
        return ATK_ROLE_TREE_ITEM;
    case AccessibilityRole::MenuBar:
        return ATK_ROLE_MENU_BAR;
    case AccessibilityRole::MenuListPopup:
    case AccessibilityRole::Menu:
        return ATK_ROLE_MENU;
    case AccessibilityRole::MenuListOption:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuButton:
        return ATK_ROLE_MENU_ITEM;
    case AccessibilityRole::MenuItemCheckbox:
        return ATK_ROLE_CHECK_MENU_ITEM;
    case AccessibilityRole::MenuItemRadio:
        return ATK_ROLE_RADIO_MENU_ITEM;
    case AccessibilityRole::Column:
        // return ATK_ROLE_TABLE_COLUMN_HEADER; // Is this right?
        return ATK_ROLE_UNKNOWN; // Matches Mozilla
    case AccessibilityRole::Row:
        return ATK_ROLE_TABLE_ROW;
    case AccessibilityRole::Toolbar:
        return ATK_ROLE_TOOL_BAR;
    case AccessibilityRole::Meter:
        return ATK_ROLE_LEVEL_BAR;
    case AccessibilityRole::BusyIndicator:
    case AccessibilityRole::ProgressIndicator:
        return ATK_ROLE_PROGRESS_BAR;
    case AccessibilityRole::Window:
        return ATK_ROLE_WINDOW;
    case AccessibilityRole::PopUpButton:
        return coreObject->hasPopup() ? ATK_ROLE_PUSH_BUTTON : ATK_ROLE_COMBO_BOX;
    case AccessibilityRole::ComboBox:
        return ATK_ROLE_COMBO_BOX;
    case AccessibilityRole::SplitGroup:
        return ATK_ROLE_SPLIT_PANE;
    case AccessibilityRole::Splitter:
        return ATK_ROLE_SEPARATOR;
#if PLATFORM(GTK)
    case AccessibilityRole::ColorWell:
        // ATK_ROLE_COLOR_CHOOSER is defined as a dialog (i.e. it's what appears when you push the button).
        return ATK_ROLE_PUSH_BUTTON;
#endif
    case AccessibilityRole::List:
        return ATK_ROLE_LIST;
    case AccessibilityRole::ScrollBar:
        return ATK_ROLE_SCROLL_BAR;
    case AccessibilityRole::ScrollArea:
    case AccessibilityRole::TabPanel:
        return ATK_ROLE_SCROLL_PANE;
    case AccessibilityRole::Grid:
    case AccessibilityRole::Table:
        return ATK_ROLE_TABLE;
    case AccessibilityRole::TreeGrid:
        return ATK_ROLE_TREE_TABLE;
    case AccessibilityRole::Application:
        return ATK_ROLE_APPLICATION;
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::Feed:
    case AccessibilityRole::Figure:
    case AccessibilityRole::GraphicsObject:
    case AccessibilityRole::Group:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::SVGRoot:
        return ATK_ROLE_PANEL;
    case AccessibilityRole::RowHeader:
        return ATK_ROLE_ROW_HEADER;
    case AccessibilityRole::ColumnHeader:
        return ATK_ROLE_COLUMN_HEADER;
    case AccessibilityRole::Caption:
        return ATK_ROLE_CAPTION;
    case AccessibilityRole::Cell:
    case AccessibilityRole::GridCell:
        return coreObject->inheritsPresentationalRole() ? ATK_ROLE_SECTION : ATK_ROLE_TABLE_CELL;
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
    case AccessibilityRole::ImageMapLink:
        return ATK_ROLE_LINK;
    case AccessibilityRole::ImageMap:
        return ATK_ROLE_IMAGE_MAP;
    case AccessibilityRole::GraphicsSymbol:
    case AccessibilityRole::Image:
        return ATK_ROLE_IMAGE;
    case AccessibilityRole::ListMarker:
        return ATK_ROLE_TEXT;
    case AccessibilityRole::DocumentArticle:
        return ATK_ROLE_ARTICLE;
    case AccessibilityRole::Document:
    case AccessibilityRole::GraphicsDocument:
        return ATK_ROLE_DOCUMENT_FRAME;
    case AccessibilityRole::DocumentNote:
        return ATK_ROLE_COMMENT;
    case AccessibilityRole::Heading:
        return ATK_ROLE_HEADING;
    case AccessibilityRole::ListBox:
        // https://rawgit.com/w3c/aria/master/core-aam/core-aam.html#role-map-listbox
        return coreObject->isDescendantOfRole(AccessibilityRole::ComboBox) ? ATK_ROLE_MENU : ATK_ROLE_LIST_BOX;
    case AccessibilityRole::ListItem:
        return coreObject->inheritsPresentationalRole() ? ATK_ROLE_SECTION : ATK_ROLE_LIST_ITEM;
    case AccessibilityRole::ListBoxOption:
        return coreObject->isDescendantOfRole(AccessibilityRole::ComboBox) ? ATK_ROLE_MENU_ITEM : ATK_ROLE_LIST_ITEM;
    case AccessibilityRole::Paragraph:
        return ATK_ROLE_PARAGRAPH;
    case AccessibilityRole::Label:
    case AccessibilityRole::Legend:
        return ATK_ROLE_LABEL;
    case AccessibilityRole::Blockquote:
        return ATK_ROLE_BLOCK_QUOTE;
#if ATK_CHECK_VERSION(2, 25, 2)
    case AccessibilityRole::Footnote:
        return ATK_ROLE_FOOTNOTE;
#endif
    case AccessibilityRole::ApplicationTextGroup:
    case AccessibilityRole::Div:
    case AccessibilityRole::Pre:
    case AccessibilityRole::SVGText:
    case AccessibilityRole::TextGroup:
        return ATK_ROLE_SECTION;
    case AccessibilityRole::Footer:
        return ATK_ROLE_FOOTER;
    case AccessibilityRole::Form:
        if (coreObject->ariaRoleAttribute() != AccessibilityRole::Unknown)
            return ATK_ROLE_LANDMARK;
        return ATK_ROLE_FORM;
    case AccessibilityRole::Canvas:
        return ATK_ROLE_CANVAS;
    case AccessibilityRole::HorizontalRule:
        return ATK_ROLE_SEPARATOR;
    case AccessibilityRole::SpinButton:
        return ATK_ROLE_SPIN_BUTTON;
    case AccessibilityRole::Tab:
        return ATK_ROLE_PAGE_TAB;
    case AccessibilityRole::UserInterfaceTooltip:
        return ATK_ROLE_TOOL_TIP;
    case AccessibilityRole::WebArea:
        return ATK_ROLE_DOCUMENT_WEB;
    case AccessibilityRole::WebApplication:
        return ATK_ROLE_EMBEDDED;
    case AccessibilityRole::ApplicationLog:
        return ATK_ROLE_LOG;
    case AccessibilityRole::ApplicationMarquee:
        return ATK_ROLE_MARQUEE;
    case AccessibilityRole::ApplicationTimer:
        return ATK_ROLE_TIMER;
    case AccessibilityRole::Definition:
        return ATK_ROLE_DEFINITION;
    case AccessibilityRole::DocumentMath:
        return ATK_ROLE_MATH;
    case AccessibilityRole::MathElement:
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
        if (coreObject->isMathFraction())
            return ATK_ROLE_MATH_FRACTION;
        if (coreObject->isMathSquareRoot() || coreObject->isMathRoot())
            return ATK_ROLE_MATH_ROOT;
        if (coreObject->isMathScriptObject(AccessibilityMathScriptObjectType::Subscript)
            || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSubscript) || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSubscript))
            return ATK_ROLE_SUBSCRIPT;
        if (coreObject->isMathScriptObject(AccessibilityMathScriptObjectType::Superscript)
            || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PreSuperscript) || coreObject->isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType::PostSuperscript))
            return ATK_ROLE_SUPERSCRIPT;
        if (coreObject->isMathToken())
            return ATK_ROLE_STATIC;
        return ATK_ROLE_UNKNOWN;
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
        return ATK_ROLE_LANDMARK;
    case AccessibilityRole::DescriptionList:
        return ATK_ROLE_DESCRIPTION_LIST;
    case AccessibilityRole::Term:
    case AccessibilityRole::DescriptionListTerm:
        return ATK_ROLE_DESCRIPTION_TERM;
    case AccessibilityRole::DescriptionListDetail:
        return ATK_ROLE_DESCRIPTION_VALUE;
    case AccessibilityRole::Deletion:
#if ATK_CHECK_VERSION(2, 33, 3)
        return ATK_ROLE_CONTENT_DELETION;
#else
        return ATK_ROLE_STATIC;
#endif
    case AccessibilityRole::Insertion:
#if ATK_CHECK_VERSION(2, 33, 3)
        return ATK_ROLE_CONTENT_INSERTION;
#else
        return ATK_ROLE_STATIC;
#endif
    case AccessibilityRole::Subscript:
        return ATK_ROLE_SUBSCRIPT;
    case AccessibilityRole::Superscript:
        return ATK_ROLE_SUPERSCRIPT;
    case AccessibilityRole::Inline:
    case AccessibilityRole::SVGTextPath:
    case AccessibilityRole::SVGTSpan:
    case AccessibilityRole::Time:
        return ATK_ROLE_STATIC;
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkitAccessibleGetRole(AtkObject* object)
{
    // ATK_ROLE_UNKNOWN should only be applied in cases where there is a valid
    // WebCore accessible object for which the platform role mapping is unknown.
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, ATK_ROLE_INVALID);

    if (!accessible->priv->object)
        return ATK_ROLE_INVALID;
    
    // Note: Why doesn't WebCore have a password field for this
    if (accessible->priv->object->isPasswordField())
        return ATK_ROLE_PASSWORD_TEXT;

    return atkRole(accessible->priv->object);
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
    auto* axObject = coreObject->wrapper();
    AtkRole role = axObject ? atk_object_get_role(ATK_OBJECT(axObject)) : ATK_ROLE_INVALID;
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
        || coreObject->currentState() != AccessibilityCurrentState::False)
        atk_state_set_add_state(stateSet, ATK_STATE_ACTIVE);

    if (coreObject->isBusy())
        atk_state_set_add_state(stateSet, ATK_STATE_BUSY);

    if (coreObject->supportsChecked() && coreObject->canSetValueAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKABLE);

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

    // According to the Core AAM, if the element which is focused has a valid aria-activedescendant,
    // we should not expose the focused state on the element which is actually focused, but instead
    // on its active descendant.
    if ((coreObject->isFocused() && !coreObject->activeDescendant()) || isTextWithCaret(coreObject))
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);
    else if (coreObject->isActiveDescendantOfFocusedContainer()) {
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);
    }

    if (coreObject->orientation() == AccessibilityOrientation::Horizontal)
        atk_state_set_add_state(stateSet, ATK_STATE_HORIZONTAL);
    else if (coreObject->orientation() == AccessibilityOrientation::Vertical)
        atk_state_set_add_state(stateSet, ATK_STATE_VERTICAL);

    if (coreObject->hasPopup())
        atk_state_set_add_state(stateSet, ATK_STATE_HAS_POPUP);

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);
    else if (coreObject->isCheckboxOrRadio() || coreObject->isMenuItem() || coreObject->isToggleButton()) {
        if (coreObject->checkboxOrRadioValue() == AccessibilityButtonState::Mixed)
            atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);
    }

    if (coreObject->isModalNode())
        atk_state_set_add_state(stateSet, ATK_STATE_MODAL);

    if (coreObject->invalidStatus() != "false")
        atk_state_set_add_state(stateSet, ATK_STATE_INVALID_ENTRY);

    if (coreObject->isMultiSelectable())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

    if (!coreObject->canSetValueAttribute() && (coreObject->supportsReadOnly()))
        atk_state_set_add_state(stateSet, ATK_STATE_READ_ONLY);

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
    if (coreObject->roleValue() == AccessibilityRole::TextArea || coreObject->ariaIsMultiline())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);
    else if (coreObject->roleValue() == AccessibilityRole::TextField || coreObject->roleValue() == AccessibilityRole::SearchField)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);

    // TODO: ATK_STATE_SENSITIVE

    if (coreObject->supportsAutoComplete() && coreObject->autoCompleteValue() != "none")
        atk_state_set_add_state(stateSet, ATK_STATE_SUPPORTS_AUTOCOMPLETION);

    if (coreObject->isVisited())
        atk_state_set_add_state(stateSet, ATK_STATE_VISITED);
}

static AtkStateSet* webkitAccessibleRefStateSet(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_state_set(object);

    // Make sure the layout is updated to really know whether the object
    // is defunct or not, so we can return the proper state.
    if (accessible->priv->object)
        accessible->priv->object->updateBackingStore();
    if (!accessible->priv->object) {
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    // Text objects must be focusable.
    AtkRole role = atk_object_get_role(object);
    if (role == ATK_ROLE_TEXT || role == ATK_ROLE_PARAGRAPH)
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    setAtkStateSetFromCoreObject(accessible->priv->object, stateSet);
    return stateSet;
}

static AtkRelationSet* webkitAccessibleRefRelationSet(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    AtkRelationSet* relationSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_relation_set(object);
    if (accessible->priv->object)
        setAtkRelationSetFromCoreObject(accessible->priv->object, relationSet);
    return relationSet;
}

static void webkitAccessibleInit(AtkObject* object, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize)
        ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize(object, data);

    WebKitAccessible* accessible = WEBKIT_ACCESSIBLE(object);
    accessible->priv->object = reinterpret_cast<AccessibilityObject*>(data);
}

static const gchar* webkitAccessibleGetObjectLocale(AtkObject* object)
{
    auto* accessible = WEBKIT_ACCESSIBLE(object);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    if (!accessible->priv->object)
        return nullptr;
    
    if (ATK_IS_DOCUMENT(object)) {
        // TODO: Should we fall back on lang xml:lang when the following comes up empty?
        String language = accessible->priv->object->language();
        if (!language.isEmpty())
            return webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedDocumentLocale, language.utf8());

    } else if (ATK_IS_TEXT(object)) {
        const gchar* locale = nullptr;

        AtkAttributeSet* textAttributes = atk_text_get_default_attributes(ATK_TEXT(object));
        for (auto* attributes = textAttributes; attributes; attributes = attributes->next) {
            auto* atkAttribute = static_cast<AtkAttribute*>(attributes->data);
            if (!strcmp(atkAttribute->name, atk_text_attribute_get_name(ATK_TEXT_ATTR_LANGUAGE))) {
                locale = webkitAccessibleCacheAndReturnAtkProperty(accessible, AtkCachedDocumentLocale, atkAttribute->value);
                break;
            }
        }
        atk_attribute_set_free(textAttributes);

        return locale;
    }

    return nullptr;
}

static void webkit_accessible_class_init(WebKitAccessibleClass* klass)
{
    auto* atkObjectClass = ATK_OBJECT_CLASS(klass);
    atkObjectClass->initialize = webkitAccessibleInit;
    atkObjectClass->get_name = webkitAccessibleGetName;
    atkObjectClass->get_description = webkitAccessibleGetDescription;
    atkObjectClass->get_parent = webkitAccessibleGetParent;
    atkObjectClass->get_n_children = webkitAccessibleGetNChildren;
    atkObjectClass->ref_child = webkitAccessibleRefChild;
    atkObjectClass->get_role = webkitAccessibleGetRole;
    atkObjectClass->ref_state_set = webkitAccessibleRefStateSet;
    atkObjectClass->get_index_in_parent = webkitAccessibleGetIndexInParent;
    atkObjectClass->get_attributes = webkitAccessibleGetAttributes;
    atkObjectClass->ref_relation_set = webkitAccessibleRefRelationSet;
    atkObjectClass->get_object_locale = webkitAccessibleGetObjectLocale;
}

static const GInterfaceInfo atkInterfacesInitFunctions[] = {
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleActionInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleSelectionInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleEditableTextInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleTextInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleComponentInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleImageInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleTableInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleTableCellInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleHypertextInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleHyperlinkImplInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleDocumentInterfaceInit)), nullptr, nullptr},
    {reinterpret_cast<GInterfaceInitFunc>(reinterpret_cast<GCallback>(webkitAccessibleValueInterfaceInit)), nullptr, nullptr}
};

enum WAIType {
    WAIAction,
    WAISelection,
    WAIEditableText,
    WAIText,
    WAIComponent,
    WAIImage,
    WAITable,
    WAITableCell,
    WAIHypertext,
    WAIHyperlink,
    WAIDocument,
    WAIValue,
};

static GType atkInterfaceTypeFromWAIType(WAIType type)
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
    case WAITableCell:
        return ATK_TYPE_TABLE_CELL;
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

static guint16 interfaceMaskFromObject(AXCoreObject* coreObject)
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
    if (role == AccessibilityRole::StaticText || coreObject->isMenuListOption())
        interfaceMask |= 1 << WAIText;
    else if (coreObject->isTextControl() || coreObject->isNonNativeTextControl()) {
        interfaceMask |= 1 << WAIText;
        if (coreObject->canSetValueAttribute())
            interfaceMask |= 1 << WAIEditableText;
    } else if (!coreObject->isWebArea()) {
        if (role != AccessibilityRole::Table) {
            interfaceMask |= 1 << WAIHypertext;
            if ((renderer && renderer->childrenInline()) || roleIsTextType(role) || coreObject->isMathToken())
                interfaceMask |= 1 << WAIText;
        }

        // Add the TEXT interface for list items whose
        // first accessible child has a text renderer
        if (role == AccessibilityRole::ListItem) {
            const auto& children = coreObject->children();
            if (!children.isEmpty())
                interfaceMask |= interfaceMaskFromObject(children[0].get());
        }
    }

    // Image
    if (coreObject->isImage())
        interfaceMask |= 1 << WAIImage;

    // Table
    if (coreObject->isTable())
        interfaceMask |= 1 << WAITable;

    if (role == AccessibilityRole::Cell || role == AccessibilityRole::GridCell || role == AccessibilityRole::ColumnHeader || role == AccessibilityRole::RowHeader)
        interfaceMask |= 1 << WAITableCell;

    // Document
    if (role == AccessibilityRole::WebArea)
        interfaceMask |= 1 << WAIDocument;

    // Value
    if (coreObject->supportsRangeValue())
        interfaceMask |= 1 << WAIValue;

#if ENABLE(INPUT_TYPE_COLOR)
    // Color type.
    if (role == AccessibilityRole::ColorWell)
        interfaceMask |= 1 << WAIText;
#endif

    return interfaceMask;
}

static const char* uniqueAccessibilityTypeName(guint16 interfaceMask)
{
#define WAI_TYPE_NAME_LEN (30) // Enough for prefix + 5 hex characters (max).
    static char name[WAI_TYPE_NAME_LEN + 1];

    g_sprintf(name, "WAIType%x", interfaceMask);
    name[WAI_TYPE_NAME_LEN] = '\0';

    return name;
}

static GType accessibilityTypeFromObject(AccessibilityObject* coreObject)
{
    static const GTypeInfo typeInfo = {
        sizeof(WebKitAccessibleClass),
        nullptr, // GBaseInitFunc
        nullptr, // GBaseFinalizeFunc
        nullptr, // GClassInitFunc
        nullptr, // GClassFinalizeFunc
        nullptr, // class data
        sizeof(WebKitAccessible), // instance size
        0, // nb preallocs
        nullptr, // GInstanceInitFunc
        nullptr // value table
    };

    guint16 interfaceMask = interfaceMaskFromObject(coreObject);
    const char* atkTypeName = uniqueAccessibilityTypeName(interfaceMask);
    if (GType type = g_type_from_name(atkTypeName))
        return type;

    GType type = g_type_register_static(WEBKIT_TYPE_ACCESSIBLE, atkTypeName, &typeInfo, static_cast<GTypeFlags>(0));
    for (unsigned i = 0; i < G_N_ELEMENTS(atkInterfacesInitFunctions); ++i) {
        if (interfaceMask & (1 << i)) {
            g_type_add_interface_static(type,
                atkInterfaceTypeFromWAIType(static_cast<WAIType>(i)),
                &atkInterfacesInitFunctions[i]);
        }
    }

    return type;
}

WebKitAccessible* webkitAccessibleNew(AccessibilityObject* coreObject)
{
    auto* object = ATK_OBJECT(g_object_new(accessibilityTypeFromObject(coreObject), nullptr));
    atk_object_initialize(object, coreObject);
    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject& webkitAccessibleGetAccessibilityObject(WebKitAccessible* accessible)
{
    ASSERT(WEBKIT_IS_ACCESSIBLE(accessible));
    ASSERT(accessible->priv->object);
    return *accessible->priv->object;
}

void webkitAccessibleDetach(WebKitAccessible* accessible)
{
    ASSERT(WEBKIT_IS_ACCESSIBLE(accessible));

    if (accessible->priv->object && accessible->priv->object->roleValue() == AccessibilityRole::WebArea)
        atk_object_notify_state_change(ATK_OBJECT(accessible), ATK_STATE_DEFUNCT, TRUE);

    accessible->priv->object = nullptr;
}

bool webkitAccessibleIsDetached(WebKitAccessible* accessible)
{
    ASSERT(WEBKIT_IS_ACCESSIBLE(accessible));
    return !accessible->priv->object;
}

const char* webkitAccessibleCacheAndReturnAtkProperty(WebKitAccessible* accessible, AtkCachedProperty property, CString&& value)
{
    ASSERT(WEBKIT_IS_ACCESSIBLE(accessible));

    WebKitAccessiblePrivate* priv = accessible->priv;
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
    if (*propertyPtr != value)
        *propertyPtr = WTFMove(value);

    return (*propertyPtr).data();
}

#endif // ENABLE(ACCESSIBILITY)
