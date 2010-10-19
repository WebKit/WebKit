/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityUIElement.h"

#include "WebAccessibilityObject.h"
#include "WebCString.h"
#include "WebString.h"
#include <wtf/Assertions.h>

using namespace WebKit;
using namespace std;

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
static string roleToString(WebAccessibilityRole role)
{
    string result = "AXRole: AX";
    switch (role) {
    case WebAccessibilityRoleButton:
        return result.append("Button");
    case WebAccessibilityRoleRadioButton:
        return result.append("RadioButton");
    case WebAccessibilityRoleCheckBox:
        return result.append("CheckBox");
    case WebAccessibilityRoleSlider:
        return result.append("Slider");
    case WebAccessibilityRoleTabGroup:
        return result.append("TabGroup");
    case WebAccessibilityRoleTextField:
        return result.append("TextField");
    case WebAccessibilityRoleStaticText:
        return result.append("StaticText");
    case WebAccessibilityRoleTextArea:
        return result.append("TextArea");
    case WebAccessibilityRoleScrollArea:
        return result.append("ScrollArea");
    case WebAccessibilityRolePopUpButton:
        return result.append("PopUpButton");
    case WebAccessibilityRoleMenuButton:
        return result.append("MenuButton");
    case WebAccessibilityRoleTable:
        return result.append("Table");
    case WebAccessibilityRoleApplication:
        return result.append("Application");
    case WebAccessibilityRoleGroup:
        return result.append("Group");
    case WebAccessibilityRoleRadioGroup:
        return result.append("RadioGroup");
    case WebAccessibilityRoleList:
        return result.append("List");
    case WebAccessibilityRoleScrollBar:
        return result.append("ScrollBar");
    case WebAccessibilityRoleValueIndicator:
        return result.append("ValueIndicator");
    case WebAccessibilityRoleImage:
        return result.append("Image");
    case WebAccessibilityRoleMenuBar:
        return result.append("MenuBar");
    case WebAccessibilityRoleMenu:
        return result.append("Menu");
    case WebAccessibilityRoleMenuItem:
        return result.append("MenuItem");
    case WebAccessibilityRoleColumn:
        return result.append("Column");
    case WebAccessibilityRoleRow:
        return result.append("Row");
    case WebAccessibilityRoleToolbar:
        return result.append("Toolbar");
    case WebAccessibilityRoleBusyIndicator:
        return result.append("BusyIndicator");
    case WebAccessibilityRoleProgressIndicator:
        return result.append("ProgressIndicator");
    case WebAccessibilityRoleWindow:
        return result.append("Window");
    case WebAccessibilityRoleDrawer:
        return result.append("Drawer");
    case WebAccessibilityRoleSystemWide:
        return result.append("SystemWide");
    case WebAccessibilityRoleOutline:
        return result.append("Outline");
    case WebAccessibilityRoleIncrementor:
        return result.append("Incrementor");
    case WebAccessibilityRoleBrowser:
        return result.append("Browser");
    case WebAccessibilityRoleComboBox:
        return result.append("ComboBox");
    case WebAccessibilityRoleSplitGroup:
        return result.append("SplitGroup");
    case WebAccessibilityRoleSplitter:
        return result.append("Splitter");
    case WebAccessibilityRoleColorWell:
        return result.append("ColorWell");
    case WebAccessibilityRoleGrowArea:
        return result.append("GrowArea");
    case WebAccessibilityRoleSheet:
        return result.append("Sheet");
    case WebAccessibilityRoleHelpTag:
        return result.append("HelpTag");
    case WebAccessibilityRoleMatte:
        return result.append("Matte");
    case WebAccessibilityRoleRuler:
        return result.append("Ruler");
    case WebAccessibilityRoleRulerMarker:
        return result.append("RulerMarker");
    case WebAccessibilityRoleLink:
        return result.append("Link");
    case WebAccessibilityRoleDisclosureTriangle:
        return result.append("DisclosureTriangle");
    case WebAccessibilityRoleGrid:
        return result.append("Grid");
    case WebAccessibilityRoleCell:
        return result.append("Cell");
    case WebAccessibilityRoleColumnHeader:
        return result.append("ColumnHeader");
    case WebAccessibilityRoleRowHeader:
        return result.append("RowHeader");
    case WebAccessibilityRoleWebCoreLink:
        // Maps to Link role.
        return result.append("Link");
    case WebAccessibilityRoleImageMapLink:
        return result.append("ImageMapLink");
    case WebAccessibilityRoleImageMap:
        return result.append("ImageMap");
    case WebAccessibilityRoleListMarker:
        return result.append("ListMarker");
    case WebAccessibilityRoleWebArea:
        return result.append("WebArea");
    case WebAccessibilityRoleHeading:
        return result.append("Heading");
    case WebAccessibilityRoleListBox:
        return result.append("ListBox");
    case WebAccessibilityRoleListBoxOption:
        return result.append("ListBoxOption");
    case WebAccessibilityRoleTableHeaderContainer:
        return result.append("TableHeaderContainer");
    case WebAccessibilityRoleDefinitionListTerm:
        return result.append("DefinitionListTerm");
    case WebAccessibilityRoleDefinitionListDefinition:
        return result.append("DefinitionListDefinition");
    case WebAccessibilityRoleAnnotation:
        return result.append("Annotation");
    case WebAccessibilityRoleSliderThumb:
        return result.append("SliderThumb");
    case WebAccessibilityRoleLandmarkApplication:
        return result.append("LandmarkApplication");
    case WebAccessibilityRoleLandmarkBanner:
        return result.append("LandmarkBanner");
    case WebAccessibilityRoleLandmarkComplementary:
        return result.append("LandmarkComplementary");
    case WebAccessibilityRoleLandmarkContentInfo:
        return result.append("LandmarkContentInfo");
    case WebAccessibilityRoleLandmarkMain:
        return result.append("LandmarkMain");
    case WebAccessibilityRoleLandmarkNavigation:
        return result.append("LandmarkNavigation");
    case WebAccessibilityRoleLandmarkSearch:
        return result.append("LandmarkSearch");
    case WebAccessibilityRoleApplicationLog:
        return result.append("ApplicationLog");
    case WebAccessibilityRoleApplicationMarquee:
        return result.append("ApplicationMarquee");
    case WebAccessibilityRoleApplicationStatus:
        return result.append("ApplicationStatus");
    case WebAccessibilityRoleApplicationTimer:
        return result.append("ApplicationTimer");
    case WebAccessibilityRoleDocument:
        return result.append("Document");
    case WebAccessibilityRoleDocumentArticle:
        return result.append("DocumentArticle");
    case WebAccessibilityRoleDocumentNote:
        return result.append("DocumentNote");
    case WebAccessibilityRoleDocumentRegion:
        return result.append("DocumentRegion");
    case WebAccessibilityRoleUserInterfaceTooltip:
        return result.append("UserInterfaceTooltip");
    default:
        // Also matches WebAccessibilityRoleUnknown.
        return result.append("Unknown");
    }
}

string getDescription(const WebAccessibilityObject& object)
{
    string description = object.accessibilityDescription().utf8();
    return description.insert(0, "AXDescription: ");
}

string getRole(const WebAccessibilityObject& object)
{
    return roleToString(object.roleValue());
}

string getTitle(const WebAccessibilityObject& object)
{
    string title = object.title().utf8();
    return title.insert(0, "AXTitle: ");
}

string getAttributes(const WebAccessibilityObject& object)
{
    // FIXME: Concatenate all attributes of the AccessibilityObject.
    string attributes(getTitle(object));
    attributes.append("\n");
    attributes.append(getRole(object));
    attributes.append("\n");
    attributes.append(getDescription(object));
    return attributes;
}


// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
public:
    void collectAttributes(const WebAccessibilityObject& object)
    {
        m_attributes.append("\n------------\n");
        m_attributes.append(getAttributes(object));
    }

    string attributes() const { return m_attributes; }

private:
    string m_attributes;
};

AccessibilityUIElement::AccessibilityUIElement(const WebAccessibilityObject& object, Factory* factory)
    : m_accessibilityObject(object)
    , m_factory(factory)
{

    ASSERT(factory);

    bindMethod("allAttributes", &AccessibilityUIElement::allAttributesCallback);
    bindMethod("attributesOfLinkedUIElements",
               &AccessibilityUIElement::attributesOfLinkedUIElementsCallback);
    bindMethod("attributesOfDocumentLinks",
               &AccessibilityUIElement::attributesOfDocumentLinksCallback);
    bindMethod("attributesOfChildren",
               &AccessibilityUIElement::attributesOfChildrenCallback);
    bindMethod("parameterizedAttributeNames",
               &AccessibilityUIElement::parametrizedAttributeNamesCallback);
    bindMethod("lineForIndex", &AccessibilityUIElement::lineForIndexCallback);
    bindMethod("boundsForRange", &AccessibilityUIElement::boundsForRangeCallback);
    bindMethod("stringForRange", &AccessibilityUIElement::stringForRangeCallback);
    bindMethod("childAtIndex", &AccessibilityUIElement::childAtIndexCallback);
    bindMethod("elementAtPoint", &AccessibilityUIElement::elementAtPointCallback);
    bindMethod("attributesOfColumnHeaders",
               &AccessibilityUIElement::attributesOfColumnHeadersCallback);
    bindMethod("attributesOfRowHeaders",
               &AccessibilityUIElement::attributesOfRowHeadersCallback);
    bindMethod("attributesOfColumns",
               &AccessibilityUIElement::attributesOfColumnsCallback);
    bindMethod("attributesOfRows",
               &AccessibilityUIElement::attributesOfRowsCallback);
    bindMethod("attributesOfVisibleCells",
               &AccessibilityUIElement::attributesOfVisibleCellsCallback);
    bindMethod("attributesOfHeader",
               &AccessibilityUIElement::attributesOfHeaderCallback);
    bindMethod("indexInTable", &AccessibilityUIElement::indexInTableCallback);
    bindMethod("rowIndexRange", &AccessibilityUIElement::rowIndexRangeCallback);
    bindMethod("columnIndexRange",
               &AccessibilityUIElement::columnIndexRangeCallback);
    bindMethod("cellForColumnAndRow",
               &AccessibilityUIElement::cellForColumnAndRowCallback);
    bindMethod("titleUIElement", &AccessibilityUIElement::titleUIElementCallback);
    bindMethod("setSelectedTextRange",
               &AccessibilityUIElement::setSelectedTextRangeCallback);
    bindMethod("attributeValue", &AccessibilityUIElement::attributeValueCallback);
    bindMethod("isAttributeSettable",
               &AccessibilityUIElement::isAttributeSettableCallback);
    bindMethod("isActionSupported",
               &AccessibilityUIElement::isActionSupportedCallback);
    bindMethod("parentElement", &AccessibilityUIElement::parentElementCallback);
    bindMethod("increment", &AccessibilityUIElement::incrementCallback);
    bindMethod("decrement", &AccessibilityUIElement::decrementCallback);

    bindProperty("role", &AccessibilityUIElement::roleGetterCallback);
    bindProperty("subrole", &m_subrole);
    bindProperty("title", &AccessibilityUIElement::titleGetterCallback);
    bindProperty("description",
                 &AccessibilityUIElement::descriptionGetterCallback);
    bindProperty("language", &m_language);
    bindProperty("x", &m_x);
    bindProperty("y", &m_y);
    bindProperty("width", &m_width);
    bindProperty("height", &m_height);
    bindProperty("clickPointX", &m_clickPointX);
    bindProperty("clickPointY", &m_clickPointY);
    bindProperty("intValue", &m_intValue);
    bindProperty("minValue", &m_minValue);
    bindProperty("maxValue", &m_maxValue);
    bindProperty("childrenCount",
                 &AccessibilityUIElement::childrenCountGetterCallback);
    bindProperty("insertionPointLineNumber", &m_insertionPointLineNumber);
    bindProperty("selectedTextRange", &m_selectedTextRange);
    bindProperty("isEnabled", &AccessibilityUIElement::isEnabledGetterCallback);
    bindProperty("isRequired", &m_isRequired);
    bindProperty("isSelected", &AccessibilityUIElement::isSelectedGetterCallback);
    bindProperty("valueDescription", &m_valueDescription);

    bindFallbackMethod(&AccessibilityUIElement::fallbackCallback);
}

AccessibilityUIElement* AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    return m_factory->create(accessibilityObject().childAt(index));
}

void AccessibilityUIElement::allAttributesCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(getAttributes(accessibilityObject()));
}

void AccessibilityUIElement::attributesOfLinkedUIElementsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfDocumentLinksCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfChildrenCallback(const CppArgumentList& arguments, CppVariant* result)
{
    AttributesCollector collector;
    unsigned size = accessibilityObject().childCount();
    for (unsigned i = 0; i < size; ++i)
        collector.collectAttributes(accessibilityObject().childAt(i));
    result->set(collector.attributes());
}

void AccessibilityUIElement::parametrizedAttributeNamesCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::lineForIndexCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::boundsForRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::stringForRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::childAtIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    AccessibilityUIElement* child = getChildAtIndex(arguments[0].toInt32());
    if (!child) {
        result->setNull();
        return;
    }

    result->set(*(child->getAsCppVariant()));
}

void AccessibilityUIElement::elementAtPointCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfColumnHeadersCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfRowHeadersCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfColumnsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfRowsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfVisibleCellsCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributesOfHeaderCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::indexInTableCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::rowIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::columnIndexRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::cellForColumnAndRowCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::titleUIElementCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::setSelectedTextRangeCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::attributeValueCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::isAttributeSettableCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 && !arguments[0].isString()) {
        result->setNull();
        return;
    }

    string attribute = arguments[0].toString();
    bool settable = false;
    if (attribute == "AXValue")
        settable = accessibilityObject().canSetValueAttribute();
    result->set(settable);
}

void AccessibilityUIElement::isActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    // This one may be really hard to implement.
    // Not exposed by AccessibilityObject.
    result->setNull();
}

void AccessibilityUIElement::parentElementCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::incrementCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::decrementCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::fallbackCallback(const CppArgumentList &, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void AccessibilityUIElement::childrenCountGetterCallback(CppVariant* result)
{
    int count = 1; // Root object always has only one child, the WebView.
    if (!isRoot())
        count = accessibilityObject().childCount();
    result->set(count);
}

void AccessibilityUIElement::descriptionGetterCallback(CppVariant* result)
{
    result->set(getDescription(accessibilityObject()));
}

void AccessibilityUIElement::isEnabledGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isEnabled());
}

void AccessibilityUIElement::isSelectedGetterCallback(CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::roleGetterCallback(CppVariant* result)
{
    result->set(getRole(accessibilityObject()));
}

void AccessibilityUIElement::titleGetterCallback(CppVariant* result)
{
    result->set(getTitle(accessibilityObject()));
}


RootAccessibilityUIElement::RootAccessibilityUIElement(const WebAccessibilityObject &object, Factory *factory)
    : AccessibilityUIElement(object, factory) { }

AccessibilityUIElement* RootAccessibilityUIElement::getChildAtIndex(unsigned index)
{
    if (index)
        return 0;

    return factory()->create(accessibilityObject());
}


AccessibilityUIElementList ::~AccessibilityUIElementList()
{
    clear();
}

void AccessibilityUIElementList::clear()
{
    for (ElementList::iterator i = m_elements.begin(); i != m_elements.end(); ++i)
        delete (*i);
    m_elements.clear();
}

AccessibilityUIElement* AccessibilityUIElementList::create(const WebAccessibilityObject& object)
{
    if (object.isNull())
        return 0;

    AccessibilityUIElement* element = new AccessibilityUIElement(object, this);
    m_elements.append(element);
    return element;
}

AccessibilityUIElement* AccessibilityUIElementList::createRoot(const WebAccessibilityObject& object)
{
    AccessibilityUIElement* element = new RootAccessibilityUIElement(object, this);
    m_elements.append(element);
    return element;
}
