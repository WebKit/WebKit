/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityUIElementAtspi.h"

#if USE(ATSPI)
#include "AccessibilityNotificationHandler.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/AccessibilityAtspiEnums.h>
#include <WebCore/AccessibilityObjectAtspi.h>
#include <WebKit/WKBundleFrame.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTR {

Ref<AccessibilityUIElementAtspi> AccessibilityUIElementAtspi::create(PlatformUIElement element)
{
    return adoptRef(*new AccessibilityUIElementAtspi(element));
}

Ref<AccessibilityUIElementAtspi> AccessibilityUIElementAtspi::create(const AccessibilityUIElementAtspi& other)
{
    return adoptRef(*new AccessibilityUIElementAtspi(other));
}

AccessibilityUIElementAtspi::AccessibilityUIElementAtspi(PlatformUIElement element)
    : AccessibilityUIElement(element)
    , m_element(element)
{
    if (!s_controller)
        s_controller = InjectedBundle::singleton().accessibilityController();
}

AccessibilityUIElementAtspi::AccessibilityUIElementAtspi(const AccessibilityUIElementAtspi& other)
    : AccessibilityUIElement(other)
    , m_element(other.m_element)
{
}

AccessibilityUIElementAtspi::~AccessibilityUIElementAtspi() = default;

bool AccessibilityUIElementAtspi::isValid() const
{
    return m_element;
}

bool AccessibilityUIElementAtspi::isEqual(AccessibilityUIElement* otherElement)
{
    return otherElement && m_element.get() == otherElement->platformUIElement();
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementAtspi::getChildren() const
{
    return { };
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementAtspi::getChildrenInRange(unsigned location, unsigned length) const
{
    return { };
}

unsigned AccessibilityUIElementAtspi::childrenCount()
{
    m_element->updateBackingStore();
    return m_element->childCount();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::elementAtPoint(int x, int y)
{
    m_element->updateBackingStore();
    auto* element = m_element->hitTest({ x, y }, WebCore::Atspi::CoordinateType::WindowCoordinates);
    return AccessibilityUIElementAtspi::create(element ? element : m_element.get());
}

unsigned AccessibilityUIElementAtspi::indexOfChild(AccessibilityUIElement* element)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::childAtIndex(unsigned index)
{
    m_element->updateBackingStore();
    if (auto* child = m_element->childAt(index))
        return AccessibilityUIElementAtspi::create(child);
    return nullptr;
}

static RefPtr<AccessibilityUIElement> elementForRelationAtIndex(WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::Relation relation, unsigned index)
{
    element->updateBackingStore();
    auto relationMap = element->relationMap();
    auto targets = relationMap.get(relation);
    if (targets.isEmpty() || index >= targets.size())
        return nullptr;

    Ref target = targets[index];
    return AccessibilityUIElementAtspi::create(target.ptr());
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::linkedUIElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaOwnsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::NodeParentOf, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ownerElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::NodeChildOf, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaFlowToElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::FlowsTo, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::flowFromElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::FlowsFrom, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaActionsElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaControlsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ControllerFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::controllerElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ControlledBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaLabelledByElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelledBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::labelForElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaDescribedByElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DescribedBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::descriptionForElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DescriptionFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaDetailsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::Details, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::detailsForElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DetailsFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::ariaErrorMessageElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ErrorMessage, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::errorMessageForElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ErrorFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::disclosedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::rowAtIndex(unsigned index)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return nullptr;

    m_element->updateBackingStore();
    auto rows = m_element->rows();
    if (index >= rows.size())
        return nullptr;

    return AccessibilityUIElementAtspi::create(rows[index].ptr());
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::selectedChildAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return nullptr;

    m_element->updateBackingStore();
    if (auto* selectedChild = m_element->selectedChild(index))
        return AccessibilityUIElementAtspi::create(selectedChild);
    return nullptr;
}

unsigned AccessibilityUIElementAtspi::selectedChildrenCount() const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return 0;

    m_element->updateBackingStore();
    return m_element->selectionCount();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::selectedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::titleUIElement()
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelledBy, 0);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::parentElement()
{
    m_element->updateBackingStore();
    if (auto* parent = m_element->parent().value_or(nullptr))
        return AccessibilityUIElementAtspi::create(parent);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::disclosedByRow()
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfLinkedUIElements()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfDocumentLinks()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

static RefPtr<StringImpl> attributesOfElement(AccessibilityUIElement& element)
{
    StringBuilder builder;

    builder.append(element.role()->string(), '\n');

    builder.append("AXParent: "_s);
    if (auto parent = element.parentElement()) {
        builder.append(substring(parent->role()->string(), 8));
        auto parentName = substring(parent->title()->string(), 9);
        if (!isEmpty(parentName))
            builder.append(": "_s, parentName);
    } else
        builder.append("(null)"_s);
    builder.append('\n');

    builder.append("AXChildren: "_s, element.childrenCount(), '\n');

    builder.append("AXPosition:  { "_s, FormattedNumber::fixedPrecision(element.x(), 6, TrailingZerosPolicy::Keep));
    builder.append(", "_s, FormattedNumber::fixedPrecision(element.y(), 6, TrailingZerosPolicy::Keep));
    builder.append(" }\n"_s);

    builder.append("AXSize: { "_s, FormattedNumber::fixedPrecision(element.width(), 6, TrailingZerosPolicy::Keep));
    builder.append(", "_s, FormattedNumber::fixedPrecision(element.height(), 6, TrailingZerosPolicy::Keep));
    builder.append(" }\n"_s);

    RefPtr<StringImpl> title = element.title()->string();
    if (!isEmpty(title)) {
        builder.append(title);
        builder.append('\n');
    }

    RefPtr<StringImpl> description = element.description()->string();
    if (!isEmpty(description))
        builder.append(description, '\n');

    RefPtr<StringImpl> value = element.stringValue()->string();
    if (!isEmpty(value))
        builder.append(value, '\n');

    builder.append("AXFocusable: "_s, element.isFocusable(), '\n');
    builder.append("AXFocused: "_s, element.isFocused(), '\n');
    builder.append("AXSelectable: "_s, element.isSelectable(), '\n');
    builder.append("AXSelected: "_s, element.isSelected(), '\n');
    builder.append("AXMultiSelectable: "_s, element.isMultiSelectable(), '\n');
    builder.append("AXEnabled: "_s, element.isEnabled(), '\n');
    builder.append("AXExpanded: "_s, element.isExpanded(), '\n');
    builder.append("AXRequired: "_s, element.isRequired(), '\n');
    builder.append("AXChecked: "_s, element.isChecked(), '\n');

    RefPtr<StringImpl> url = element.url()->string();
    if (!isEmpty(url))
        builder.append(url, '\n');

    // We append the platform attributes as a single line at the end.
    builder.append("AXPlatformAttributes: "_s);
    auto attributes = element.platformUIElement()->attributes();
    auto keys = copyToVector(attributes.keys());
    std::sort(keys.begin(), keys.end(), WTF::codePointCompareLessThan);

    bool isFirst = true;
    for (const auto& key : keys) {
        if (equal(key, "id"_s) || equal(key, "toolkit"_s))
            continue;

        if (!isFirst)
            builder.append(", "_s);
        isFirst = false;
        builder.append(key, ':', attributes.get(key));
    }

    return builder.toString();
}

static RefPtr<StringImpl> attributesOfElements(Vector<Ref<AccessibilityUIElement>>& elements)
{
    StringBuilder builder;
    for (auto& element : elements)
        builder.append(attributesOfElement(element), "\n------------\n"_s);
    return builder.toString();
}

static Vector<Ref<AccessibilityUIElement>> elementsVector(const Vector<Ref<WebCore::AccessibilityObjectAtspi>>& wrappers)
{
    Vector<Ref<AccessibilityUIElement>> elements(ReserveCapacityTag { }, wrappers.size());
    for (auto& wrapper : wrappers)
        elements.append(AccessibilityUIElementAtspi::create(wrapper.ptr()));
    return elements;
}

static RefPtr<StringImpl> attributesOfElements(const Vector<Ref<WebCore::AccessibilityObjectAtspi>>& wrappers)
{
    auto elements = elementsVector(wrappers);
    return attributesOfElements(elements);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfChildren()
{
    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->children()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::allAttributes()
{
    return OpaqueJSString::tryCreate(attributesOfElement(*this));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

static bool checkElementState(WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::State state)
{
    return element->states().contains(state);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::stringAttributeValue(JSStringRef attribute)
{
    RefPtr<StringImpl> attributeName = toWTFString(attribute);
    if (equal(attributeName, "AXSelectedText"_s)) {
        if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
            return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

        m_element->updateBackingStore();
        auto text = m_element->text();
        auto offset = m_element->selectedRange();
        return OpaqueJSString::tryCreate(substring(text, offset.x(), offset.y() - offset.x()));
    }

    m_element->updateBackingStore();
    auto attributes = m_element->attributes();

    if (equal(attributeName, "AXPlaceholderValue"_s))
        return OpaqueJSString::tryCreate(attributes.get("placeholder-text"_s));

    if (equal(attributeName, "AXInvalid"_s)) {
        auto textAttributes = m_element->textAttributes();
        auto value = textAttributes.attributes.get("invalid"_s);
        if (isEmpty(value))
            value = checkElementState(m_element.get(), WebCore::Atspi::State::InvalidEntry) ? "true"_si : "false"_si;
        return OpaqueJSString::tryCreate(value);
    }

    if (equal(attributeName, "AXARIALive"_s))
        return OpaqueJSString::tryCreate(attributes.get("live"_s));

    if (equal(attributeName, "AXARIARelevant"_s))
        return OpaqueJSString::tryCreate(attributes.get("relevant"_s));

    if (equal(attributeName, "AXAutocompleteValue"_s))
        return OpaqueJSString::tryCreate(attributes.get("autocomplete"_s));

    if (equal(attributeName, "AXKeyShortcutsValue"_s))
        return OpaqueJSString::tryCreate(attributes.get("keyshortcuts"_s));

    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

double AccessibilityUIElementAtspi::numberAttributeValue(JSStringRef attribute)
{
    RefPtr<StringImpl> attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    if (equal(attributeName, "AXARIASetSize"_s))
        return toDouble(attributes.get("setsize"_s));
    if (equal(attributeName, "AXARIAPosInSet"_s))
        return toDouble(attributes.get("posinset"_s));
    if (equal(attributeName, "AXARIAColumnCount"_s))
        return toDouble(attributes.get("colcount"_s));
    if (equal(attributeName, "AXARIARowCount"_s))
        return toDouble(attributes.get("rowcount"_s));
    if (equal(attributeName, "AXARIAColumnIndex"_s))
        return toDouble(attributes.get("colindex"_s));
    if (equal(attributeName, "AXARIARowIndex"_s))
        return toDouble(attributes.get("rowindex"_s));
    if (equal(attributeName, "AXARIAColumnSpan"_s))
        return toDouble(attributes.get("colspan"_s));
    if (equal(attributeName, "AXARIARowSpan"_s))
        return toDouble(attributes.get("rowspan"_s));

    return 0;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::currentStateValue() const
{
    m_element->updateBackingStore();
    auto value = m_element->attributes().get("current"_s);
    return OpaqueJSString::tryCreate(value ? value : "false"_si);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::sortDirection() const
{
    m_element->updateBackingStore();
    auto sort = m_element->attributes().get("sort"_s);

    if (equal(sort, "ascending"_s))
        return OpaqueJSString::tryCreate("AXAscendingSortDirection"_s);
    if (equal(sort, "descending"_s))
        return OpaqueJSString::tryCreate("AXDescendingSortDirection"_s);
    if (equal(sort, "other"_s))
        return OpaqueJSString::tryCreate("AXUnknownSortDirection"_s);

    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::domIdentifier() const
{
    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(m_element->attributes().get("id"_s));
}

JSValueRef AccessibilityUIElementAtspi::uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute)
{
    return nullptr;
}

static JSValueRef makeJSArray(JSContextRef context, const Vector<Ref<AccessibilityUIElement>>& elements)
{
    size_t elementCount = elements.size();
    auto valueElements = makeUniqueArray<JSValueRef>(elementCount);
    for (size_t i = 0; i < elementCount; i++)
        valueElements[i] = JSObjectMake(context, elements[i]->wrapperClass(), elements[i].ptr());

    return JSObjectMakeArray(context, elementCount, valueElements.get(), nullptr);
}

JSValueRef AccessibilityUIElementAtspi::rowHeaders(JSContextRef context)
{
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table)) {
        m_element->updateBackingStore();
        return makeJSArray(context, elementsVector(m_element->rowHeaders()));
    }

    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell)) {
        m_element->updateBackingStore();
        return makeJSArray(context, elementsVector(m_element->cellRowHeaders()));
    }

    return makeJSArray(context, { });
}

JSValueRef AccessibilityUIElementAtspi::columnHeaders(JSContextRef context)
{
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table)) {
        m_element->updateBackingStore();
        return makeJSArray(context, elementsVector(m_element->columnHeaders()));
    }

    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell)) {
        m_element->updateBackingStore();
        return makeJSArray(context, elementsVector(m_element->cellColumnHeaders()));
    }

    return makeJSArray(context, { });
}

JSValueRef AccessibilityUIElementAtspi::selectedCells(JSContextRef context)
{
    return makeJSArray(context, { });
}

static JSValueRef elementsForRelation(JSContextRef context, WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::Relation relation)
{
    element->updateBackingStore();
    auto relationMap = element->relationMap();
    auto targets = relationMap.get(relation);
    if (targets.isEmpty())
        return { };

    return makeJSArray(context, elementsVector(targets));
}

JSValueRef AccessibilityUIElementAtspi::detailsElements(JSContextRef context)
{
    return elementsForRelation(context, m_element.get(), WebCore::Atspi::Relation::Details);
}

JSValueRef AccessibilityUIElementAtspi::errorMessageElements(JSContextRef context)
{
    return elementsForRelation(context, m_element.get(), WebCore::Atspi::Relation::ErrorMessage);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::uiElementAttributeValue(JSStringRef attribute) const
{
    return nullptr;
}

bool AccessibilityUIElementAtspi::boolAttributeValue(JSStringRef attribute)
{
    RefPtr<StringImpl> attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    if (equal(attributeName, "AXElementBusy"_s))
        return checkElementState(m_element.get(), WebCore::Atspi::State::Busy);
    if (equal(attributeName, "AXModal"_s))
        return checkElementState(m_element.get(), WebCore::Atspi::State::Modal);
    if (equal(attributeName, "AXSupportsAutoCompletion"_s))
        return checkElementState(m_element.get(), WebCore::Atspi::State::SupportsAutocompletion);
    if (equal(attributeName, "AXVisited"_s))
        return checkElementState(m_element.get(), WebCore::Atspi::State::Visited);
    if (equal(attributeName, "AXInterfaceTable"_s))
        return m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table);
    if (equal(attributeName, "AXInterfaceTableCell"_s))
        return m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell);
    if (equal(attributeName, "AXARIAAtomic"_s))
        return equal(m_element->attributes().get("atomic"_s), "true"_s);

    return false;
}

bool AccessibilityUIElementAtspi::isAttributeSettable(JSStringRef attribute)
{
    RefPtr<StringImpl> attributeName = toWTFString(attribute);
    if (!equal(attributeName, "AXValue"_s))
        return false;

    m_element->updateBackingStore();
    if (checkElementState(m_element.get(), WebCore::Atspi::State::ReadOnly))
        return false;

    if (checkElementState(m_element.get(), WebCore::Atspi::State::Editable))
        return true;

    if (checkElementState(m_element.get(), WebCore::Atspi::State::Checkable))
        return true;

    auto attributes = m_element->attributes();
    RefPtr<StringImpl> isReadOnly = attributes.get("readonly"_s);
    if (!isEmpty(isReadOnly))
        return equal(isReadOnly, "true"_s) ? false : true;

    // If we have a listbox or combobox and the value can be set, the options should be selectable.
    auto elementRole = m_element->role();
    switch (elementRole) {
    case WebCore::Atspi::Role::ComboBox:
    case WebCore::Atspi::Role::ListBox:
        if (auto child = childAtIndex(0)) {
            if (elementRole == WebCore::Atspi::Role::ComboBox) {
                // First child is the menu.
                child = child->childAtIndex(0);
            }

            if (child)
                return checkElementState(static_cast<AccessibilityUIElementAtspi*>(child.get())->m_element.get(), WebCore::Atspi::State::Selectable);
        }
        break;
    default:
        break;
    }

    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value) && checkElementState(m_element.get(), WebCore::Atspi::State::Focusable)) {
        if (m_element->minimumValue() != m_element->maximumValue())
            return true;
    }

    return false;
}

bool AccessibilityUIElementAtspi::isAttributeSupported(JSStringRef attribute)
{
    RefPtr<StringImpl> attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    if (equal(attributeName, "AXARIASetSize"_s))
        return attributes.contains("setsize"_s);
    if (equal(attributeName, "AXARIAPosInSet"_s))
        return attributes.contains("posinset"_s);
    if (equal(attributeName, "AXARIALive"_s)) {
        auto value = attributes.get("live"_s);
        return !isEmpty(value) && !equal(value, "off"_s);
    }
    if (equal(attributeName, "AXARIARelevant"_s))
        return attributes.contains("relevant"_s);
    if (equal(attributeName, "AXARIAAtomic"_s))
        return attributes.contains("atomic"_s);
    if (equal(attributeName, "AXElementBusy"_s))
        return true;

    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::parameterizedAttributeNames()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

static RefPtr<StringImpl> xmlRoleValueString(StringImpl* xmlRoles)
{
    static NeverDestroyed<HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>> regionRoles = HashSet<RefPtr<StringImpl>, ASCIICaseInsensitiveHash>({
        "doc-acknowledgments"_si,
        "doc-afterword"_si,
        "doc-appendix"_si,
        "doc-bibliography"_si,
        "doc-chapter"_si,
        "doc-conclusion"_si,
        "doc-credits"_si,
        "doc-endnotes"_si,
        "doc-epilogue"_si,
        "doc-errata"_si,
        "doc-foreword"_si,
        "doc-glossary"_si,
        "doc-glossref"_si,
        "doc-index"_si,
        "doc-introduction"_si,
        "doc-pagelist"_si,
        "doc-part"_si,
        "doc-preface"_si,
        "doc-prologue"_si,
        "doc-toc"_si,
        "region"_si
    });

    if (regionRoles->contains(xmlRoles))
        return "AXLandmarkRegion"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "banner"_s))
        return "AXLandmarkBanner"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "complementary"_s))
        return "AXLandmarkComplementary"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "contentinfo"_s))
        return "AXLandmarkContentInfo"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "form"_s))
        return "AXLandmarkForm"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "main"_s))
        return "AXLandmarkMain"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "navigation"_s))
        return "AXLandmarkNavigation"_si;
    if (equalLettersIgnoringASCIICase(xmlRoles, "search"_s))
        return "AXLandmarkSearch"_si;

    return { };
}

static RefPtr<StringImpl> roleValueToString(WebCore::Atspi::Role roleValue)
{
    switch (roleValue) {
    case WebCore::Atspi::Role::Alert:
        return "AXAlert"_si;
    case WebCore::Atspi::Role::Article:
        return "AXArticle"_si;
    case WebCore::Atspi::Role::Audio:
        return "AXAudio"_si;
    case WebCore::Atspi::Role::BlockQuote:
        return "AXBlockquote"_si;
    case WebCore::Atspi::Role::Canvas:
        return "AXCanvas"_si;
    case WebCore::Atspi::Role::Caption:
        return "AXCaption"_si;
    case WebCore::Atspi::Role::CheckBox:
        return "AXCheckBox"_si;
    case WebCore::Atspi::Role::CheckMenuItem:
        return "AXCheckMenuItem"_si;
    case WebCore::Atspi::Role::ColorChooser:
        return "AXColorWell"_si;
    case WebCore::Atspi::Role::ColumnHeader:
    case WebCore::Atspi::Role::TableColumnHeader:
        return "AXColumnHeader"_si;
    case WebCore::Atspi::Role::ComboBox:
        return "AXComboBox"_si;
    case WebCore::Atspi::Role::Comment:
        return "AXComment"_si;
    case WebCore::Atspi::Role::ContentDeletion:
        return "AXDeletion"_si;
    case WebCore::Atspi::Role::ContentInsertion:
        return "AXInsertion"_si;
    case WebCore::Atspi::Role::Definition:
        return "AXDefinition"_si;
    case WebCore::Atspi::Role::DescriptionList:
        return "AXDescriptionList"_si;
    case WebCore::Atspi::Role::DescriptionTerm:
        return "AXDescriptionTerm"_si;
    case WebCore::Atspi::Role::DescriptionValue:
        return "AXDescriptionValue"_si;
    case WebCore::Atspi::Role::Dialog:
        return "AXDialog"_si;
    case WebCore::Atspi::Role::DocumentFrame:
        return "AXDocument"_si;
    case WebCore::Atspi::Role::DocumentWeb:
        return "AXWebArea"_si;
    case WebCore::Atspi::Role::Embedded:
        return "AXEmbedded"_si;
    case WebCore::Atspi::Role::Entry:
        return "AXTextField"_si;
    case WebCore::Atspi::Role::Footer:
        return "AXFooter"_si;
    case WebCore::Atspi::Role::Footnote:
        return "AXFootnote"_si;
    case WebCore::Atspi::Role::Form:
        return "AXForm"_si;
    case WebCore::Atspi::Role::Grouping:
    case WebCore::Atspi::Role::Panel:
        return "AXGroup"_si;
    case WebCore::Atspi::Role::Heading:
        return "AXHeading"_si;
    case WebCore::Atspi::Role::Image:
        return "AXImage"_si;
    case WebCore::Atspi::Role::ImageMap:
        return "AXImageMap"_si;
    case WebCore::Atspi::Role::InvalidRole:
        return "AXInvalid"_si;
    case WebCore::Atspi::Role::Label:
        return "AXLabel"_si;
    case WebCore::Atspi::Role::LevelBar:
        return "AXLevelIndicator"_si;
    case WebCore::Atspi::Role::Link:
        return "AXLink"_si;
    case WebCore::Atspi::Role::ListBox:
        return "AXListBox"_si;
    case WebCore::Atspi::Role::List:
        return "AXList"_si;
    case WebCore::Atspi::Role::ListItem:
        return "AXListItem"_si;
    case WebCore::Atspi::Role::Log:
        return "AXLog"_si;
    case WebCore::Atspi::Role::Marquee:
        return "AXMarquee"_si;
    case WebCore::Atspi::Role::Math:
        return "AXMath"_si;
    case WebCore::Atspi::Role::MathFraction:
        return "AXMathFraction"_si;
    case WebCore::Atspi::Role::MathRoot:
        return "AXMathRoot"_si;
    case WebCore::Atspi::Role::Menu:
        return "AXMenu"_si;
    case WebCore::Atspi::Role::MenuBar:
        return "AXMenuBar"_si;
    case WebCore::Atspi::Role::MenuItem:
        return "AXMenuItem"_si;
    case WebCore::Atspi::Role::Notification:
        return "AXNotification"_si;
    case WebCore::Atspi::Role::PageTab:
        return "AXTab"_si;
    case WebCore::Atspi::Role::PageTabList:
        return "AXTabGroup"_si;
    case WebCore::Atspi::Role::Paragraph:
        return "AXParagraph"_si;
    case WebCore::Atspi::Role::PasswordText:
        return "AXPasswordField"_si;
    case WebCore::Atspi::Role::ProgressBar:
        return "AXProgressIndicator"_si;
    case WebCore::Atspi::Role::PushButton:
        return "AXButton"_si;
    case WebCore::Atspi::Role::RadioButton:
        return "AXRadioButton"_si;
    case WebCore::Atspi::Role::RadioMenuItem:
        return "AXRadioMenuItem"_si;
    case WebCore::Atspi::Role::RowHeader:
    case WebCore::Atspi::Role::TableRowHeader:
        return "AXRowHeader"_si;
    case WebCore::Atspi::Role::Ruler:
        return "AXRuler"_si;
    case WebCore::Atspi::Role::ScrollBar:
        return "AXScrollBar"_si;
    case WebCore::Atspi::Role::ScrollPane:
        return "AXScrollArea"_si;
    case WebCore::Atspi::Role::Section:
        return "AXSection"_si;
    case WebCore::Atspi::Role::Separator:
        return "AXSeparator"_si;
    case WebCore::Atspi::Role::Slider:
        return "AXSlider"_si;
    case WebCore::Atspi::Role::SpinButton:
        return "AXSpinButton"_si;
    case WebCore::Atspi::Role::Static:
    case WebCore::Atspi::Role::Text:
        return "AXStatic"_si;
    case WebCore::Atspi::Role::StatusBar:
        return "AXStatusBar"_si;
    case WebCore::Atspi::Role::Subscript:
        return "AXSubscript"_si;
    case WebCore::Atspi::Role::Superscript:
        return "AXSuperscript"_si;
    case WebCore::Atspi::Role::Table:
        return "AXTable"_si;
    case WebCore::Atspi::Role::TableCell:
        return "AXCell"_si;
    case WebCore::Atspi::Role::TableRow:
        return "AXRow"_si;
    case WebCore::Atspi::Role::Timer:
        return "AXTimer"_si;
    case WebCore::Atspi::Role::ToggleButton:
        return "AXToggleButton"_si;
    case WebCore::Atspi::Role::ToolBar:
        return "AXToolbar"_si;
    case WebCore::Atspi::Role::ToolTip:
        return "AXUserInterfaceTooltip"_si;
    case WebCore::Atspi::Role::Tree:
        return "AXTree"_si;
    case WebCore::Atspi::Role::TreeTable:
        return "AXTreeGrid"_si;
    case WebCore::Atspi::Role::TreeItem:
        return "AXTreeItem"_si;
    case WebCore::Atspi::Role::Unknown:
        return "AXUnknown"_si;
    case WebCore::Atspi::Role::Video:
        return "AXVideo"_si;
    case WebCore::Atspi::Role::Window:
        return "AXWindow"_si;
    default:
        break;
    }

    return { };
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::role()
{
    m_element->updateBackingStore();
    auto roleValue = m_element->role();
    auto roleValueString = roleValue == WebCore::Atspi::Role::Landmark ? xmlRoleValueString(m_element->attributes().get("xml-roles"_s)) : roleValueToString(roleValue);
    if (isEmpty(roleValueString))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    return OpaqueJSString::tryCreate(makeString("AXRole: "_s, roleValueString));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::subrole()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::roleDescription()
{
    m_element->updateBackingStore();
    auto roleDescription = m_element->attributes().get("roledescription"_s);
    return OpaqueJSString::tryCreate(makeString("AXRoleDescription: "_s, roleDescription));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::computedRoleString()
{
    m_element->updateBackingStore();
    auto computedRole = m_element->attributes().get("computed-role"_s);
    if (isEmpty(computedRole))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    return OpaqueJSString::tryCreate(computedRole);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::title()
{
    m_element->updateBackingStore();
    auto titleValue = makeString("AXTitle: "_s, m_element->name().span());
    return OpaqueJSString::tryCreate(titleValue);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::description()
{
    m_element->updateBackingStore();
    auto descriptionValue = makeString("AXDescription: "_s, m_element->description().span());
    return OpaqueJSString::tryCreate(descriptionValue);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::orientation() const
{
    m_element->updateBackingStore();
    ASCIILiteral orientation;
    if (checkElementState(m_element.get(), WebCore::Atspi::State::Horizontal))
        orientation = "AXHorizontalOrientation"_s;
    else if (checkElementState(m_element.get(), WebCore::Atspi::State::Vertical))
        orientation = "AXVerticalOrientation"_s;
    else
        orientation = "AXUnknownOrientation"_s;

    auto orientationValue = makeString("AXOrientation: "_s, orientation);
    return OpaqueJSString::tryCreate(orientationValue);
}

bool AccessibilityUIElementAtspi::isAtomicLiveRegion() const
{
    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::liveRegionRelevant() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::liveRegionStatus() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::stringValue()
{
    m_element->updateBackingStore();
    if (m_element->role() == WebCore::Atspi::Role::ComboBox) {
        // Tests expect the combo box to expose the selected element name as the string value.
        if (auto menu = childAtIndex(0)) {
            if (auto* selectedChild = static_cast<AccessibilityUIElementAtspi*>(menu.get())->m_element->selectedChild(0))
                return OpaqueJSString::tryCreate(makeString("AXValue: "_s, selectedChild->name().span()));
        }
    }

    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    auto value = makeString("AXValue: "_s, makeStringByReplacingAll(makeStringByReplacingAll(m_element->text(), '\n', "<\\n>"_s), objectReplacementCharacter, "<obj>"_s));
    return OpaqueJSString::tryCreate(value);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::language()
{
    m_element->updateBackingStore();
    auto locale = m_element->locale();
    if (isEmpty(locale))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    return OpaqueJSString::tryCreate(makeString("AXLanguage: "_s, locale));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::helpText() const
{
    m_element->updateBackingStore();
    auto relationMap = m_element->relationMap();
    auto targets = relationMap.get(WebCore::Atspi::Relation::DescribedBy);
    if (targets.isEmpty())
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    StringBuilder builder;
    builder.append("AXHelp: "_s);

    bool isFirst = true;
    for (const auto& target : targets) {
        if (!isFirst)
            builder.append(' ');
        isFirst = false;
        target->updateBackingStore();
        builder.append(target->text());
    }

    return OpaqueJSString::tryCreate(builder.toString());
}

double AccessibilityUIElementAtspi::pageX()
{
    return 0;
}

double AccessibilityUIElementAtspi::pageY()
{
    return 0;
}

double AccessibilityUIElementAtspi::x()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).x();
}

double AccessibilityUIElementAtspi::y()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).y();
}

double AccessibilityUIElementAtspi::width()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).width();
}

double AccessibilityUIElementAtspi::height()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).height();
}

double AccessibilityUIElementAtspi::clickPointX()
{
    m_element->updateBackingStore();
    auto rect = m_element->elementRect(WebCore::Atspi::CoordinateType::WindowCoordinates);
    return rect.center().x();
}

double AccessibilityUIElementAtspi::clickPointY()
{
    m_element->updateBackingStore();
    auto rect = m_element->elementRect(WebCore::Atspi::CoordinateType::WindowCoordinates);
    return rect.center().y();
}

double AccessibilityUIElementAtspi::intValue() const
{
    m_element->updateBackingStore();
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return m_element->currentValue();

    // Consider headings as an special case when returning the int value.
    if (m_element->role() == WebCore::Atspi::Role::Heading)
        return toDouble(m_element->attributes().get("level"_s));

    return 0;
}

double AccessibilityUIElementAtspi::minValue()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return 0;

    m_element->updateBackingStore();
    return m_element->minimumValue();
}

double AccessibilityUIElementAtspi::maxValue()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return 0;

    m_element->updateBackingStore();
    return m_element->maximumValue();
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::valueDescription()
{
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    auto value = makeString("AXValueDescription: "_s, attributes.get("valuetext"_s));
    return OpaqueJSString::tryCreate(value);
}

int AccessibilityUIElementAtspi::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElementAtspi::isPressActionSupported()
{
    m_element->updateBackingStore();
    auto name = m_element->actionName();
    return equal(name, "press"_s) || equal(name, "jump"_s);
}

bool AccessibilityUIElementAtspi::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElementAtspi::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElementAtspi::isBusy() const
{
    // FIXME: Implement.
    return false;
}

bool AccessibilityUIElementAtspi::isEnabled()
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Enabled);
}

bool AccessibilityUIElementAtspi::isRequired() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Required);
}

bool AccessibilityUIElementAtspi::isFocused() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Focused);
}

bool AccessibilityUIElementAtspi::isSelected() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Selected);
}

bool AccessibilityUIElementAtspi::isSelectedOptionActive() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Active);
}

bool AccessibilityUIElementAtspi::isExpanded() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Expanded);
}

bool AccessibilityUIElementAtspi::isChecked() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Checked);
}

bool AccessibilityUIElementAtspi::isIndeterminate() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Indeterminate);
}

int AccessibilityUIElementAtspi::hierarchicalLevel() const
{
    m_element->updateBackingStore();
    auto level = m_element->attributes().get("level"_s);
    if (isEmpty(level))
        return 0;

    return parseIntegerAllowingTrailingJunk<int>(level).value_or(0);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::speakAs()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

bool AccessibilityUIElementAtspi::isGrabbed() const
{
    m_element->updateBackingStore();
    return equal(m_element->attributes().get("grabbed"_s), "true"_s);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::ariaDropEffects() const
{
    m_element->updateBackingStore();
    auto dropEffects = m_element->attributes().get("dropeffect"_s);
    if (isEmpty(dropEffects))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
    return OpaqueJSString::tryCreate(dropEffects);
}

int AccessibilityUIElementAtspi::lineForIndex(int index)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return -1;

    m_element->updateBackingStore();
    auto text = m_element->text();
    if (index < 0 || index > static_cast<int>(length(text)))
        return -1;

    int lineNumber = 0;
    for (int i = 0; i < index; ++i) {
        if (codeUnitAt(text, i) == '\n')
            lineNumber++;
    }

    return lineNumber;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::rangeForLine(int line)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    WebCore::IntPoint offset;
    for (int i = 0; i <= line; ++i)
        offset = m_element->boundaryOffset(offset.y(), WebCore::AccessibilityObjectAtspi::TextGranularity::LineStart);

    auto range = makeString('{', offset.x(), ", "_s, offset.y() - offset.x(), '}');
    return OpaqueJSString::tryCreate(range);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::rangeForPosition(int x, int y)
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::boundsForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto rect = m_element->boundsForRange(location, length, WebCore::Atspi::CoordinateType::WindowCoordinates);
    auto bounds = makeString('{', rect.x(), ", "_s, rect.y(), ", "_s, rect.width(), ", "_s, rect.height(), '}');
    return OpaqueJSString::tryCreate(bounds);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::stringForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(substring(m_element->text(), location, length));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributedStringForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto text = m_element->text();
    auto limit = location + length;

    if (limit > WTF::length(text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    StringBuilder builder;

    auto buildAttributes = [&](const WebCore::AccessibilityObjectAtspi::TextAttributes& attributes) {
        for (const auto& it : attributes.attributes) {
            builder.append("\n\t\t"_s);
            builder.append(it.key, ':', it.value);
        }
    };

    m_element->updateBackingStore();
    builder.append("\n\tDefault text attributes:"_s);
    buildAttributes(m_element->textAttributes());

    int endOffset = 0;
    for (unsigned i = location; i < limit; i = endOffset) {
        auto attributes = m_element->textAttributes(i);
        auto rangeStart = std::max<int>(location, attributes.startOffset);
        auto rangeEnd = std::min<int>(limit, attributes.endOffset);
        builder.append("\n\tRange attributes for '"_s, makeStringByReplacingAll(makeStringByReplacingAll(substring(text, rangeStart, rangeEnd - rangeStart), '\n', "<\\n>"_s), objectReplacementCharacter, "<obj>"_s), "':"_s);
        buildAttributes(attributes);
        endOffset = attributes.endOffset;
    }

    return OpaqueJSString::tryCreate(builder.toString());
}

bool AccessibilityUIElementAtspi::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    return false;
}

unsigned AccessibilityUIElementAtspi::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return nullptr;
}

JSValueRef AccessibilityUIElementAtspi::uiElementsForSearchPredicate(JSContextRef, AccessibilityUIElement*, bool, JSValueRef, JSStringRef, bool, bool, unsigned)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfColumnHeaders()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->columnHeaders()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfRowHeaders()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->rowHeaders()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfColumns()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfRows()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->rows()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfVisibleCells()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->cells()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributesOfHeader()
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

int AccessibilityUIElementAtspi::rowCount()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return 0;

    m_element->updateBackingStore();
    return m_element->rowCount();
}

int AccessibilityUIElementAtspi::columnCount()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return 0;

    m_element->updateBackingStore();
    return m_element->columnCount();
}

int AccessibilityUIElementAtspi::indexInTable()
{
    return -1;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::rowIndexRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto position = m_element->cellPosition().first;
    auto span = m_element->rowSpan();
    if (!position || !span)
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    return OpaqueJSString::tryCreate(makeString('{', *position, ", "_s, span, '}'));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::columnIndexRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto position = m_element->cellPosition().second;
    auto span = m_element->columnSpan();
    if (!position || !span)
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    return OpaqueJSString::tryCreate(makeString('{', *position, ", "_s, span, '}'));
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::cellForColumnAndRow(unsigned column, unsigned row)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return nullptr;

    m_element->updateBackingStore();
    if (auto* cell = m_element->cell(row, column))
        return AccessibilityUIElementAtspi::create(cell);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::horizontalScrollbar() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::verticalScrollbar() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::selectedTextRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto offset = m_element->selectedRange();
    auto range = makeString('{', offset.x(), ", "_s, offset.y() - offset.x(), '}');
    return OpaqueJSString::tryCreate(range);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementAtspi::intersectionWithSelectionRange()
{
    return nullptr;
}

bool AccessibilityUIElementAtspi::setSelectedTextRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return false;

    m_element->updateBackingStore();
    auto textLength = WTF::length(m_element->text());
    m_element->setSelectedRange(std::min(location, textLength), std::min(length, textLength));
    return true;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::textInputMarkedRange() const
{
    return nullptr;
}

void AccessibilityUIElementAtspi::increment()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return;

    m_element->updateBackingStore();
    m_element->increment();
}

void AccessibilityUIElementAtspi::decrement()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return;

    m_element->updateBackingStore();
    m_element->decrement();
}

void AccessibilityUIElementAtspi::showMenu()
{
}

void AccessibilityUIElementAtspi::press()
{
    m_element->updateBackingStore();
    m_element->doAction();
}

bool AccessibilityUIElementAtspi::dismiss()
{
    m_element->updateBackingStore();
    return m_element->dismiss();
}

void AccessibilityUIElementAtspi::setSelectedChild(AccessibilityUIElement* element) const
{
}

void AccessibilityUIElementAtspi::setSelectedChildAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->setChildSelected(index, /* selected */ true);
}

void AccessibilityUIElementAtspi::removeSelectionAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->setChildSelected(index, /* selected */ false);
}

void AccessibilityUIElementAtspi::clearSelectedChildren() const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->clearSelection();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::activeElement() const
{
    m_element->updateBackingStore();
    if (auto* activeDescendant = m_element->activeDescendant())
        return AccessibilityUIElementAtspi::create(activeDescendant);
    return nullptr;
}

JSValueRef AccessibilityUIElementAtspi::selectedChildren(JSContextRef context)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return makeJSArray(context, { });

    m_element->updateBackingStore();
    return makeJSArray(context, elementsVector(m_element->selectedChildren()));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::accessibilityValue() const
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::url()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Hyperlink))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto axURL = m_element->url();
    if (axURL.isNull())
        return adoptRef(JSStringCreateWithUTF8CString("AXURL: (null)"));

    auto stringURL = axURL.string();
    if (axURL.protocolIsFile()) {
        // Do not expose absolute paths.
        auto index = find(stringURL, "LayoutTests"_s);
        if (index != notFound)
            stringURL = substring(stringURL, index);
    }
    return OpaqueJSString::tryCreate(makeString("AXURL: "_s, stringURL));
}

bool AccessibilityUIElementAtspi::addNotificationListener(JSContextRef, JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    if (m_notificationHandler)
        return false;

    m_notificationHandler = makeUnique<AccessibilityNotificationHandler>(functionCallback, m_element.get());
    return true;
}

bool AccessibilityUIElementAtspi::removeNotificationListener()
{
    ASSERT(m_notificationHandler);
    m_notificationHandler = nullptr;
    return true;
}

bool AccessibilityUIElementAtspi::isFocusable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Focusable);
}

bool AccessibilityUIElementAtspi::isSelectable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Selectable);
}

bool AccessibilityUIElementAtspi::isMultiSelectable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Multiselectable);
}

bool AccessibilityUIElementAtspi::isVisible() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Visible);
}

bool AccessibilityUIElementAtspi::isOffScreen() const
{
    m_element->updateBackingStore();
    return !checkElementState(m_element.get(), WebCore::Atspi::State::Showing);
}

bool AccessibilityUIElementAtspi::isCollapsed() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Collapsed);
}

bool AccessibilityUIElementAtspi::isIgnored() const
{
    m_element->updateBackingStore();
    return m_element->isIgnored();
}

bool AccessibilityUIElementAtspi::isSingleLine() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::SingleLine);
}

bool AccessibilityUIElementAtspi::isMultiLine() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::MultiLine);
}

bool AccessibilityUIElementAtspi::hasPopup() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::HasPopup);
}

void AccessibilityUIElementAtspi::takeFocus()
{
    m_element->updateBackingStore();
    m_element->focus();
}

void AccessibilityUIElementAtspi::takeSelection()
{
}

void AccessibilityUIElementAtspi::addSelection()
{
}

void AccessibilityUIElementAtspi::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementAtspi::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementAtspi::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    return nullptr;
}

int AccessibilityUIElementAtspi::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::rectsForTextMarkerRange(AccessibilityTextMarkerRange* markerRange, JSStringRef searchText)
{
    return adoptRef(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementAtspi::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::textMarkerForPoint(int x, int y)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
}

bool AccessibilityUIElementAtspi::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    return false;
}

int AccessibilityUIElementAtspi::indexForTextMarker(AccessibilityTextMarker* marker)
{
    return -1;
}

bool AccessibilityUIElementAtspi::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::textMarkerForIndex(int textIndex)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::startTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementAtspi::endTextMarker()
{
    return nullptr;
}

bool AccessibilityUIElementAtspi::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return false;
}

void AccessibilityUIElementAtspi::scrollToMakeVisible()
{
    m_element->updateBackingStore();
    m_element->scrollToMakeVisible(WebCore::Atspi::ScrollType::Anywhere);
}

void AccessibilityUIElementAtspi::scrollToGlobalPoint(int x, int y)
{
    m_element->updateBackingStore();
    m_element->scrollToPoint({ x, y }, WebCore::Atspi::CoordinateType::WindowCoordinates);
}

void AccessibilityUIElementAtspi::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::supportedActions() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::pathDescription() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::pathAsBounds() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::mathPostscriptsDescription() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::mathPrescriptsDescription() const
{
    return nullptr;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::classList() const
{
    return nullptr;
}

static RefPtr<StringImpl> stringAtOffset(WebCore::AccessibilityObjectAtspi* element, int offset, WebCore::AccessibilityObjectAtspi::TextGranularity granularity)
{
    if (!element || !element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return { };

    element->updateBackingStore();
    auto text = element->text();
    if (offset < 0 || offset > static_cast<int>(length(text)))
        return { };

    auto bounds = element->boundaryOffset(offset, granularity);
    unsigned startOffset = std::max<int>(bounds.x(), 0);
    unsigned endOffset = std::min<int>(bounds.y(), length(text));
    return makeString(substring(text, startOffset, endOffset - startOffset), ", "_s, startOffset, ", "_s, endOffset);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::characterAtOffset(int offset)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    m_element->updateBackingStore();
    auto text = m_element->text();
    if (offset < 0 || offset > static_cast<int>(length(text)))
        return adoptRef(JSStringCreateWithCharacters(nullptr, 0));

    auto string = makeString(substring(text, offset, 1), ", "_s, offset, ", "_s, offset + 1);
    return OpaqueJSString::tryCreate(string);
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::wordAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::WordStart));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::lineAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::LineStart));
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::sentenceAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::SentenceStart));
}


RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::focusableAncestor()
{
    m_element->updateBackingStore();
    if (auto ancestor = m_element->focusableAncestor())
        return AccessibilityUIElementAtspi::create(ancestor);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::editableAncestor()
{
    m_element->updateBackingStore();
    if (auto ancestor = m_element->editableAncestor())
        return AccessibilityUIElementAtspi::create(ancestor);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementAtspi::highestEditableAncestor()
{
    m_element->updateBackingStore();
    if (auto ancestor = m_element->highestEditableAncestor())
        return AccessibilityUIElementAtspi::create(ancestor);
    return nullptr;
}

bool AccessibilityUIElementAtspi::replaceTextInRange(JSStringRef, int, int)
{
    return false;
}

bool AccessibilityUIElementAtspi::insertText(JSStringRef)
{
    return false;
}

RefPtr<OpaqueJSString> AccessibilityUIElementAtspi::popupValue() const
{
    return nullptr;
}

bool AccessibilityUIElementAtspi::isInsertion() const
{
    return false;
}

bool AccessibilityUIElementAtspi::isDeletion() const
{
    return false;
}

bool AccessibilityUIElementAtspi::isFirstItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElementAtspi::isLastItemInSuggestion() const
{
    return false;
}

} // namespace WTR

#endif // USE(ATSPI)
