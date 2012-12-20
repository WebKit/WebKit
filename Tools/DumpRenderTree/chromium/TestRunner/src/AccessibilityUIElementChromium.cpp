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
#include "AccessibilityUIElementChromium.h"

#include "WebAccessibilityObject.h"
#include "platform/WebCString.h"
#include "platform/WebPoint.h"
#include "platform/WebRect.h"
#include "platform/WebString.h"
#include <wtf/Assertions.h>
#include <wtf/StringExtras.h>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
string roleToString(WebAccessibilityRole role)
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
    case WebAccessibilityRoleToggleButton:
        return result.append("ToggleButton");
    case WebAccessibilityRoleCanvas:
        return result.append("Canvas");
    case WebAccessibilityRoleParagraph:
        return result.append("Paragraph");
    case WebAccessibilityRoleDiv:
        return result.append("Div");
    case WebAccessibilityRoleLabel:
        return result.append("Label");
    case WebAccessibilityRoleForm:
        return result.append("Form");
    case WebAccessibilityRoleHorizontalRule:
        return result.append("HorizontalRule");
    case WebAccessibilityRoleLegend:
        return result.append("Legend");
    case WebAccessibilityRoleApplicationAlert:
        return result.append("Alert");
    case WebAccessibilityRoleApplicationAlertDialog:
        return result.append("AlertDialog");
    case WebAccessibilityRoleApplicationDialog:
        return result.append("ApplicationDialog");
    case WebAccessibilityRoleDirectory:
        return result.append("Directory");
    case WebAccessibilityRoleDocumentMath:
        return result.append("Math");
    case WebAccessibilityRoleEditableText:
        return result.append("EditableText");
    case WebAccessibilityRoleFooter:
        return result.append("Footer");
    case WebAccessibilityRoleIgnored:
        return result.append("Ignored");
    case WebAccessibilityRoleListItem:
        return result.append("ListItem");
    case WebAccessibilityRoleMenuListPopup:
        return result.append("MenuListPopup");
    case WebAccessibilityRoleMenuListOption:
        return result.append("MenuListOption");
    case WebAccessibilityRolePresentational:
        return result.append("Presentational");
    case WebAccessibilityRoleSpinButton:
        return result.append("SpinButton");
    case WebAccessibilityRoleSpinButtonPart:
        return result.append("SpinButtonPart");
    case WebAccessibilityRoleTabList:
        return result.append("TabList");
    case WebAccessibilityRoleTabPanel:
        return result.append("TabPanel");
    case WebAccessibilityRoleTab:
        return result.append("Tab");
    case WebAccessibilityRoleTreeRole:
        return result.append("Tree");
    case WebAccessibilityRoleTreeGrid:
        return result.append("TreeGrid");
    case WebAccessibilityRoleTreeItemRole:
        return result.append("TreeItem");
    case WebAccessibilityRoleUnknown:
    default:
        return result.append("Unknown");
    }
}

string getDescription(const WebAccessibilityObject& object)
{
    string description = object.accessibilityDescription().utf8();
    return description.insert(0, "AXDescription: ");
}

string getHelpText(const WebAccessibilityObject& object)
{
    string helpText = object.helpText().utf8();
    return helpText.insert(0, "AXHelp: ");
}

string getStringValue(const WebAccessibilityObject& object)
{
    string value = object.stringValue().utf8();
    return value.insert(0, "AXValue: ");
}

string getRole(const WebAccessibilityObject& object)
{
    string roleString = roleToString(object.roleValue());

    // Special-case canvas with fallback content because Chromium wants to
    // treat this as essentially a separate role that it can map differently depending
    // on the platform.
    if (object.roleValue() == WebAccessibilityRoleCanvas && object.canvasHasFallbackContent())
        roleString += "WithFallbackContent";

    return roleString;
}

string getTitle(const WebAccessibilityObject& object)
{
    string title = object.title().utf8();
    return title.insert(0, "AXTitle: ");
}

string getOrientation(const WebAccessibilityObject& object)
{
    if (object.isVertical())
        return "AXOrientation: AXVerticalOrientation";

    return "AXOrientation: AXHorizontalOrientation";
}

string getValueDescription(const WebAccessibilityObject& object)
{
    string valueDescription = object.valueDescription().utf8();
    return valueDescription.insert(0, "AXValueDescription: ");
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

}

AccessibilityUIElement::AccessibilityUIElement(const WebAccessibilityObject& object, Factory* factory)
    : m_accessibilityObject(object)
    , m_factory(factory)
{

    ASSERT(factory);

    //
    // Properties
    //

    bindProperty("role", &AccessibilityUIElement::roleGetterCallback);
    bindProperty("title", &AccessibilityUIElement::titleGetterCallback);
    bindProperty("description", &AccessibilityUIElement::descriptionGetterCallback);
    bindProperty("helpText", &AccessibilityUIElement::helpTextGetterCallback);
    bindProperty("stringValue", &AccessibilityUIElement::stringValueGetterCallback);
    bindProperty("x", &AccessibilityUIElement::xGetterCallback);
    bindProperty("y", &AccessibilityUIElement::yGetterCallback);
    bindProperty("width", &AccessibilityUIElement::widthGetterCallback);
    bindProperty("height", &AccessibilityUIElement::heightGetterCallback);
    bindProperty("intValue", &AccessibilityUIElement::intValueGetterCallback);
    bindProperty("minValue", &AccessibilityUIElement::minValueGetterCallback);
    bindProperty("maxValue", &AccessibilityUIElement::maxValueGetterCallback);
    bindProperty("valueDescription", &AccessibilityUIElement::valueDescriptionGetterCallback);
    bindProperty("childrenCount", &AccessibilityUIElement::childrenCountGetterCallback);
    bindProperty("insertionPointLineNumber", &AccessibilityUIElement::insertionPointLineNumberGetterCallback);
    bindProperty("selectedTextRange", &AccessibilityUIElement::selectedTextRangeGetterCallback);
    bindProperty("isEnabled", &AccessibilityUIElement::isEnabledGetterCallback);
    bindProperty("isRequired", &AccessibilityUIElement::isRequiredGetterCallback);
    bindProperty("isFocused", &AccessibilityUIElement::isFocusedGetterCallback);
    bindProperty("isFocusable", &AccessibilityUIElement::isFocusableGetterCallback);
    bindProperty("isSelected", &AccessibilityUIElement::isSelectedGetterCallback);
    bindProperty("isSelectable", &AccessibilityUIElement::isSelectableGetterCallback);
    bindProperty("isMultiSelectable", &AccessibilityUIElement::isMultiSelectableGetterCallback);
    bindProperty("isSelectedOptionActive", &AccessibilityUIElement::isSelectedOptionActiveGetterCallback);
    bindProperty("isExpanded", &AccessibilityUIElement::isExpandedGetterCallback);
    bindProperty("isChecked", &AccessibilityUIElement::isCheckedGetterCallback);
    bindProperty("isVisible", &AccessibilityUIElement::isVisibleGetterCallback);
    bindProperty("isOffScreen", &AccessibilityUIElement::isOffScreenGetterCallback);
    bindProperty("isCollapsed", &AccessibilityUIElement::isCollapsedGetterCallback);
    bindProperty("hasPopup", &AccessibilityUIElement::hasPopupGetterCallback);
    bindProperty("isValid", &AccessibilityUIElement::isValidGetterCallback);
    bindProperty("isReadOnly", &AccessibilityUIElement::isReadOnlyGetterCallback);
    bindProperty("orientation", &AccessibilityUIElement::orientationGetterCallback);
    bindProperty("clickPointX", &AccessibilityUIElement::clickPointXGetterCallback);
    bindProperty("clickPointY", &AccessibilityUIElement::clickPointYGetterCallback);

    //
    // Methods
    //

    bindMethod("allAttributes", &AccessibilityUIElement::allAttributesCallback);
    bindMethod("attributesOfLinkedUIElements", &AccessibilityUIElement::attributesOfLinkedUIElementsCallback);
    bindMethod("attributesOfDocumentLinks", &AccessibilityUIElement::attributesOfDocumentLinksCallback);
    bindMethod("attributesOfChildren", &AccessibilityUIElement::attributesOfChildrenCallback);
    bindMethod("lineForIndex", &AccessibilityUIElement::lineForIndexCallback);
    bindMethod("boundsForRange", &AccessibilityUIElement::boundsForRangeCallback);
    bindMethod("stringForRange", &AccessibilityUIElement::stringForRangeCallback);
    bindMethod("childAtIndex", &AccessibilityUIElement::childAtIndexCallback);
    bindMethod("elementAtPoint", &AccessibilityUIElement::elementAtPointCallback);
    bindMethod("attributesOfColumnHeaders", &AccessibilityUIElement::attributesOfColumnHeadersCallback);
    bindMethod("attributesOfRowHeaders", &AccessibilityUIElement::attributesOfRowHeadersCallback);
    bindMethod("attributesOfColumns", &AccessibilityUIElement::attributesOfColumnsCallback);
    bindMethod("attributesOfRows", &AccessibilityUIElement::attributesOfRowsCallback);
    bindMethod("attributesOfVisibleCells", &AccessibilityUIElement::attributesOfVisibleCellsCallback);
    bindMethod("attributesOfHeader", &AccessibilityUIElement::attributesOfHeaderCallback);
    bindMethod("indexInTable", &AccessibilityUIElement::indexInTableCallback);
    bindMethod("rowIndexRange", &AccessibilityUIElement::rowIndexRangeCallback);
    bindMethod("columnIndexRange", &AccessibilityUIElement::columnIndexRangeCallback);
    bindMethod("cellForColumnAndRow", &AccessibilityUIElement::cellForColumnAndRowCallback);
    bindMethod("titleUIElement", &AccessibilityUIElement::titleUIElementCallback);
    bindMethod("setSelectedTextRange", &AccessibilityUIElement::setSelectedTextRangeCallback);
    bindMethod("attributeValue", &AccessibilityUIElement::attributeValueCallback);
    bindMethod("isAttributeSettable", &AccessibilityUIElement::isAttributeSettableCallback);
    bindMethod("isPressActionSupported", &AccessibilityUIElement::isPressActionSupportedCallback);
    bindMethod("isIncrementActionSupported", &AccessibilityUIElement::isIncrementActionSupportedCallback);
    bindMethod("isDecrementActionSupported", &AccessibilityUIElement::isDecrementActionSupportedCallback);
    bindMethod("parentElement", &AccessibilityUIElement::parentElementCallback);
    bindMethod("increment", &AccessibilityUIElement::incrementCallback);
    bindMethod("decrement", &AccessibilityUIElement::decrementCallback);
    bindMethod("showMenu", &AccessibilityUIElement::showMenuCallback);
    bindMethod("press", &AccessibilityUIElement::pressCallback);
    bindMethod("isEqual", &AccessibilityUIElement::isEqualCallback);
    bindMethod("addNotificationListener", &AccessibilityUIElement::addNotificationListenerCallback);
    bindMethod("removeNotificationListener", &AccessibilityUIElement::removeNotificationListenerCallback);
    bindMethod("takeFocus", &AccessibilityUIElement::takeFocusCallback);
    bindMethod("scrollToMakeVisible", &AccessibilityUIElement::scrollToMakeVisibleCallback);
    bindMethod("scrollToMakeVisibleWithSubFocus", &AccessibilityUIElement::scrollToMakeVisibleWithSubFocusCallback);
    bindMethod("scrollToGlobalPoint", &AccessibilityUIElement::scrollToGlobalPointCallback);

    bindFallbackMethod(&AccessibilityUIElement::fallbackCallback);
}

AccessibilityUIElement* AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    return m_factory->getOrCreate(accessibilityObject().childAt(index));
}

bool AccessibilityUIElement::isEqual(const WebKit::WebAccessibilityObject& other)
{
    return accessibilityObject().equals(other);
}

void AccessibilityUIElement::notificationReceived(const char* notificationName)
{
    size_t callbackCount = m_notificationCallbacks.size();
    for (size_t i = 0; i < callbackCount; i++) {
        CppVariant notificationNameArgument;
        notificationNameArgument.set(notificationName);
        CppVariant invokeResult;
        m_notificationCallbacks[i].invokeDefault(&notificationNameArgument, 1, invokeResult);
    }
}

//
// Properties
//

void AccessibilityUIElement::roleGetterCallback(CppVariant* result)
{
    result->set(getRole(accessibilityObject()));
}

void AccessibilityUIElement::titleGetterCallback(CppVariant* result)
{
    result->set(getTitle(accessibilityObject()));
}

void AccessibilityUIElement::descriptionGetterCallback(CppVariant* result)
{
    result->set(getDescription(accessibilityObject()));
}

void AccessibilityUIElement::helpTextGetterCallback(CppVariant* result)
{
    result->set(getHelpText(accessibilityObject()));
}

void AccessibilityUIElement::stringValueGetterCallback(CppVariant* result)
{
    result->set(getStringValue(accessibilityObject()));
}

void AccessibilityUIElement::xGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().x);
}

void AccessibilityUIElement::yGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().y);
}

void AccessibilityUIElement::widthGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().width);
}

void AccessibilityUIElement::heightGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().boundingBoxRect().height);
}

void AccessibilityUIElement::intValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().valueForRange());
}

void AccessibilityUIElement::minValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().minValueForRange());
}

void AccessibilityUIElement::maxValueGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().maxValueForRange());
}

void AccessibilityUIElement::valueDescriptionGetterCallback(CppVariant* result)
{
    result->set(getValueDescription(accessibilityObject()));
}

void AccessibilityUIElement::childrenCountGetterCallback(CppVariant* result)
{
    int count = 1; // Root object always has only one child, the WebView.
    if (!isRoot())
        count = accessibilityObject().childCount();
    result->set(count);
}

void AccessibilityUIElement::insertionPointLineNumberGetterCallback(CppVariant* result)
{
    if (!accessibilityObject().isFocused()) {
        result->set(-1);
        return;
    }

    int lineNumber = accessibilityObject().selectionEndLineNumber();
    result->set(lineNumber);
}

void AccessibilityUIElement::selectedTextRangeGetterCallback(CppVariant* result)
{
    unsigned selectionStart = accessibilityObject().selectionStart();
    unsigned selectionEnd = accessibilityObject().selectionEnd();
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "{%d, %d}", selectionStart, selectionEnd - selectionStart);

    result->set(std::string(buffer));
}

void AccessibilityUIElement::isEnabledGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isEnabled());
}

void AccessibilityUIElement::isRequiredGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isRequired());
}

void AccessibilityUIElement::isFocusedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isFocused());
}

void AccessibilityUIElement::isFocusableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetFocusAttribute());
}

void AccessibilityUIElement::isSelectedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelected());
}

void AccessibilityUIElement::isSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().canSetSelectedAttribute());
}

void AccessibilityUIElement::isMultiSelectableGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isMultiSelectable());
}

void AccessibilityUIElement::isSelectedOptionActiveGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isSelectedOptionActive());
}

void AccessibilityUIElement::isExpandedGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isCollapsed());
}

void AccessibilityUIElement::isCheckedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isChecked());
}

void AccessibilityUIElement::isVisibleGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isVisible());
}

void AccessibilityUIElement::isOffScreenGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isOffScreen());
}

void AccessibilityUIElement::isCollapsedGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isCollapsed());
}

void AccessibilityUIElement::hasPopupGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().ariaHasPopup());
}

void AccessibilityUIElement::isValidGetterCallback(CppVariant* result)
{
    result->set(!accessibilityObject().isDetached());
}

void AccessibilityUIElement::isReadOnlyGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().isReadOnly());
}

void AccessibilityUIElement::orientationGetterCallback(CppVariant* result)
{
    result->set(getOrientation(accessibilityObject()));
}

void AccessibilityUIElement::clickPointXGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().x);
}

void AccessibilityUIElement::clickPointYGetterCallback(CppVariant* result)
{
    result->set(accessibilityObject().clickPoint().y);
}

//
// Methods
//

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

void AccessibilityUIElement::lineForIndexCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (!arguments.size() || !arguments[0].isNumber()) {
        result->setNull();
        return;
    }

    int index = arguments[0].toInt32();

    WebVector<int> lineBreaks;
    accessibilityObject().lineBreaks(lineBreaks);
    int line = 0;
    int vectorSize = static_cast<int>(lineBreaks.size());
    while (line < vectorSize && lineBreaks[line] <= index)
        line++;
    result->set(line);
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

void AccessibilityUIElement::elementAtPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    WebPoint point(x, y);
    WebAccessibilityObject obj = accessibilityObject().hitTest(point);
    if (obj.isNull())
        return;

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
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
    WebAccessibilityObject obj = accessibilityObject().titleUIElement();
    if (obj.isNull()) {
        result->setNull();
        return;
    }

    result->set(*(m_factory->getOrCreate(obj)->getAsCppVariant()));
}

void AccessibilityUIElement::setSelectedTextRangeCallback(const CppArgumentList&arguments, CppVariant* result)
{
    result->setNull();
    if (arguments.size() != 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    int selectionStart = arguments[0].toInt32();
    int selectionEnd = selectionStart + arguments[1].toInt32();
    accessibilityObject().setSelectedTextRange(selectionStart, selectionEnd);
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

void AccessibilityUIElement::isPressActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canPress());
}

void AccessibilityUIElement::isIncrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canIncrement());
}

void AccessibilityUIElement::isDecrementActionSupportedCallback(const CppArgumentList&, CppVariant* result)
{
    result->set(accessibilityObject().canDecrement());
}

void AccessibilityUIElement::parentElementCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::incrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().increment();
    result->setNull();
}

void AccessibilityUIElement::decrementCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().decrement();
    result->setNull();
}

void AccessibilityUIElement::showMenuCallback(const CppArgumentList&, CppVariant* result)
{
    result->setNull();
}

void AccessibilityUIElement::pressCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().press();
    result->setNull();
}

void AccessibilityUIElement::isEqualCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    result->set(arguments[0].isEqual(*getAsCppVariant()));
}

void AccessibilityUIElement::addNotificationListenerCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    m_notificationCallbacks.push_back(arguments[0]);
    result->setNull();
}

void AccessibilityUIElement::removeNotificationListenerCallback(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void AccessibilityUIElement::takeFocusCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().setFocused(true);
    result->setNull();
}

void AccessibilityUIElement::scrollToMakeVisibleCallback(const CppArgumentList&, CppVariant* result)
{
    accessibilityObject().scrollToMakeVisible();
    result->setNull();
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocusCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 4
        || !arguments[0].isNumber()
        || !arguments[1].isNumber()
        || !arguments[2].isNumber()
        || !arguments[3].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();
    int width = arguments[2].toInt32();
    int height = arguments[3].toInt32();
    accessibilityObject().scrollToMakeVisibleWithSubFocus(WebRect(x, y, width, height));
    result->setNull();
}

void AccessibilityUIElement::scrollToGlobalPointCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() != 2
        || !arguments[0].isNumber()
        || !arguments[1].isNumber())
        return;

    int x = arguments[0].toInt32();
    int y = arguments[1].toInt32();

    accessibilityObject().scrollToGlobalPoint(WebPoint(x, y));
    result->setNull();
}

void AccessibilityUIElement::fallbackCallback(const CppArgumentList &, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

RootAccessibilityUIElement::RootAccessibilityUIElement(const WebAccessibilityObject &object, Factory *factory)
    : AccessibilityUIElement(object, factory) { }

AccessibilityUIElement* RootAccessibilityUIElement::getChildAtIndex(unsigned index)
{
    if (index)
        return 0;

    return factory()->getOrCreate(accessibilityObject());
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

AccessibilityUIElement* AccessibilityUIElementList::getOrCreate(const WebAccessibilityObject& object)
{
    if (object.isNull())
        return 0;

    size_t elementCount = m_elements.size();
    for (size_t i = 0; i < elementCount; i++) {
        if (m_elements[i]->isEqual(object))
            return m_elements[i];
    }

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

}
