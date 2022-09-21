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
#include "AccessibilityUIElement.h"

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
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTR {

RefPtr<AccessibilityController> AccessibilityUIElement::s_controller;

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
    if (!s_controller)
        s_controller = InjectedBundle::singleton().accessibilityController();
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : JSWrappable()
    , m_element(other.m_element)
{
}

AccessibilityUIElement::~AccessibilityUIElement()
{
}

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    return otherElement && m_element.get() == otherElement->platformUIElement();
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& children)
{
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement> >& children, unsigned location, unsigned length)
{
}

int AccessibilityUIElement::childrenCount()
{
    m_element->updateBackingStore();
    return m_element->childCount();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int x, int y)
{
    m_element->updateBackingStore();
    auto* element = m_element->hitTest({ x, y }, WebCore::Atspi::CoordinateType::WindowCoordinates);
    return AccessibilityUIElement::create(element ? element : m_element.get());
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned index)
{
    m_element->updateBackingStore();
    if (auto* child = m_element->childAt(index))
        return AccessibilityUIElement::create(child);
    return nullptr;
}

static RefPtr<AccessibilityUIElement> elementForRelationAtIndex(WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::Relation relation, unsigned index)
{
    element->updateBackingStore();
    auto relationMap = element->relationMap();
    auto targets = relationMap.get(relation);
    if (targets.isEmpty() || index >= targets.size())
        return nullptr;

    auto target = targets[index];
    if (!target)
        return nullptr;

    return AccessibilityUIElement::create(target.get());
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::NodeParentOf, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::NodeChildOf, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::FlowsTo, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::FlowsFrom, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ControllerFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ControlledBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaLabelledByElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelledBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaLabelledByReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDescribedByElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DescribedBy, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDescribedByReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DescriptionFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::Details, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::DetailsFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ErrorMessage, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageReferencingElementAtIndex(unsigned index)
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::ErrorFor, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned index)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return nullptr;

    m_element->updateBackingStore();
    auto rows = m_element->rows();
    if (index >= rows.size())
        return nullptr;

    return AccessibilityUIElement::create(rows[index].get());
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return nullptr;

    m_element->updateBackingStore();
    if (auto* selectedChild = m_element->selectedChild(index))
        return AccessibilityUIElement::create(selectedChild);
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return 0;

    m_element->updateBackingStore();
    return m_element->selectionCount();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    return elementForRelationAtIndex(m_element.get(), WebCore::Atspi::Relation::LabelledBy, 0);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    m_element->updateBackingStore();
    if (auto* parent = m_element->parent().value_or(nullptr))
        return AccessibilityUIElement::create(parent);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

static String attributesOfElement(AccessibilityUIElement& element)
{
    StringBuilder builder;

    builder.append(element.role()->string(), '\n');

    builder.append("AXParent: ");
    if (auto parent = element.parentElement()) {
        builder.append(parent->role()->string().substring(8));
        auto parentName = parent->title()->string().substring(9);
        if (!parentName.isEmpty())
            builder.append(": ", parentName);
    } else
        builder.append("(null)");
    builder.append('\n');

    builder.append("AXChildren: ", element.childrenCount(), '\n');

    builder.append("AXPosition:  { ", FormattedNumber::fixedPrecision(element.x(), 6, KeepTrailingZeros));
    builder.append(", ", FormattedNumber::fixedPrecision(element.y(), 6, KeepTrailingZeros));
    builder.append(" }\n");

    builder.append("AXSize: { ", FormattedNumber::fixedPrecision(element.width(), 6, KeepTrailingZeros));
    builder.append(", ", FormattedNumber::fixedPrecision(element.height(), 6, KeepTrailingZeros));
    builder.append(" }\n");

    String title = element.title()->string();
    if (!title.isEmpty()) {
        builder.append(title);
        builder.append('\n');
    }

    String description = element.description()->string();
    if (!description.isEmpty())
        builder.append(description.utf8().data(), '\n');

    String value = element.stringValue()->string();
    if (!value.isEmpty())
        builder.append(value, '\n');

    builder.append("AXFocusable: ", element.isFocusable(), '\n');
    builder.append("AXFocused: ", element.isFocused(), '\n');
    builder.append("AXSelectable: ", element.isSelectable(), '\n');
    builder.append("AXSelected: ", element.isSelected(), '\n');
    builder.append("AXMultiSelectable: ", element.isMultiSelectable(), '\n');
    builder.append("AXEnabled: ", element.isEnabled(), '\n');
    builder.append("AXExpanded: ", element.isExpanded(), '\n');
    builder.append("AXRequired: ", element.isRequired(), '\n');
    builder.append("AXChecked: ", element.isChecked(), '\n');

    String url = element.url()->string();
    if (!url.isEmpty())
        builder.append(url, '\n');

    // We append the platform attributes as a single line at the end.
    builder.append("AXPlatformAttributes: ");
    auto attributes = element.platformUIElement()->attributes();
    auto keys = copyToVector(attributes.keys());
    std::sort(keys.begin(), keys.end(), WTF::codePointCompareLessThan);

    bool isFirst = true;
    for (const auto& key : keys) {
        if (key == "id"_s || key == "toolkit"_s)
            continue;

        if (!isFirst)
            builder.append(", ");
        isFirst = false;
        builder.append(key, ':', attributes.get(key));
    }

    return builder.toString();
}

static String attributesOfElements(Vector<RefPtr<AccessibilityUIElement>>& elements)
{
    StringBuilder builder;
    for (auto& element : elements)
        builder.append(attributesOfElement(*element), "\n------------\n");
    return builder.toString();
}

static Vector<RefPtr<AccessibilityUIElement>> elementsVector(const Vector<RefPtr<WebCore::AccessibilityObjectAtspi>>& wrappers)
{
    Vector<RefPtr<AccessibilityUIElement>> elements;
    elements.reserveInitialCapacity(wrappers.size());
    for (auto& wrapper : wrappers)
        elements.uncheckedAppend(AccessibilityUIElement::create(wrapper.get()));
    return elements;
}

static String attributesOfElements(const Vector<RefPtr<WebCore::AccessibilityObjectAtspi>>& wrappers)
{
    auto elements = elementsVector(wrappers);
    return attributesOfElements(elements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->children())).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return OpaqueJSString::tryCreate(attributesOfElement(*this)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

static bool checkElementState(WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::State state)
{
    return element->state() & (G_GUINT64_CONSTANT(1) << state);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    String attributeName = toWTFString(attribute);
    if (attributeName == "AXSelectedText"_s) {
        if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
            return JSStringCreateWithCharacters(nullptr, 0);

        m_element->updateBackingStore();
        auto text = m_element->text();
        auto offset = m_element->selectedRange();
        return OpaqueJSString::tryCreate(text.substring(offset.x(), offset.y() - offset.x())).leakRef();
    }

    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    if (attributeName == "AXPlaceholderValue"_s)
        return OpaqueJSString::tryCreate(attributes.get("placeholder-text"_s)).leakRef();
    if (attributeName == "AXInvalid"_s) {
        auto textAttributes = m_element->textAttributes();
        auto value = textAttributes.attributes.get("invalid"_s);
        if (value.isEmpty())
            value = checkElementState(m_element.get(), WebCore::Atspi::State::InvalidEntry) ? "true"_s : "false"_s;
        return OpaqueJSString::tryCreate(value).leakRef();
    }
    if (attributeName == "AXARIALive"_s)
        return OpaqueJSString::tryCreate(attributes.get("live"_s)).leakRef();
    if (attributeName == "AXARIARelevant"_s)
        return OpaqueJSString::tryCreate(attributes.get("relevant"_s)).leakRef();

    return JSStringCreateWithCharacters(nullptr, 0);
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    String attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    if (attributeName == "AXARIASetSize"_s)
        return attributes.get("setsize"_s).toDouble();
    if (attributeName == "AXARIAPosInSet"_s)
        return attributes.get("posinset"_s).toDouble();
    if (attributeName == "AXARIAColumnCount"_s)
        return attributes.get("colcount"_s).toDouble();
    if (attributeName == "AXARIARowCount"_s)
        return attributes.get("rowcount"_s).toDouble();
    if (attributeName == "AXARIAColumnIndex"_s)
        return attributes.get("colindex"_s).toDouble();
    if (attributeName == "AXARIARowIndex"_s)
        return attributes.get("rowindex"_s).toDouble();
    if (attributeName == "AXARIAColumnSpan"_s)
        return attributes.get("colspan"_s).toDouble();
    if (attributeName == "AXARIARowSpan"_s)
        return attributes.get("rowspan"_s).toDouble();

    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::currentStateValue() const
{
    m_element->updateBackingStore();
    auto value = m_element->attributes().get("current"_s);
    return OpaqueJSString::tryCreate(!value.isNull() ? value : "false"_s).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(m_element->attributes().get("id"_s)).leakRef();
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    return nullptr;
}

static JSValueRef makeJSArray(const Vector<RefPtr<AccessibilityUIElement>>& elements)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    size_t elementCount = elements.size();
    auto valueElements = makeUniqueArray<JSValueRef>(elementCount);
    for (size_t i = 0; i < elementCount; i++)
        valueElements[i] = JSObjectMake(context, elements[i]->wrapperClass(), elements[i].get());

    return JSObjectMakeArray(context, elementCount, valueElements.get(), nullptr);
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table)) {
        m_element->updateBackingStore();
        return makeJSArray(elementsVector(m_element->rowHeaders()));
    }

    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell)) {
        m_element->updateBackingStore();
        return makeJSArray(elementsVector(m_element->cellRowHeaders()));
    }

    return makeJSArray({ });
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table)) {
        m_element->updateBackingStore();
        return makeJSArray(elementsVector(m_element->columnHeaders()));
    }

    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell)) {
        m_element->updateBackingStore();
        return makeJSArray(elementsVector(m_element->cellColumnHeaders()));
    }

    return makeJSArray({ });
}

static JSValueRef elementsForRelation(WebCore::AccessibilityObjectAtspi* element, WebCore::Atspi::Relation relation)
{
    element->updateBackingStore();
    auto relationMap = element->relationMap();
    auto targets = relationMap.get(relation);
    if (targets.isEmpty())
        return { };

    return makeJSArray(elementsVector(targets));
}

JSValueRef AccessibilityUIElement::detailsElements() const
{
    return elementsForRelation(m_element.get(), WebCore::Atspi::Relation::Details);
}

JSValueRef AccessibilityUIElement::errorMessageElements() const
{
    return elementsForRelation(m_element.get(), WebCore::Atspi::Relation::ErrorMessage);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    return nullptr;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    String attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    if (attributeName == "AXElementBusy"_s)
        return checkElementState(m_element.get(), WebCore::Atspi::State::Busy);
    if (attributeName == "AXModal"_s)
        return checkElementState(m_element.get(), WebCore::Atspi::State::Modal);
    if (attributeName == "AXSupportsAutoCompletion"_s)
        return checkElementState(m_element.get(), WebCore::Atspi::State::SupportsAutocompletion);
    if (attributeName == "AXInterfaceTable"_s)
        return m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table);
    if (attributeName == "AXInterfaceTableCell"_s)
        return m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell);
    if (attributeName == "AXARIAAtomic"_s)
        return m_element->attributes().get("atomic"_s) == "true"_s;

    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    String attributeName = toWTFString(attribute);
    if (attributeName != "AXValue"_s)
        return false;

    m_element->updateBackingStore();
    if (checkElementState(m_element.get(), WebCore::Atspi::State::ReadOnly))
        return false;

    if (checkElementState(m_element.get(), WebCore::Atspi::State::Editable))
        return true;

    if (checkElementState(m_element.get(), WebCore::Atspi::State::Checkable))
        return true;

    auto attributes = m_element->attributes();
    String isReadOnly = attributes.get("readonly"_s);
    if (!isReadOnly.isEmpty())
        return isReadOnly == "true"_s ? false : true;

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
                return checkElementState(child->m_element.get(), WebCore::Atspi::State::Selectable);
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

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    String attributeName = toWTFString(attribute);
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    if (attributeName == "AXARIASetSize"_s)
        return attributes.contains("setsize"_s);
    if (attributeName == "AXARIAPosInSet"_s)
        return attributes.contains("posinset"_s);
    if (attributeName == "AXARIALive"_s) {
        auto value = attributes.get("live"_s);
        return !value.isEmpty() && value != "off"_s;
    }
    if (attributeName == "AXARIARelevant"_s)
        return attributes.contains("relevant"_s);
    if (attributeName == "AXARIAAtomic"_s)
        return attributes.contains("atomic"_s);
    if (attributeName == "AXElementBusy"_s)
        return true;

    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

static String xmlRoleValueString(const String& xmlRoles)
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> regionRoles = HashSet<String, ASCIICaseInsensitiveHash>({
        "doc-acknowledgments"_s,
        "doc-afterword"_s,
        "doc-appendix"_s,
        "doc-bibliography"_s,
        "doc-chapter"_s,
        "doc-conclusion"_s,
        "doc-credits"_s,
        "doc-endnotes"_s,
        "doc-epilogue"_s,
        "doc-errata"_s,
        "doc-foreword"_s,
        "doc-glossary"_s,
        "doc-glossref"_s,
        "doc-index"_s,
        "doc-introduction"_s,
        "doc-pagelist"_s,
        "doc-part"_s,
        "doc-preface"_s,
        "doc-prologue"_s,
        "doc-toc"_s,
        "region"_s
    });

    if (regionRoles->contains(xmlRoles))
        return "AXLandmarkRegion"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "banner"_s))
        return "AXLandmarkBanner"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "complementary"_s))
        return "AXLandmarkComplementary"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "contentinfo"_s))
        return "AXLandmarkContentInfo"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "form"_s))
        return "AXLandmarkForm"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "main"_s))
        return "AXLandmarkMain"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "navigation"_s))
        return "AXLandmarkNavigation"_s;
    if (equalLettersIgnoringASCIICase(xmlRoles, "search"_s))
        return "AXLandmarkSearch"_s;

    return { };
}

static String roleValueToString(unsigned roleValue)
{
    switch (roleValue) {
    case WebCore::Atspi::Role::Alert:
        return "AXAlert"_s;
    case WebCore::Atspi::Role::Article:
        return "AXArticle"_s;
    case WebCore::Atspi::Role::Audio:
        return "AXAudio"_s;
    case WebCore::Atspi::Role::BlockQuote:
        return "AXBlockquote"_s;
    case WebCore::Atspi::Role::Canvas:
        return "AXCanvas"_s;
    case WebCore::Atspi::Role::Caption:
        return "AXCaption"_s;
    case WebCore::Atspi::Role::CheckBox:
        return "AXCheckBox"_s;
    case WebCore::Atspi::Role::CheckMenuItem:
        return "AXCheckMenuItem"_s;
    case WebCore::Atspi::Role::ColorChooser:
        return "AXColorWell"_s;
    case WebCore::Atspi::Role::ColumnHeader:
    case WebCore::Atspi::Role::TableColumnHeader:
        return "AXColumnHeader"_s;
    case WebCore::Atspi::Role::ComboBox:
        return "AXComboBox"_s;
    case WebCore::Atspi::Role::Comment:
        return "AXComment"_s;
    case WebCore::Atspi::Role::ContentDeletion:
        return "AXDeletion"_s;
    case WebCore::Atspi::Role::ContentInsertion:
        return "AXInsertion"_s;
    case WebCore::Atspi::Role::Definition:
        return "AXDefinition"_s;
    case WebCore::Atspi::Role::DescriptionList:
        return "AXDescriptionList"_s;
    case WebCore::Atspi::Role::DescriptionTerm:
        return "AXDescriptionTerm"_s;
    case WebCore::Atspi::Role::DescriptionValue:
        return "AXDescriptionValue"_s;
    case WebCore::Atspi::Role::Dialog:
        return "AXDialog"_s;
    case WebCore::Atspi::Role::DocumentFrame:
        return "AXDocument"_s;
    case WebCore::Atspi::Role::DocumentWeb:
        return "AXWebArea"_s;
    case WebCore::Atspi::Role::Embedded:
        return "AXEmbedded"_s;
    case WebCore::Atspi::Role::Entry:
        return "AXTextField"_s;
    case WebCore::Atspi::Role::Footer:
        return "AXFooter"_s;
    case WebCore::Atspi::Role::Footnote:
        return "AXFootnote"_s;
    case WebCore::Atspi::Role::Form:
        return "AXForm"_s;
    case WebCore::Atspi::Role::Grouping:
    case WebCore::Atspi::Role::Panel:
        return "AXGroup"_s;
    case WebCore::Atspi::Role::Heading:
        return "AXHeading"_s;
    case WebCore::Atspi::Role::Image:
        return "AXImage"_s;
    case WebCore::Atspi::Role::ImageMap:
        return "AXImageMap"_s;
    case WebCore::Atspi::Role::InvalidRole:
        return "AXInvalid"_s;
    case WebCore::Atspi::Role::Label:
        return "AXLabel"_s;
    case WebCore::Atspi::Role::LevelBar:
        return "AXLevelIndicator"_s;
    case WebCore::Atspi::Role::Link:
        return "AXLink"_s;
    case WebCore::Atspi::Role::ListBox:
        return "AXListBox"_s;
    case WebCore::Atspi::Role::List:
        return "AXList"_s;
    case WebCore::Atspi::Role::ListItem:
        return "AXListItem"_s;
    case WebCore::Atspi::Role::Log:
        return "AXLog"_s;
    case WebCore::Atspi::Role::Marquee:
        return "AXMarquee"_s;
    case WebCore::Atspi::Role::Math:
        return "AXMath"_s;
    case WebCore::Atspi::Role::MathFraction:
        return "AXMathFraction"_s;
    case WebCore::Atspi::Role::MathRoot:
        return "AXMathRoot"_s;
    case WebCore::Atspi::Role::Menu:
        return "AXMenu"_s;
    case WebCore::Atspi::Role::MenuBar:
        return "AXMenuBar"_s;
    case WebCore::Atspi::Role::MenuItem:
        return "AXMenuItem"_s;
    case WebCore::Atspi::Role::Notification:
        return "AXNotification"_s;
    case WebCore::Atspi::Role::PageTab:
        return "AXTab"_s;
    case WebCore::Atspi::Role::PageTabList:
        return "AXTabGroup"_s;
    case WebCore::Atspi::Role::Paragraph:
        return "AXParagraph"_s;
    case WebCore::Atspi::Role::PasswordText:
        return "AXPasswordField"_s;
    case WebCore::Atspi::Role::ProgressBar:
        return "AXProgressIndicator"_s;
    case WebCore::Atspi::Role::PushButton:
        return "AXButton"_s;
    case WebCore::Atspi::Role::RadioButton:
        return "AXRadioButton"_s;
    case WebCore::Atspi::Role::RadioMenuItem:
        return "AXRadioMenuItem"_s;
    case WebCore::Atspi::Role::RowHeader:
    case WebCore::Atspi::Role::TableRowHeader:
        return "AXRowHeader"_s;
    case WebCore::Atspi::Role::Ruler:
        return "AXRuler"_s;
    case WebCore::Atspi::Role::ScrollBar:
        return "AXScrollBar"_s;
    case WebCore::Atspi::Role::ScrollPane:
        return "AXScrollArea"_s;
    case WebCore::Atspi::Role::Section:
        return "AXSection"_s;
    case WebCore::Atspi::Role::Separator:
        return "AXSeparator"_s;
    case WebCore::Atspi::Role::Slider:
        return "AXSlider"_s;
    case WebCore::Atspi::Role::SpinButton:
        return "AXSpinButton"_s;
    case WebCore::Atspi::Role::Static:
    case WebCore::Atspi::Role::Text:
        return "AXStatic"_s;
    case WebCore::Atspi::Role::StatusBar:
        return "AXStatusBar"_s;
    case WebCore::Atspi::Role::Subscript:
        return "AXSubscript"_s;
    case WebCore::Atspi::Role::Superscript:
        return "AXSuperscript"_s;
    case WebCore::Atspi::Role::Table:
        return "AXTable"_s;
    case WebCore::Atspi::Role::TableCell:
        return "AXCell"_s;
    case WebCore::Atspi::Role::TableRow:
        return "AXRow"_s;
    case WebCore::Atspi::Role::Timer:
        return "AXTimer"_s;
    case WebCore::Atspi::Role::ToggleButton:
        return "AXToggleButton"_s;
    case WebCore::Atspi::Role::ToolBar:
        return "AXToolbar"_s;
    case WebCore::Atspi::Role::ToolTip:
        return "AXUserInterfaceTooltip"_s;
    case WebCore::Atspi::Role::Tree:
        return "AXTree"_s;
    case WebCore::Atspi::Role::TreeTable:
        return "AXTreeGrid"_s;
    case WebCore::Atspi::Role::TreeItem:
        return "AXTreeItem"_s;
    case WebCore::Atspi::Role::Unknown:
        return "AXUnknown"_s;
    case WebCore::Atspi::Role::Video:
        return "AXVideo"_s;
    case WebCore::Atspi::Role::Window:
        return "AXWindow"_s;
    default:
        break;
    }

    return { };
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    m_element->updateBackingStore();
    auto roleValue = m_element->role();
    auto roleValueString = roleValue == WebCore::Atspi::Role::Landmark ? xmlRoleValueString(m_element->attributes().get("xml-roles"_s)) : roleValueToString(roleValue);
    if (roleValueString.isEmpty())
        return JSStringCreateWithCharacters(nullptr, 0);

    return OpaqueJSString::tryCreate(makeString("AXRole: ", roleValueString)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    m_element->updateBackingStore();
    auto roleDescription = m_element->attributes().get("roledescription"_s);
    return OpaqueJSString::tryCreate(makeString("AXRoleDescription: ", roleDescription)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    m_element->updateBackingStore();
    auto computedRole = m_element->attributes().get("computed-role"_s);
    if (computedRole.isEmpty())
        return JSStringCreateWithCharacters(nullptr, 0);

    return OpaqueJSString::tryCreate(computedRole).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    m_element->updateBackingStore();
    auto titleValue = makeString("AXTitle: ", String::fromUTF8(m_element->name()));
    return OpaqueJSString::tryCreate(titleValue).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    m_element->updateBackingStore();
    auto descriptionValue = makeString("AXDescription: ", String::fromUTF8(m_element->description()));
    return OpaqueJSString::tryCreate(descriptionValue).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    m_element->updateBackingStore();
    const char* orientation = nullptr;
    if (checkElementState(m_element.get(), WebCore::Atspi::State::Horizontal))
        orientation = "AXHorizontalOrientation";
    else if (checkElementState(m_element.get(), WebCore::Atspi::State::Vertical))
        orientation = "AXVerticalOrientation";
    else
        orientation = "AXUnknownOrientation";

    auto orientationValue = makeString("AXOrientation: ", orientation);
    return OpaqueJSString::tryCreate(orientationValue).leakRef();
}

bool AccessibilityUIElement::isAtomicLiveRegion() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    m_element->updateBackingStore();
    if (m_element->role() == WebCore::Atspi::Role::ComboBox) {
        // Tests expect the combo box to expose the selected element name as the string value.
        if (auto menu = childAtIndex(0)) {
            if (auto* selectedChild = menu->m_element->selectedChild(0))
                return OpaqueJSString::tryCreate(makeString("AXValue: ", String::fromUTF8(selectedChild->name()))).leakRef();
        }
    }

    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    auto value = makeString("AXValue: ", makeStringByReplacingAll(makeStringByReplacingAll(m_element->text(), '\n', "<\\n>"_s), objectReplacementCharacter, "<obj>"_s));
    return OpaqueJSString::tryCreate(value).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    m_element->updateBackingStore();
    auto locale = m_element->locale();
    if (locale.isEmpty())
        return JSStringCreateWithCharacters(nullptr, 0);

    return OpaqueJSString::tryCreate(makeString("AXLanguage: ", locale)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    m_element->updateBackingStore();
    auto relationMap = m_element->relationMap();
    auto targets = relationMap.get(WebCore::Atspi::Relation::DescribedBy);
    if (targets.isEmpty())
        return JSStringCreateWithCharacters(nullptr, 0);

    StringBuilder builder;
    builder.append("AXHelp: ");

    bool isFirst = true;
    for (const auto& target : targets) {
        if (!isFirst)
            builder.append(' ');
        isFirst = false;
        target->updateBackingStore();
        builder.append(target->text());
    }

    return OpaqueJSString::tryCreate(builder.toString()).leakRef();
}

double AccessibilityUIElement::x()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).x();
}

double AccessibilityUIElement::y()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).y();
}

double AccessibilityUIElement::width()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).width();
}

double AccessibilityUIElement::height()
{
    m_element->updateBackingStore();
    return m_element->elementRect(WebCore::Atspi::CoordinateType::ScreenCoordinates).height();
}

double AccessibilityUIElement::clickPointX()
{
    m_element->updateBackingStore();
    auto rect = m_element->elementRect(WebCore::Atspi::CoordinateType::WindowCoordinates);
    return rect.center().x();
}

double AccessibilityUIElement::clickPointY()
{
    m_element->updateBackingStore();
    auto rect = m_element->elementRect(WebCore::Atspi::CoordinateType::WindowCoordinates);
    return rect.center().y();
}

double AccessibilityUIElement::intValue() const
{
    m_element->updateBackingStore();
    if (m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return m_element->currentValue();

    // Consider headings as an special case when returning the int value.
    if (m_element->role() == WebCore::Atspi::Role::Heading)
        return m_element->attributes().get("level"_s).toDouble();

    return 0;
}

double AccessibilityUIElement::minValue()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return 0;

    m_element->updateBackingStore();
    return m_element->minimumValue();
}

double AccessibilityUIElement::maxValue()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return 0;

    m_element->updateBackingStore();
    return m_element->maximumValue();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    m_element->updateBackingStore();
    auto attributes = m_element->attributes();
    auto value = makeString("AXValueDescription: ", attributes.get("valuetext"_s));
    return OpaqueJSString::tryCreate(value).leakRef();
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    m_element->updateBackingStore();
    auto name = m_element->actionName();
    return name == "press"_s || name == "jump"_s;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isBusy() const
{
    // FIXME: Implement.
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Enabled);
}

bool AccessibilityUIElement::isRequired() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Required);
}

bool AccessibilityUIElement::isFocused() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Focused);
}

bool AccessibilityUIElement::isSelected() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Selected);
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Active);
}

bool AccessibilityUIElement::isExpanded() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Expanded);
}

bool AccessibilityUIElement::isChecked() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Checked);
}

bool AccessibilityUIElement::isIndeterminate() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Indeterminate);
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    m_element->updateBackingStore();
    auto level = m_element->attributes().get("level"_s);
    if (level.isEmpty())
        return 0;

    return parseIntegerAllowingTrailingJunk<int>(level).value_or(0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    m_element->updateBackingStore();
    return m_element->attributes().get("grabbed"_s) == "true"_s;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    m_element->updateBackingStore();
    auto dropEffects = m_element->attributes().get("dropeffect"_s);
    if (dropEffects.isEmpty())
        return JSStringCreateWithCharacters(nullptr, 0);
    return OpaqueJSString::tryCreate(dropEffects).leakRef();
}

int AccessibilityUIElement::lineForIndex(int index)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return -1;

    m_element->updateBackingStore();
    auto text = m_element->text();
    if (index < 0 || index > static_cast<int>(text.length()))
        return -1;

    int lineNumber = 0;
    for (int i = 0; i < index; ++i) {
        if (text[i] == '\n')
            lineNumber++;
    }

    return lineNumber;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    WebCore::IntPoint offset;
    for (int i = 0; i <= line; ++i)
        offset = m_element->boundaryOffset(offset.y(), WebCore::AccessibilityObjectAtspi::TextGranularity::LineStart);

    auto range = makeString('{', offset.x(), ", ", offset.y() - offset.x(), '}');
    return OpaqueJSString::tryCreate(range).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto rect = m_element->boundsForRange(location, length, WebCore::Atspi::CoordinateType::WindowCoordinates);
    auto bounds = makeString('{', rect.x(), ", ", rect.y(), ", ", rect.width(), ", ", rect.height(), '}');
    return OpaqueJSString::tryCreate(bounds).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(m_element->text().substring(location, length)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto text = m_element->text();
    auto limit = location + length;

    if (limit > text.length())
        return JSStringCreateWithCharacters(nullptr, 0);

    StringBuilder builder;

    auto buildAttributes = [&](const WebCore::AccessibilityObjectAtspi::TextAttributes& attributes) {
        for (const auto& it : attributes.attributes) {
            builder.append("\n\t\t");
            builder.append(it.key, ':', it.value);
        }
    };

    m_element->updateBackingStore();
    builder.append("\n\tDefault text attributes:");
    buildAttributes(m_element->textAttributes());

    int endOffset = 0;
    for (unsigned i = location; i < limit; i = endOffset) {
        auto attributes = m_element->textAttributes(i);
        auto rangeStart = std::max<int>(location, attributes.startOffset);
        auto rangeEnd = std::min<int>(limit, attributes.endOffset);
        builder.append("\n\tRange attributes for '", makeStringByReplacingAll(makeStringByReplacingAll(text.substring(rangeStart, rangeEnd - rangeStart), '\n', "<\\n>"_s), objectReplacementCharacter, "<obj>"_s), "':");
        buildAttributes(attributes);
        endOffset = attributes.endOffset;
    }

    return OpaqueJSString::tryCreate(builder.toString()).leakRef();
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->columnHeaders())).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->rowHeaders())).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->rows())).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(attributesOfElements(m_element->cells())).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

int AccessibilityUIElement::rowCount()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return 0;

    m_element->updateBackingStore();
    return m_element->rowCount();
}

int AccessibilityUIElement::columnCount()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return 0;

    m_element->updateBackingStore();
    return m_element->columnCount();
}

int AccessibilityUIElement::indexInTable()
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto position = m_element->cellPosition().first;
    auto span = m_element->rowSpan();
    if (!position || !span)
        return JSStringCreateWithCharacters(nullptr, 0);

    return OpaqueJSString::tryCreate(makeString('{', *position, ", ", span, '}')).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::TableCell))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto position = m_element->cellPosition().second;
    auto span = m_element->columnSpan();
    if (!position || !span)
        return JSStringCreateWithCharacters(nullptr, 0);

    return OpaqueJSString::tryCreate(makeString('{', *position, ", ", span, '}')).leakRef();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned column, unsigned row)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Table))
        return nullptr;

    m_element->updateBackingStore();
    if (auto* cell = m_element->cell(row, column))
        return AccessibilityUIElement::create(cell);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto offset = m_element->selectedRange();
    auto range = makeString('{', offset.x(), ", ", offset.y() - offset.x(), '}');
    return OpaqueJSString::tryCreate(range).leakRef();
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return false;

    m_element->updateBackingStore();
    auto textLength = m_element->text().length();
    m_element->setSelectedRange(std::min(location, textLength), std::min(length, textLength));
    return true;
}

void AccessibilityUIElement::increment()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return;

    m_element->updateBackingStore();
    m_element->setCurrentValue(intValue() + m_element->minimumIncrement());
}

void AccessibilityUIElement::decrement()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Value))
        return;

    m_element->updateBackingStore();
    m_element->setCurrentValue(intValue() - m_element->minimumIncrement());
}

void AccessibilityUIElement::showMenu()
{
}

void AccessibilityUIElement::press()
{
    m_element->updateBackingStore();
    m_element->doAction();
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->setChildSelected(index, true);
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->setChildSelected(index, false);
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Selection))
        return;

    m_element->updateBackingStore();
    m_element->clearSelection();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Document))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(m_element->documentAttribute("Encoding"_s)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Document))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    return OpaqueJSString::tryCreate(m_element->documentAttribute("URI"_s)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Hyperlink))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto axURL = m_element->url();
    if (axURL.isNull())
        return JSStringCreateWithUTF8CString("AXURL: (null)");

    auto stringURL = axURL.string();
    if (axURL.isLocalFile()) {
        // Do not expose absolute paths.
        auto index = stringURL.find("LayoutTests"_s);
        if (index != notFound)
            stringURL = stringURL.substring(index);
    }
    return OpaqueJSString::tryCreate(makeString("AXURL: ", stringURL)).leakRef();
}

bool AccessibilityUIElement::addNotificationListener(JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    if (m_notificationHandler)
        return false;

    m_notificationHandler = makeUnique<AccessibilityNotificationHandler>(functionCallback, m_element.get());
    return true;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    ASSERT(m_notificationHandler);
    m_notificationHandler = nullptr;
    return true;
}

bool AccessibilityUIElement::isFocusable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Focusable);
}

bool AccessibilityUIElement::isSelectable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Selectable);
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Multiselectable);
}

bool AccessibilityUIElement::isVisible() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Visible);
}

bool AccessibilityUIElement::isOffScreen() const
{
    m_element->updateBackingStore();
    return !checkElementState(m_element.get(), WebCore::Atspi::State::Showing);
}

bool AccessibilityUIElement::isCollapsed() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::Collapsed);
}

bool AccessibilityUIElement::isIgnored() const
{
    m_element->updateBackingStore();
    return m_element->isIgnored();
}

bool AccessibilityUIElement::isSingleLine() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::SingleLine);
}

bool AccessibilityUIElement::isMultiLine() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::MultiLine);
}

bool AccessibilityUIElement::hasPopup() const
{
    m_element->updateBackingStore();
    return checkElementState(m_element.get(), WebCore::Atspi::State::HasPopup);
}

void AccessibilityUIElement::takeFocus()
{
}

void AccessibilityUIElement::takeSelection()
{
}

void AccessibilityUIElement::addSelection()
{
}

void AccessibilityUIElement::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rectsForTextMarkerRange(AccessibilityTextMarkerRange* markerRange, JSStringRef searchText)
{
    return JSStringCreateWithCharacters(nullptr, 0);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker* marker)
{
    return -1;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return false;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    m_element->updateBackingStore();
    m_element->scrollToMakeVisible(WebCore::Atspi::ScrollType::Anywhere);
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    m_element->updateBackingStore();
    m_element->scrollToPoint({ x, y }, WebCore::Atspi::CoordinateType::WindowCoordinates);
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

static String stringAtOffset(WebCore::AccessibilityObjectAtspi* element, int offset, WebCore::AccessibilityObjectAtspi::TextGranularity granularity)
{
    if (!element || !element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return { };

    element->updateBackingStore();
    auto text = element->text();
    if (offset < 0 || offset > static_cast<int>(text.length()))
        return { };

    auto bounds = element->boundaryOffset(offset, granularity);
    unsigned startOffset = std::max<int>(bounds.x(), 0);
    unsigned endOffset = std::min<int>(bounds.y(), text.length());
    return makeString(text.substring(startOffset, endOffset - startOffset), ", ", startOffset, ", ", endOffset);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int offset)
{
    if (!m_element->interfaces().contains(WebCore::AccessibilityObjectAtspi::Interface::Text))
        return JSStringCreateWithCharacters(nullptr, 0);

    m_element->updateBackingStore();
    auto text = m_element->text();
    if (offset < 0 || offset > static_cast<int>(text.length()))
        return JSStringCreateWithCharacters(nullptr, 0);

    auto string = makeString(text.substring(offset, 1), ", ", offset, ", ", offset + 1);
    return OpaqueJSString::tryCreate(string).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::WordStart)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::LineStart)).leakRef();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int offset)
{
    return OpaqueJSString::tryCreate(stringAtOffset(m_element.get(), offset, WebCore::AccessibilityObjectAtspi::TextGranularity::SentenceStart)).leakRef();
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef, int, int)
{
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    return nullptr;
}

bool AccessibilityUIElement::isInsertion() const
{
    return false;
}

bool AccessibilityUIElement::isDeletion() const
{
    return false;
}

bool AccessibilityUIElement::isFirstItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElement::isLastItemInSuggestion() const
{
    return false;
}

} // namespace WTR

#endif // USE(ATSPI)
