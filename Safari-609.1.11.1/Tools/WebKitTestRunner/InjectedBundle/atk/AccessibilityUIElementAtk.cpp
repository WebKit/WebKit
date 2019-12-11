/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#if HAVE(ACCESSIBILITY)

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/NotImplemented.h>
#include <WebKit/WKBundleFrame.h>
#include <atk/atk.h>
#include <wtf/Assertions.h>
#include <wtf/UniqueArray.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WTR {

namespace {

enum RangeLimit {
    RangeLimitMinimum,
    RangeLimitMaximum
};

enum AtkAttributeType {
    ObjectAttributeType,
    TextAttributeType
};

enum AttributesIndex {
    // Attribute names.
    InvalidNameIndex = 0,
    ColumnCount,
    ColumnIndex,
    ColumnSpan,
    RowCount,
    RowIndex,
    RowSpan,
    PosInSetIndex,
    SetSizeIndex,
    PlaceholderNameIndex,
    SortNameIndex,
    CurrentNameIndex,
    AriaLiveNameIndex,
    AriaAtomicNameIndex,
    AriaRelevantNameIndex,
    BusyNameIndex,

    // Attribute values.
    SortAscendingValueIndex,
    SortDescendingValueIndex,
    SortUnknownValueIndex,

    NumberOfAttributes
};

// Attribute names & Values (keep on sync with enum AttributesIndex).
struct Attribute {
    String coreDomain;
    String atkDomain;
};
using Attributes = std::array<Attribute, NumberOfAttributes>;
static const Attributes& attributesMap()
{
    static NeverDestroyed<Attributes> attributes = Attributes({
        // Attribute names.
        Attribute { "AXInvalid", "invalid" },
        Attribute { "AXARIAColumnCount", "colcount" },
        Attribute { "AXARIAColumnIndex", "colindex" },
        Attribute { "AXARIAColumnSpan", "colspan" },
        Attribute { "AXARIARowCount", "rowcount" },
        Attribute { "AXARIARowIndex", "rowindex" },
        Attribute { "AXARIARowSpan", "rowspan" },
        Attribute { "AXARIAPosInSet", "posinset" },
        Attribute { "AXARIASetSize", "setsize" },
        Attribute { "AXPlaceholderValue", "placeholder-text" } ,
        Attribute { "AXSortDirection", "sort" },
        Attribute { "AXARIACurrent", "current" },
        Attribute { "AXARIALive", "live" },
        Attribute { "AXARIAAtomic", "atomic" },
        Attribute { "AXARIARelevant", "relevant" },
        Attribute { "AXElementBusy", "busy" },

        // Attribute values.
        Attribute { "AXAscendingSortDirection", "ascending" },
        Attribute { "AXDescendingSortDirection", "descending" },
        Attribute { "AXUnknownSortDirection", "unknown" },
    });
    return attributes.get();
}

const char* landmarkStringBanner = "AXLandmarkBanner";
const char* landmarkStringComplementary = "AXLandmarkComplementary";
const char* landmarkStringContentinfo = "AXLandmarkContentInfo";
const char* landmarkStringForm = "AXLandmarkForm";
const char* landmarkStringMain = "AXLandmarkMain";
const char* landmarkStringNavigation = "AXLandmarkNavigation";
const char* landmarkStringRegion = "AXLandmarkRegion";
const char* landmarkStringSearch = "AXLandmarkSearch";

String jsStringToWTFString(JSStringRef attribute)
{
    size_t bufferSize = JSStringGetMaximumUTF8CStringSize(attribute);
    GUniquePtr<gchar> buffer(static_cast<gchar*>(g_malloc(bufferSize)));
    JSStringGetUTF8CString(attribute, buffer.get(), bufferSize);

    return String::fromUTF8(buffer.get());
}

String coreAttributeToAtkAttribute(JSStringRef attribute)
{
    String attributeString = jsStringToWTFString(attribute);
    for (int i = 0; i < NumberOfAttributes; ++i) {
        if (attributesMap()[i].coreDomain == attributeString)
            return attributesMap()[i].atkDomain;
    }

    return attributeString;
}

String atkAttributeValueToCoreAttributeValue(AtkAttributeType type, const String& id, const String& value)
{
    if (type == ObjectAttributeType) {
        // We don't expose the "current" attribute if there is no author-provided value.
        if (id == attributesMap()[CurrentNameIndex].atkDomain && value.isEmpty())
            return "false";

        // We need to translate ATK values exposed for 'aria-sort' (e.g. 'ascending')
        // into those expected by the layout tests (e.g. 'AXAscendingSortDirection').
        if (id == attributesMap()[SortNameIndex].atkDomain && !value.isEmpty()) {
            if (value == attributesMap()[SortAscendingValueIndex].atkDomain)
                return attributesMap()[SortAscendingValueIndex].coreDomain;
            if (value == attributesMap()[SortDescendingValueIndex].atkDomain)
                return attributesMap()[SortDescendingValueIndex].coreDomain;

            return attributesMap()[SortUnknownValueIndex].coreDomain;
        }
    } else if (type == TextAttributeType) {
        // In case of 'aria-invalid' when the attribute empty or has "false" for ATK
        // it should not be mapped at all, but layout tests will expect 'false'.
        if (id == attributesMap()[InvalidNameIndex].atkDomain && value.isEmpty())
            return "false";
    }

    return value;
}

AtkAttributeSet* getAttributeSet(AtkObject* accessible, AtkAttributeType type)
{
    if (!accessible)
        return nullptr;

    if (type == ObjectAttributeType)
        return atk_object_get_attributes(accessible);

    if (type == TextAttributeType) {
        if (!ATK_IS_TEXT(accessible))
            return nullptr;

        return atk_text_get_default_attributes(ATK_TEXT(accessible));
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

String getAttributeSetValueForId(AtkObject* accessible, AtkAttributeType type, String id)
{
    AtkAttributeSet* attributeSet = getAttributeSet(accessible, type);
    if (!attributeSet)
        return String();

    String attributeValue;
    for (AtkAttributeSet* attributes = attributeSet; attributes; attributes = attributes->next) {
        AtkAttribute* atkAttribute = static_cast<AtkAttribute*>(attributes->data);
        if (id == atkAttribute->name) {
            attributeValue = String::fromUTF8(atkAttribute->value);
            break;
        }
    }
    atk_attribute_set_free(attributeSet);

    return atkAttributeValueToCoreAttributeValue(type, id, attributeValue);
}

String attributeSetToString(AtkAttributeSet* attributeSet, String separator=", ")
{
    if (!attributeSet)
        return String();

    StringBuilder builder;
    for (AtkAttributeSet* attributes = attributeSet; attributes; attributes = attributes->next) {
        AtkAttribute* attribute = static_cast<AtkAttribute*>(attributes->data);
        builder.append(attribute->name);
        builder.append(':');
        builder.append(attribute->value);
        if (attributes->next)
            builder.append(separator);
    }
    atk_attribute_set_free(attributeSet);

    return builder.toString();
}

String getAtkAttributeSetAsString(AtkObject* accessible, AtkAttributeType type,  String separator=", ")
{
    return attributeSetToString(getAttributeSet(accessible, type), separator);
}

bool checkElementState(PlatformUIElement element, AtkStateType stateType)
{
    if (!ATK_IS_OBJECT(element.get()))
        return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(element.get())));
    return atk_state_set_contains_state(stateSet.get(), stateType);
}

JSStringRef indexRangeInTable(PlatformUIElement element, bool isRowRange)
{
    GUniquePtr<gchar> rangeString(g_strdup("{0, 0}"));
    if (!ATK_IS_TABLE_CELL(element.get()))
        return JSStringCreateWithUTF8CString(rangeString.get());

    gint row = -1;
    gint column = -1;
    gint rowSpan = -1;
    gint columnSpan = -1;
    atk_table_cell_get_row_column_span(ATK_TABLE_CELL(element.get()), &row, &column, &rowSpan, &columnSpan);

    // Get the actual values, if row and columns are valid values.
    if (row != -1 && column != -1) {
        int base = 0;
        int length = 0;
        if (isRowRange) {
            base = row;
            length = rowSpan;
        } else {
            base = column;
            length = columnSpan;
        }
        rangeString.reset(g_strdup_printf("{%d, %d}", base, length));
    }

    return JSStringCreateWithUTF8CString(rangeString.get());
}

void alterCurrentValue(PlatformUIElement element, int factor)
{
    if (!ATK_IS_VALUE(element.get()))
        return;

    double currentValue;
    atk_value_get_value_and_text(ATK_VALUE(element.get()), &currentValue, nullptr);

    double increment = atk_value_get_increment(ATK_VALUE(element.get()));
    atk_value_set_value(ATK_VALUE(element.get()), currentValue + factor * increment);
}

gchar* replaceCharactersForResults(gchar* str)
{
    WTF::String uString = WTF::String::fromUTF8(str);

    // The object replacement character is passed along to ATs so we need to be
    // able to test for their presence and do so without causing test failures.
    uString.replace(objectReplacementCharacter, "<obj>");

    // The presence of newline characters in accessible text of a single object
    // is appropriate, but it makes test results (especially the accessible tree)
    // harder to read.
    uString.replace("\n", "<\\n>");

    return g_strdup(uString.utf8().data());
}

const gchar* roleToString(AtkObject* object)
{
    AtkRole role = atk_object_get_role(object);

    if (role == ATK_ROLE_LANDMARK) {
        String xmlRolesValue = getAttributeSetValueForId(object, ObjectAttributeType, "xml-roles");
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "banner"))
            return landmarkStringBanner;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "complementary"))
            return landmarkStringComplementary;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "contentinfo"))
            return landmarkStringContentinfo;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-acknowledgments"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-afterword"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-appendix"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-bibliography"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-chapter"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-conclusion"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-credits"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-endnotes"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-epilogue"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-errata"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-foreword"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-glossary"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-glossref"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-index"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-introduction"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-pagelist"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-part"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-preface"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-prologue"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "doc-toc"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "form"))
            return landmarkStringForm;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "main"))
            return landmarkStringMain;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "navigation"))
            return landmarkStringNavigation;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "region"))
            return landmarkStringRegion;
        if (equalLettersIgnoringASCIICase(xmlRolesValue, "search"))
            return landmarkStringSearch;
    }

    switch (role) {
    case ATK_ROLE_ALERT:
        return "AXAlert";
    case ATK_ROLE_DIALOG:
        return "AXDialog";
    case ATK_ROLE_CANVAS:
        return "AXCanvas";
    case ATK_ROLE_CAPTION:
        return "AXCaption";
    case ATK_ROLE_CHECK_BOX:
        return "AXCheckBox";
    case ATK_ROLE_COLOR_CHOOSER:
        return "AXColorWell";
    case ATK_ROLE_COLUMN_HEADER:
        return "AXColumnHeader";
    case ATK_ROLE_COMBO_BOX:
        return "AXComboBox";
    case ATK_ROLE_COMMENT:
        return "AXComment";
    case ATK_ROLE_DOCUMENT_FRAME:
        return "AXDocument";
    case ATK_ROLE_DOCUMENT_WEB:
        return "AXWebArea";
    case ATK_ROLE_EMBEDDED:
        return "AXEmbedded";
    case ATK_ROLE_ENTRY:
        return "AXTextField";
    case ATK_ROLE_FOOTER:
        return "AXFooter";
    case ATK_ROLE_FORM:
        return "AXForm";
    case ATK_ROLE_GROUPING:
        return "AXGroup";
    case ATK_ROLE_HEADING:
        return "AXHeading";
    case ATK_ROLE_IMAGE:
        return "AXImage";
    case ATK_ROLE_IMAGE_MAP:
        return "AXImageMap";
    case ATK_ROLE_INVALID:
        return "AXInvalid";
    case ATK_ROLE_LABEL:
        return "AXLabel";
    case ATK_ROLE_LEVEL_BAR:
        return "AXLevelIndicator";
    case ATK_ROLE_LINK:
        return "AXLink";
    case ATK_ROLE_LIST:
        return "AXList";
    case ATK_ROLE_LIST_BOX:
        return "AXListBox";
    case ATK_ROLE_LIST_ITEM:
        return "AXListItem";
    case ATK_ROLE_MENU:
        return "AXMenu";
    case ATK_ROLE_MENU_BAR:
        return "AXMenuBar";
    case ATK_ROLE_MENU_ITEM:
        return "AXMenuItem";
    case ATK_ROLE_PAGE_TAB:
        return "AXTab";
    case ATK_ROLE_PAGE_TAB_LIST:
        return "AXTabGroup";
    case ATK_ROLE_PANEL:
        return "AXGroup";
    case ATK_ROLE_PARAGRAPH:
        return "AXParagraph";
    case ATK_ROLE_PASSWORD_TEXT:
        return "AXPasswordField";
    case ATK_ROLE_PROGRESS_BAR:
        return "AXProgressIndicator";
    case ATK_ROLE_PUSH_BUTTON:
        return "AXButton";
    case ATK_ROLE_RADIO_BUTTON:
        return "AXRadioButton";
    case ATK_ROLE_RADIO_MENU_ITEM:
        return "AXRadioMenuItem";
    case ATK_ROLE_ROW_HEADER:
        return "AXRowHeader";
    case ATK_ROLE_CHECK_MENU_ITEM:
        return "AXCheckMenuItem";
    case ATK_ROLE_RULER:
        return "AXRuler";
    case ATK_ROLE_SCROLL_BAR:
        return "AXScrollBar";
    case ATK_ROLE_SCROLL_PANE:
        return "AXScrollArea";
    case ATK_ROLE_SECTION:
        return "AXSection";
    case ATK_ROLE_SEPARATOR:
        return "AXSeparator";
    case ATK_ROLE_SLIDER:
        return "AXSlider";
    case ATK_ROLE_SPIN_BUTTON:
        return "AXSpinButton";
    case ATK_ROLE_STATUSBAR:
        return "AXStatusBar";
    case ATK_ROLE_TABLE:
        return "AXTable";
    case ATK_ROLE_TABLE_CELL:
        return "AXCell";
    case ATK_ROLE_TABLE_COLUMN_HEADER:
        return "AXColumnHeader";
    case ATK_ROLE_TABLE_ROW:
        return "AXRow";
    case ATK_ROLE_TABLE_ROW_HEADER:
        return "AXRowHeader";
    case ATK_ROLE_TOGGLE_BUTTON:
        return "AXToggleButton";
    case ATK_ROLE_TOOL_BAR:
        return "AXToolbar";
    case ATK_ROLE_TOOL_TIP:
        return "AXUserInterfaceTooltip";
    case ATK_ROLE_TREE:
        return "AXTree";
    case ATK_ROLE_TREE_TABLE:
        return "AXTreeGrid";
    case ATK_ROLE_TREE_ITEM:
        return "AXTreeItem";
    case ATK_ROLE_WINDOW:
        return "AXWindow";
    case ATK_ROLE_UNKNOWN:
        return "AXUnknown";
    case ATK_ROLE_ARTICLE:
        return "AXArticle";
    case ATK_ROLE_AUDIO:
        return "AXAudio";
    case ATK_ROLE_BLOCK_QUOTE:
        return "AXBlockquote";
    case ATK_ROLE_DEFINITION:
        return "AXDefinition";
    case ATK_ROLE_LOG:
        return "AXLog";
    case ATK_ROLE_MARQUEE:
        return "AXMarquee";
    case ATK_ROLE_MATH:
        return "AXMath";
    case ATK_ROLE_TIMER:
        return "AXTimer";
    case ATK_ROLE_VIDEO:
        return "AXVideo";
    case ATK_ROLE_DESCRIPTION_LIST:
        return "AXDescriptionList";
    case ATK_ROLE_DESCRIPTION_TERM:
        return "AXDescriptionTerm";
    case ATK_ROLE_DESCRIPTION_VALUE:
        return "AXDescriptionValue";
    case ATK_ROLE_STATIC:
        return "AXStatic";
    case ATK_ROLE_MATH_FRACTION:
        return "AXMathFraction";
    case ATK_ROLE_MATH_ROOT:
        return "AXMathRoot";
    case ATK_ROLE_SUBSCRIPT:
        return "AXSubscript";
    case ATK_ROLE_SUPERSCRIPT:
        return "AXSuperscript";
#if ATK_CHECK_VERSION(2, 25, 2)
    case ATK_ROLE_FOOTNOTE:
        return "AXFootnote";
#endif
#if ATK_CHECK_VERSION(2, 33, 3)
    case ATK_ROLE_CONTENT_DELETION:
        return "AXDeletion";
    case ATK_ROLE_CONTENT_INSERTION:
        return "AXInsertion";
#endif
    default:
        // We want to distinguish ATK_ROLE_UNKNOWN from a known AtkRole which
        // our DRT isn't properly handling.
        return "FIXME not identified";
    }
}

String selectedText(AtkObject* accessible)
{
    if (!ATK_IS_TEXT(accessible))
        return String();

    AtkText* text = ATK_TEXT(accessible);

    gint start, end;
    g_free(atk_text_get_selection(text, 0, &start, &end));

    return atk_text_get_text(text, start, end);
}

String attributesOfElement(AccessibilityUIElement* element)
{
    StringBuilder builder;

    builder.append(element->role()->string());
    builder.append('\n');

    // For the parent we print its role and its name, if available.
    builder.appendLiteral("AXParent: ");
    RefPtr<AccessibilityUIElement> parent = element->parentElement();
    AtkObject* atkParent = parent ? parent->platformUIElement().get() : nullptr;
    if (atkParent) {
        builder.append(roleToString(atkParent));
        const char* parentName = atk_object_get_name(atkParent);
        if (parentName && parentName[0]) {
            builder.appendLiteral(": ");
            builder.append(parentName);
        }
    } else
        builder.appendLiteral("(null)");
    builder.append('\n');

    builder.appendLiteral("AXChildren: ");
    builder.appendNumber(element->childrenCount());
    builder.append('\n');

    builder.appendLiteral("AXPosition:  { ");
    builder.appendFixedPrecisionNumber(element->x(), 6, KeepTrailingZeros);
    builder.appendLiteral(", ");
    builder.appendFixedPrecisionNumber(element->y(), 6, KeepTrailingZeros);
    builder.appendLiteral(" }\n");

    builder.appendLiteral("AXSize: { ");
    builder.appendFixedPrecisionNumber(element->width(), 6, KeepTrailingZeros);
    builder.appendLiteral(", ");
    builder.appendFixedPrecisionNumber(element->height(), 6, KeepTrailingZeros);
    builder.appendLiteral(" }\n");

    String title = element->title()->string();
    if (!title.isEmpty()) {
        builder.append(title);
        builder.append('\n');
    }

    String description = element->description()->string();
    if (!description.isEmpty()) {
        builder.append(description.utf8().data());
        builder.append('\n');
    }

    String value = element->stringValue()->string();
    if (!value.isEmpty()) {
        builder.append(value);
        builder.append('\n');
    }

    builder.appendLiteral("AXFocusable: ");
    builder.appendNumber(element->isFocusable());
    builder.append('\n');

    builder.appendLiteral("AXFocused: ");
    builder.appendNumber(element->isFocused());
    builder.append('\n');

    builder.appendLiteral("AXSelectable: ");
    builder.appendNumber(element->isSelectable());
    builder.append('\n');

    builder.appendLiteral("AXSelected: ");
    builder.appendNumber(element->isSelected());
    builder.append('\n');

    builder.appendLiteral("AXMultiSelectable: ");
    builder.appendNumber(element->isMultiSelectable());
    builder.append('\n');

    builder.appendLiteral("AXEnabled: ");
    builder.appendNumber(element->isEnabled());
    builder.append('\n');

    builder.appendLiteral("AXExpanded: ");
    builder.appendNumber(element->isExpanded());
    builder.append('\n');

    builder.appendLiteral("AXRequired: ");
    builder.appendNumber(element->isRequired());
    builder.append('\n');

    builder.appendLiteral("AXChecked: ");
    builder.appendNumber(element->isChecked());
    builder.append('\n');

    String url = element->url()->string();
    if (!url.isEmpty()) {
        builder.append(url);
        builder.append('\n');
    }

    // We append the ATK specific attributes as a single line at the end.
    builder.appendLiteral("AXPlatformAttributes: ");
    builder.append(getAtkAttributeSetAsString(element->platformUIElement().get(), ObjectAttributeType));

    return builder.toString();
}

static JSRetainPtr<JSStringRef> createStringWithAttributes(const Vector<RefPtr<AccessibilityUIElement> >& elements)
{
    StringBuilder builder;

    for (Vector<RefPtr<AccessibilityUIElement> >::const_iterator it = elements.begin(); it != elements.end(); ++it) {
        builder.append(attributesOfElement(const_cast<AccessibilityUIElement*>(it->get())));
        builder.appendLiteral("\n------------\n");
    }

    return JSStringCreateWithUTF8CString(builder.toString().utf8().data());
}

static Vector<RefPtr<AccessibilityUIElement> > getTableRowHeaders(AtkTable* accessible)
{
    Vector<RefPtr<AccessibilityUIElement> > rowHeaders;

    int rowsCount = atk_table_get_n_rows(accessible);
    for (int row = 0; row < rowsCount; ++row) {
        if (AtkObject* header = atk_table_get_row_header(accessible, row))
            rowHeaders.append(AccessibilityUIElement::create(header));
    }

    return rowHeaders;
}

static Vector<RefPtr<AccessibilityUIElement> > getTableColumnHeaders(AtkTable* accessible)
{
    Vector<RefPtr<AccessibilityUIElement> > columnHeaders;

    int columnsCount = atk_table_get_n_columns(accessible);
    for (int column = 0; column < columnsCount; ++column) {
        if (AtkObject* header = atk_table_get_column_header(accessible, column))
            columnHeaders.append(AccessibilityUIElement::create(header));
    }

    return columnHeaders;
}

static Vector<RefPtr<AccessibilityUIElement> > getVisibleCells(AccessibilityUIElement* element)
{
    Vector<RefPtr<AccessibilityUIElement> > visibleCells;

    AtkTable* accessible = ATK_TABLE(element->platformUIElement().get());
    int rowsCount = atk_table_get_n_rows(accessible);
    int columnsCount = atk_table_get_n_columns(accessible);

    for (int row = 0; row < rowsCount; ++row) {
        for (int column = 0; column < columnsCount; ++column)
            visibleCells.append(element->cellForColumnAndRow(column, row));
    }

    return visibleCells;
}

static Vector<RefPtr<AccessibilityUIElement>> convertGPtrArrayToVector(const GPtrArray* array)
{
    Vector<RefPtr<AccessibilityUIElement>> cells;
    for (guint i = 0; i < array->len; i++) {
        if (AtkObject* atkObject = static_cast<AtkObject*>(g_ptr_array_index(array, i)))
            cells.append(AccessibilityUIElement::create(atkObject));
    }
    return cells;
}

static JSValueRef convertToJSObjectArray(const Vector<RefPtr<AccessibilityUIElement>>& children)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    size_t elementCount = children.size();
    auto valueElements = makeUniqueArray<JSValueRef>(elementCount);
    for (size_t i = 0; i < elementCount; i++)
        valueElements[i] = JSObjectMake(context, children[i]->wrapperClass(), children[i].get());

    return JSObjectMakeArray(context, elementCount, valueElements.get(), nullptr);
}

static double rangeMinMaxValue(AtkValue* atkValue, RangeLimit rangeLimit)
{
    AtkRange* range = atk_value_get_range(atkValue);
    if (!range)
        return 0;

    double rangeValue = 0;
    switch (rangeLimit) {
    case RangeLimitMinimum:
        rangeValue = atk_range_get_lower_limit(range);
        break;
    case RangeLimitMaximum:
        rangeValue = atk_range_get_upper_limit(range);
        break;
    };

    atk_range_free(range);
    return rangeValue;
}

} // namespace

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
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
    return m_element == otherElement->platformUIElement();
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& children)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return;

    int count = childrenCount();
    for (int i = 0; i < count; i++) {
        GRefPtr<AtkObject> child = adoptGRef(atk_object_ref_accessible_child(ATK_OBJECT(m_element.get()), i));
        children.append(AccessibilityUIElement::create(child.get()));
    }
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement> >& children, unsigned location, unsigned length)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return;
    unsigned end = location + length;
    for (unsigned i = location; i < end; i++) {
        GRefPtr<AtkObject> child = adoptGRef(atk_object_ref_accessible_child(ATK_OBJECT(m_element.get()), i));
        children.append(AccessibilityUIElement::create(child.get()));
    }
}

int AccessibilityUIElement::childrenCount()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return 0;

    return atk_object_get_n_accessible_children(ATK_OBJECT(m_element.get()));
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int x, int y)
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return nullptr;

    GRefPtr<AtkObject> objectAtPoint = adoptGRef(atk_component_ref_accessible_at_point(ATK_COMPONENT(m_element.get()), x, y, ATK_XY_WINDOW));
    return AccessibilityUIElement::create(objectAtPoint ? objectAtPoint.get() : m_element.get());
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return 0;

    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildren(children);

    unsigned count = children.size();
    for (unsigned i = 0; i < count; i++)
        if (children[i]->isEqual(element))
            return i;

    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned index)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return nullptr;

    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];

    return nullptr;
}

static RefPtr<AccessibilityUIElement> accessibilityElementAtIndex(AtkObject* element, AtkRelationType relationType, unsigned index)
{
    if (!ATK_IS_OBJECT(element))
        return nullptr;

    AtkRelationSet* relationSet = atk_object_ref_relation_set(element);
    if (!relationSet)
        return nullptr;

    AtkRelation* relation = atk_relation_set_get_relation_by_type(relationSet, relationType);
    if (!relation)
        return nullptr;

    GPtrArray* targetList = atk_relation_get_target(relation);
    if (!targetList || !targetList->len || index >= targetList->len)
        return nullptr;

    AtkObject* target = static_cast<AtkObject*>(g_ptr_array_index(targetList, index));
    g_object_unref(relationSet);

    return target ? AccessibilityUIElement::create(target).ptr() : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_NODE_PARENT_OF, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsReferencingElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_NODE_CHILD_OF, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_FLOWS_TO, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToReferencingElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_FLOWS_FROM, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_CONTROLLER_FOR, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsReferencingElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_CONTROLLED_BY, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaLabelledByElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_LABELLED_BY, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaLabelledByReferencingElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_LABEL_FOR, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDescribedByElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_DESCRIBED_BY, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDescribedByReferencingElementAtIndex(unsigned index)
{
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_DESCRIPTION_FOR, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsElementAtIndex(unsigned index)
{
#if ATK_CHECK_VERSION(2, 25, 2)
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_DETAILS, index);
#endif
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsReferencingElementAtIndex(unsigned index)
{
#if ATK_CHECK_VERSION(2, 25, 2)
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_DETAILS_FOR, index);
#endif
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageElementAtIndex(unsigned index)
{
#if ATK_CHECK_VERSION(2, 25, 2)
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_ERROR_MESSAGE, index);
#endif
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageReferencingElementAtIndex(unsigned index)
{
#if ATK_CHECK_VERSION(2, 25, 2)
    return accessibilityElementAtIndex(m_element.get(), ATK_RELATION_ERROR_FOR, index);
#endif
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned index)
{
    // ATK doesn't have API to get an accessible row by index directly. It does, however, have
    // API to get cells in the row specified by index. The parent of a cell should be the row.
    AtkTable* axTable = ATK_TABLE(m_element.get());
    unsigned nColumns = columnCount();
    for (unsigned col = 0; col < nColumns; col++) {
        // Find the first cell in this row that only spans one row.
        if (atk_table_get_row_extent_at(axTable, index, col) == 1) {
            AtkObject* cell = atk_table_ref_at(axTable, index, col);
            return cell ? AccessibilityUIElement::create(atk_object_get_parent(cell)).ptr() : nullptr;
        }
    }

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    if (!ATK_SELECTION(m_element.get()))
        return nullptr;

    GRefPtr<AtkObject> child = adoptGRef(atk_selection_ref_selection(ATK_SELECTION(m_element.get()), index));
    return child ? AccessibilityUIElement::create(child.get()).ptr() : nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    if (!ATK_IS_SELECTION(m_element.get()))
        return 0;
    return atk_selection_get_selection_count(ATK_SELECTION(m_element.get()));
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return nullptr;

    AtkRelationSet* set = atk_object_ref_relation_set(ATK_OBJECT(m_element.get()));
    if (!set)
        return nullptr;

    AtkObject* target = nullptr;
    int count = atk_relation_set_get_n_relations(set);
    for (int i = 0; i < count; i++) {
        AtkRelation* relation = atk_relation_set_get_relation(set, i);
        if (atk_relation_get_relation_type(relation) == ATK_RELATION_LABELLED_BY) {
            GPtrArray* targetList = atk_relation_get_target(relation);
            if (targetList->len)
                target = static_cast<AtkObject*>(g_ptr_array_index(targetList, 0));
        }
    }

    g_object_unref(set);
    return target ? AccessibilityUIElement::create(target).ptr() : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return nullptr;

    AtkObject* parent = atk_object_get_parent(ATK_OBJECT(m_element.get()));
    return parent ? AccessibilityUIElement::create(parent).ptr() : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildren(children);

    return createStringWithAttributes(children);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(attributesOfElement(this).utf8().data());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    String atkAttributeName = coreAttributeToAtkAttribute(attribute);

    // The value of AXSelectedText is not exposed through any AtkAttribute.
    if (atkAttributeName == "AXSelectedText") {
        String string = selectedText(m_element.get());
        return JSStringCreateWithUTF8CString(string.utf8().data());
    }

    // Try object attributes before text attributes.
    String attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, atkAttributeName);

    // Try text attributes if the requested one was not found and we have an AtkText object.
    if (attributeValue.isEmpty() && ATK_IS_TEXT(m_element.get()))
        attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), TextAttributeType, atkAttributeName);

    // Additional check to make sure that the exposure of the state ATK_STATE_INVALID_ENTRY
    // is consistent with the exposure of aria-invalid as a text attribute, if present.
    if (atkAttributeName == attributesMap()[InvalidNameIndex].atkDomain) {
        bool isInvalidState = checkElementState(m_element.get(), ATK_STATE_INVALID_ENTRY);
        if (attributeValue.isEmpty())
            return JSStringCreateWithUTF8CString(isInvalidState ? "true" : "false");

        // If the text attribute was there, check that it's consistent with
        // what the state says or force the test to fail otherwise.
        bool isAriaInvalid = attributeValue != "false";
        if (isInvalidState != isAriaInvalid)
            return JSStringCreateWithCharacters(0, 0);
    }

    return JSStringCreateWithUTF8CString(attributeValue.utf8().data());
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return 0;

    String atkAttributeName = coreAttributeToAtkAttribute(attribute);
    if (atkAttributeName.isEmpty())
        return 0;

    if (atkAttributeName == "setsize" || atkAttributeName == "posinset") {
        String attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, atkAttributeName);
        if (!attributeValue.isEmpty())
            return attributeValue.toDouble();
    }

    if (atkAttributeName.startsWith("row") || atkAttributeName.startsWith("col")) {
        String attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, atkAttributeName);
        if (!attributeValue.isEmpty())
            return attributeValue.toInt();
    }

    return 0;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    // FIXME: implement
    return nullptr;
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    if (ATK_IS_TABLE(m_element.get()))
        return convertToJSObjectArray(getTableRowHeaders(ATK_TABLE(m_element.get())));

    Vector<RefPtr<AccessibilityUIElement>> headers;
    if (!ATK_IS_TABLE_CELL(m_element.get()))
        return convertToJSObjectArray(headers);

    if (GRefPtr<GPtrArray> array = adoptGRef(atk_table_cell_get_row_header_cells(ATK_TABLE_CELL(m_element.get()))))
        headers = convertGPtrArrayToVector(array.get());
    return convertToJSObjectArray(headers);
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    if (ATK_IS_TABLE(m_element.get()))
        return convertToJSObjectArray(getTableColumnHeaders(ATK_TABLE(m_element.get())));

    Vector<RefPtr<AccessibilityUIElement>> headers;
    if (!ATK_IS_TABLE_CELL(m_element.get()))
        return convertToJSObjectArray(headers);

    if (GRefPtr<GPtrArray> array = adoptGRef(atk_table_cell_get_column_header_cells(ATK_TABLE_CELL(m_element.get()))))
        headers = convertGPtrArrayToVector(array.get());
    return convertToJSObjectArray(headers);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return nullptr;

    // ATK does not have this API. So we're "faking it" here on a case-by-case basis.
    String attributeString = jsStringToWTFString(attribute);
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element.get()));
    if (role == ATK_ROLE_SPIN_BUTTON && const_cast<AccessibilityUIElement*>(this)->childrenCount() == 2) {
        if (attributeString == "AXDecrementButton")
            return const_cast<AccessibilityUIElement*>(this)->childAtIndex(0);
        if (attributeString == "AXIncrementButton")
            return const_cast<AccessibilityUIElement*>(this)->childAtIndex(1);
    }

    return nullptr;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return false;

    String attributeString = jsStringToWTFString(attribute);
    if (attributeString == "AXElementBusy")
        return checkElementState(m_element.get(), ATK_STATE_BUSY);
    if (attributeString == "AXChecked")
        return checkElementState(m_element.get(), ATK_STATE_CHECKED);
    if (attributeString == "AXEnabled")
        return checkElementState(m_element.get(), ATK_STATE_ENABLED);
    if (attributeString == "AXExpanded")
        return checkElementState(m_element.get(), ATK_STATE_EXPANDED);
    if (attributeString == "AXFocused")
        return checkElementState(m_element.get(), ATK_STATE_FOCUSED);
    if (attributeString == "AXInvalid")
        return checkElementState(m_element.get(), ATK_STATE_INVALID);
    if (attributeString == "AXModal")
        return checkElementState(m_element.get(), ATK_STATE_MODAL);
    if (attributeString == "AXMultiSelectable")
        return checkElementState(m_element.get(), ATK_STATE_MULTISELECTABLE);
    if (attributeString == "AXRequired")
        return checkElementState(m_element.get(), ATK_STATE_REQUIRED);
    if (attributeString == "AXSelected")
        return checkElementState(m_element.get(), ATK_STATE_SELECTED);
    if (attributeString == "AXSupportsAutoCompletion")
        return checkElementState(m_element.get(), ATK_STATE_SUPPORTS_AUTOCOMPLETION);
    if (attributeString == "AXVisited")
        return checkElementState(m_element.get(), ATK_STATE_VISITED);

    if (attributeString == "AXInterfaceTable")
        return ATK_IS_TABLE(m_element.get());
    if (attributeString == "AXInterfaceTableCell")
        return ATK_IS_TABLE_CELL(m_element.get());

    if (attributeString == "AXARIAAtomic") {
        String atkAttribute = coreAttributeToAtkAttribute(attribute);
        return getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, atkAttribute) == "true";
    }

    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return false;

    String attributeString = jsStringToWTFString(attribute);
    if (attributeString != "AXValue")
        return false;

    // ATK does not have a single state or property to indicate whether or not the value
    // of an accessible object can be set. ATs look at several states and properties based
    // on the type of object. If nothing explicitly indicates the value can or cannot be
    // set, ATs make role- and interface-based decisions. We'll do something similar here.

    // This state is expected to be present only for text widgets and contenteditable elements.
    if (checkElementState(m_element.get(), ATK_STATE_EDITABLE))
        return true;

    // This state is applicable to checkboxes, radiobuttons, switches, etc.
    if (checkElementState(m_element.get(), ATK_STATE_CHECKABLE))
        return true;

    // This state is expected to be present only for controls and only if explicitly set.
    if (checkElementState(m_element.get(), ATK_STATE_READ_ONLY))
        return false;

    // We expose an object attribute to ATs when there is an author-provided ARIA property
    // and also when there is a supported ARIA role but no author-provided value.
    String isReadOnly = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "readonly");
    if (!isReadOnly.isEmpty())
        return isReadOnly == "true" ? false : true;

    // If we have a native listbox or combobox and the value can be set, the options should
    // have ATK_STATE_SELECTABLE.
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element.get()));
    if (role == ATK_ROLE_LIST_BOX || role == ATK_ROLE_COMBO_BOX) {
        if (GRefPtr<AtkObject> child = adoptGRef(atk_object_ref_accessible_child(ATK_OBJECT(m_element.get()), 0))) {
            if (atk_object_get_role(ATK_OBJECT(child.get())) == ATK_ROLE_MENU)
                child = adoptGRef(atk_object_ref_accessible_child(ATK_OBJECT(child.get()), 0));
            return child && checkElementState(child.get(), ATK_STATE_SELECTABLE);
        }
    }

    // If we have a native element which exposes a range whose value can be set, it should
    // be focusable and have a true range.
    if (ATK_IS_VALUE(m_element.get()) && checkElementState(m_element.get(), ATK_STATE_FOCUSABLE))
        return minValue() != maxValue();

    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return false;

    String atkAttributeName = coreAttributeToAtkAttribute(attribute);
    if (atkAttributeName.isEmpty())
        return false;

    // In ATK, "busy" is a state and is supported on all AtkObject instances.
    if (atkAttributeName == "busy")
        return true;

    // For now, an attribute is supported whether it's exposed as a object or a text attribute.
    String attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, atkAttributeName);
    if (attributeValue.isEmpty())
        attributeValue = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), TextAttributeType, atkAttributeName);

    // When the aria-live value is "off", we expose that value via the "live" object attribute.
    if (atkAttributeName == "live" && attributeValue == "off")
        return false;

    return !attributeValue.isEmpty();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    GUniquePtr<char> roleStringWithPrefix(g_strdup_printf("AXRole: %s", roleToString(ATK_OBJECT(m_element.get()))));
    return JSStringCreateWithUTF8CString(roleStringWithPrefix.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    String roleDescription = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "roledescription");
    GUniquePtr<gchar> axRoleDescription(g_strdup_printf("AXRoleDescription: %s", roleDescription.utf8().data()));

    return JSStringCreateWithUTF8CString(axRoleDescription.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    String role = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "computed-role");
    if (!role.isEmpty())
        return JSStringCreateWithUTF8CString(role.utf8().data());

    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    const gchar* name = atk_object_get_name(ATK_OBJECT(m_element.get()));
    GUniquePtr<gchar> axTitle(g_strdup_printf("AXTitle: %s", name ? name : ""));

    return JSStringCreateWithUTF8CString(axTitle.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    const gchar* description = atk_object_get_description(ATK_OBJECT(m_element.get()));
    if (!description)
        return JSStringCreateWithCharacters(0, 0);

    GUniquePtr<gchar> axDesc(g_strdup_printf("AXDescription: %s", description));

    return JSStringCreateWithUTF8CString(axDesc.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    const gchar* axOrientation = nullptr;
    if (checkElementState(m_element.get(), ATK_STATE_HORIZONTAL))
        axOrientation = "AXOrientation: AXHorizontalOrientation";
    else if (checkElementState(m_element.get(), ATK_STATE_VERTICAL))
        axOrientation = "AXOrientation: AXVerticalOrientation";
    else
        axOrientation = "AXOrientation: AXUnknownOrientation";

    return JSStringCreateWithUTF8CString(axOrientation);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    GUniquePtr<gchar> text(atk_text_get_text(ATK_TEXT(m_element.get()), 0, -1));
    GUniquePtr<gchar> textWithReplacedCharacters(replaceCharactersForResults(text.get()));
    GUniquePtr<gchar> axValue(g_strdup_printf("AXValue: %s", textWithReplacedCharacters.get()));

    return JSStringCreateWithUTF8CString(axValue.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    const gchar* locale = atk_object_get_object_locale(ATK_OBJECT(m_element.get()));
    if (!locale)
        return JSStringCreateWithCharacters(0, 0);

    GUniquePtr<char> axValue(g_strdup_printf("AXLanguage: %s", locale));
    return JSStringCreateWithUTF8CString(axValue.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkRelationSet* relationSet = atk_object_ref_relation_set(ATK_OBJECT(m_element.get()));
    if (!relationSet)
        return JSStringCreateWithCharacters(0, 0);

    AtkRelation* relation = atk_relation_set_get_relation_by_type(relationSet, ATK_RELATION_DESCRIBED_BY);
    if (!relation)
        return JSStringCreateWithCharacters(0, 0);

    GPtrArray* targetList = atk_relation_get_target(relation);
    if (!targetList || !targetList->len)
        return JSStringCreateWithCharacters(0, 0);

    StringBuilder builder;
    builder.appendLiteral("AXHelp: ");

    for (guint targetCount = 0; targetCount < targetList->len; targetCount++) {
        if (AtkObject* target = static_cast<AtkObject*>(g_ptr_array_index(targetList, targetCount))) {
            GUniquePtr<gchar> text(atk_text_get_text(ATK_TEXT(target), 0, -1));
            if (targetCount)
                builder.append(' ');
            builder.append(text.get());
        }
    }

    g_object_unref(relationSet);

    return JSStringCreateWithUTF8CString(builder.toString().utf8().data());
}

double AccessibilityUIElement::x()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int x;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), &x, nullptr, nullptr, nullptr, ATK_XY_SCREEN);
    return x;
}

double AccessibilityUIElement::y()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int y;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), nullptr, &y, nullptr, nullptr, ATK_XY_SCREEN);
    return y;
}

double AccessibilityUIElement::width()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int width;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), nullptr, nullptr, &width, nullptr, ATK_XY_WINDOW);
    return width;
}

double AccessibilityUIElement::height()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int height;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), nullptr, nullptr, nullptr, &height, ATK_XY_WINDOW);
    return height;
}

double AccessibilityUIElement::clickPointX()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int x, width;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), &x, nullptr, &width, nullptr, ATK_XY_WINDOW);
    return x + width / 2.0;
}

double AccessibilityUIElement::clickPointY()
{
    if (!ATK_IS_COMPONENT(m_element.get()))
        return 0;

    int y, height;
    atk_component_get_extents(ATK_COMPONENT(m_element.get()), nullptr, &y, nullptr, &height, ATK_XY_WINDOW);
    return y + height / 2.0;
}

double AccessibilityUIElement::intValue() const
{
    if (!ATK_IS_OBJECT(m_element.get()))
        return 0;

    if (ATK_IS_VALUE(m_element.get())) {
        double value;
        atk_value_get_value_and_text(ATK_VALUE(m_element.get()), &value, nullptr);
        return value;
    }

    // Consider headings as an special case when returning the "int value" of
    // an AccessibilityUIElement, so we can reuse some tests to check the level
    // both for HTML headings and objects with the aria-level attribute.
    if (atk_object_get_role(ATK_OBJECT(m_element.get())) == ATK_ROLE_HEADING) {
        String headingLevel = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "level");
        bool ok;
        double headingLevelValue = headingLevel.toDouble(&ok);
        if (ok)
            return headingLevelValue;
    }

    return 0;
}

double AccessibilityUIElement::minValue()
{
    if (!ATK_IS_VALUE(m_element.get()))
        return 0;

    return rangeMinMaxValue(ATK_VALUE(m_element.get()), RangeLimitMinimum);
}

double AccessibilityUIElement::maxValue()
{
    if (!ATK_IS_VALUE(m_element.get()))
        return 0;

    return rangeMinMaxValue(ATK_VALUE(m_element.get()), RangeLimitMaximum);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    String valueText = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "valuetext");
    GUniquePtr<gchar> valueDescription(g_strdup_printf("AXValueDescription: %s", valueText.utf8().data()));
    return JSStringCreateWithUTF8CString(valueDescription.get());
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    // FIXME: implement
    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    if (!ATK_IS_ACTION(m_element.get()))
        return false;

    const gchar* actionName = atk_action_get_name(ATK_ACTION(m_element.get()), 0);
    return equalLettersIgnoringASCIICase(String(actionName), "press") || equalLettersIgnoringASCIICase(String(actionName), "jump");
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    return checkElementState(m_element.get(), ATK_STATE_ENABLED);
}

bool AccessibilityUIElement::isRequired() const
{
    return checkElementState(m_element.get(), ATK_STATE_REQUIRED);
}

bool AccessibilityUIElement::isFocused() const
{
    return checkElementState(m_element.get(), ATK_STATE_FOCUSED);
}

bool AccessibilityUIElement::isSelected() const
{
    return checkElementState(m_element.get(), ATK_STATE_SELECTED);
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    return checkElementState(m_element.get(), ATK_STATE_ACTIVE);
}

bool AccessibilityUIElement::isExpanded() const
{
    return checkElementState(m_element.get(), ATK_STATE_EXPANDED);
}

bool AccessibilityUIElement::isChecked() const
{
    return checkElementState(m_element.get(), ATK_STATE_CHECKED);
}

bool AccessibilityUIElement::isIndeterminate() const
{
    return checkElementState(m_element.get(), ATK_STATE_INDETERMINATE);
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    String level = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "level");
    if (!level.isEmpty())
        return level.toInt();

    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "grabbed") == "true";
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    String dropEffects = getAttributeSetValueForId(ATK_OBJECT(m_element.get()), ObjectAttributeType, "dropeffect");
    return dropEffects.isEmpty() ? JSStringCreateWithCharacters(0, 0) : JSStringCreateWithUTF8CString(dropEffects.utf8().data());
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return -1;

    if (index < 0 || index > atk_text_get_character_count(ATK_TEXT(m_element.get())))
        return -1;

    GUniquePtr<gchar> text(atk_text_get_text(ATK_TEXT(m_element.get()), 0, index));
    int lineNo = 0;
    for (gchar* offset = text.get(); *offset; ++offset) {
        if (*offset == '\n')
            ++lineNo;
    }

    return lineNo;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkText* text = ATK_TEXT(m_element.get());
    gint startOffset = 0, endOffset = 0;
    for (int i = 0; i <= line; ++i)
        atk_text_get_string_at_offset(text, endOffset, ATK_TEXT_GRANULARITY_LINE, &startOffset, &endOffset);

    GUniquePtr<gchar> range(g_strdup_printf("{%d, %d}", startOffset, endOffset - startOffset));
    return JSStringCreateWithUTF8CString(range.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkTextRectangle rect;
    atk_text_get_range_extents(ATK_TEXT(m_element.get()), location, location + length, ATK_XY_WINDOW, &rect);

    GUniquePtr<gchar> bounds(g_strdup_printf("{%d, %d, %d, %d}", rect.x, rect.y, rect.width, rect.height));
    return JSStringCreateWithUTF8CString(bounds.get());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    String string = atk_text_get_text(ATK_TEXT(m_element.get()), location, location + length);
    return JSStringCreateWithUTF8CString(string.utf8().data());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    StringBuilder builder;

    // The default text attributes apply to the entire element.
    builder.appendLiteral("\n\tDefault text attributes:\n\t\t");
    builder.append(attributeSetToString(getAttributeSet(m_element.get(), TextAttributeType), "\n\t\t"));

    // The attribute run provides attributes specific to the range of text at the specified offset.
    AtkText* text = ATK_TEXT(m_element.get());
    gint start = 0, end = 0;
    for (unsigned i = location; i < location + length; i = end) {
        AtkAttributeSet* attributeSet = atk_text_get_run_attributes(text, i, &start, &end);
        GUniquePtr<gchar> substring(replaceCharactersForResults(atk_text_get_text(text, start, end)));
        builder.appendLiteral("\n\tRange attributes for '");
        builder.append(substring.get());
        builder.appendLiteral("':\n\t\t");
        builder.append(attributeSetToString(attributeSet, "\n\t\t"));
    }

    return JSStringCreateWithUTF8CString(builder.toString().utf8().data());
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    // FIXME: implement
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    // FIXME: implement
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    if (!ATK_IS_TABLE(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    Vector<RefPtr<AccessibilityUIElement> > columnHeaders = getTableColumnHeaders(ATK_TABLE(m_element.get()));
    return createStringWithAttributes(columnHeaders);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    if (!ATK_IS_TABLE(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    Vector<RefPtr<AccessibilityUIElement> > rowHeaders = getTableRowHeaders(ATK_TABLE(m_element.get()));
    return createStringWithAttributes(rowHeaders);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    if (!ATK_IS_TABLE(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    Vector<RefPtr<AccessibilityUIElement> > visibleCells = getVisibleCells(this);
    return createStringWithAttributes(visibleCells);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::rowCount()
{
    if (!ATK_IS_TABLE(m_element.get()))
        return 0;

    return atk_table_get_n_rows(ATK_TABLE(m_element.get()));
}

int AccessibilityUIElement::columnCount()
{
    if (!ATK_IS_TABLE(m_element.get()))
        return 0;

    return atk_table_get_n_columns(ATK_TABLE(m_element.get()));
}

int AccessibilityUIElement::indexInTable()
{
    // FIXME: implement
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    // Range in table for rows.
    return indexRangeInTable(m_element.get(), true);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    // Range in table for columns.
    return indexRangeInTable(m_element.get(), false);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    if (!ATK_IS_TABLE(m_element.get()))
        return nullptr;

    // Adopt the AtkObject representing the cell because
    // at_table_ref_at() transfers full ownership.
    GRefPtr<AtkObject> foundCell = adoptGRef(atk_table_ref_at(ATK_TABLE(m_element.get()), row, col));
    return foundCell ? AccessibilityUIElement::create(foundCell.get()).ptr() : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    if (!ATK_IS_TEXT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    gint start, end;
    g_free(atk_text_get_selection(ATK_TEXT(m_element.get()), 0, &start, &end));

    GUniquePtr<gchar> selection(g_strdup_printf("{%d, %d}", start, end - start));
    return JSStringCreateWithUTF8CString(selection.get());
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    if (!ATK_IS_TEXT(m_element.get()))
        return false;

    if (!length)
        return atk_text_set_caret_offset(ATK_TEXT(m_element.get()), location);

    return atk_text_set_selection(ATK_TEXT(m_element.get()), 0, location, location + length);
}

void AccessibilityUIElement::increment()
{
    alterCurrentValue(m_element.get(), 1);
}

void AccessibilityUIElement::decrement()
{
    alterCurrentValue(m_element.get(), -1);
}

void AccessibilityUIElement::showMenu()
{
    // FIXME: implement
}

void AccessibilityUIElement::press()
{
    if (!ATK_IS_ACTION(m_element.get()))
        return;

    // Only one action per object is supported so far.
    atk_action_do_action(ATK_ACTION(m_element.get()), 0);
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
    // FIXME: implement
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    if (!ATK_IS_SELECTION(m_element.get()))
        return;

    atk_selection_add_selection(ATK_SELECTION(m_element.get()), index);
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    if (!ATK_IS_SELECTION(m_element.get()))
        return;

    atk_selection_remove_selection(ATK_SELECTION(m_element.get()), index);
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    if (!ATK_IS_SELECTION(m_element.get()))
        return;

    atk_selection_clear_selection(ATK_SELECTION(m_element.get()));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    if (!ATK_IS_DOCUMENT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element.get()));
    if (role != ATK_ROLE_DOCUMENT_WEB)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element.get()), "Encoding"));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    if (!ATK_IS_DOCUMENT(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element.get()));
    if (role != ATK_ROLE_DOCUMENT_WEB)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element.get()), "URI"));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    if (!ATK_IS_HYPERLINK_IMPL(m_element.get()))
        return JSStringCreateWithCharacters(0, 0);

    AtkHyperlink* hyperlink = atk_hyperlink_impl_get_hyperlink(ATK_HYPERLINK_IMPL(m_element.get()));
    GUniquePtr<char> hyperlinkURI(atk_hyperlink_get_uri(hyperlink, 0));

    if (!hyperlinkURI.get())
        return JSStringCreateWithUTF8CString("AXURL: (null)");

    // Build the result string, stripping the absolute URL paths if present.
    char* localURI = g_strstr_len(hyperlinkURI.get(), -1, "LayoutTests");
    String axURL = makeString("AXURL: ", localURI ? localURI : hyperlinkURI.get());
    return JSStringCreateWithUTF8CString(axURL.utf8().data());
}

bool AccessibilityUIElement::addNotificationListener(JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    // Only one notification listener per element.
    if (m_notificationHandler)
        return false;

    m_notificationHandler = AccessibilityNotificationHandler::create();
    m_notificationHandler->setPlatformElement(platformUIElement());
    m_notificationHandler->setNotificationFunctionCallback(functionCallback);

    return true;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    // Programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);
    m_notificationHandler = nullptr;

    return true;
}

bool AccessibilityUIElement::isFocusable() const
{
    return checkElementState(m_element.get(), ATK_STATE_FOCUSABLE);
}

bool AccessibilityUIElement::isSelectable() const
{
    return checkElementState(m_element.get(), ATK_STATE_SELECTABLE);
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    return checkElementState(m_element.get(), ATK_STATE_MULTISELECTABLE);
}

bool AccessibilityUIElement::isVisible() const
{
    return checkElementState(m_element.get(), ATK_STATE_VISIBLE);
}

bool AccessibilityUIElement::isOffScreen() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSingleLine() const
{
    return checkElementState(m_element.get(), ATK_STATE_SINGLE_LINE);
}

bool AccessibilityUIElement::isMultiLine() const
{
    return checkElementState(m_element.get(), ATK_STATE_MULTI_LINE);
}

bool AccessibilityUIElement::hasPopup() const
{
    return checkElementState(m_element.get(), ATK_STATE_HAS_POPUP);
}

void AccessibilityUIElement::takeFocus()
{
    // FIXME: implement
}

void AccessibilityUIElement::takeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::addSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::removeSelection()
{
    // FIXME: implement
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    // FIXME: implement
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    // FIXME: implement
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    // FIXME: implement
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    // FIXME: implement
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
    // FIXME: implement
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker* marker)
{
    // FIXME: implement
    return -1;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    // FIXME: implement
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    // FIXME: implement
    return nullptr;
}
    
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    // FIXME: implement
    return nullptr;    
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    // FIXME: implement
    return nullptr;
}

bool AccessibilityUIElement::setSelectedVisibleTextRange(AccessibilityTextMarkerRange*)
{
    return false;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
#if ATK_CHECK_VERSION(2, 30, 0)
    if (!ATK_IS_COMPONENT(m_element.get()))
        return;

    atk_component_scroll_to(ATK_COMPONENT(m_element.get()), ATK_SCROLL_ANYWHERE);
#endif
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
#if ATK_CHECK_VERSION(2, 30, 0)
    if (!ATK_IS_COMPONENT(m_element.get()))
        return;

    atk_component_scroll_to_point(ATK_COMPONENT(m_element.get()), ATK_XY_WINDOW, x, y);
#endif
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    // FIXME: implement
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    // FIXME: implement
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    notImplemented();
    return nullptr;
}

JSRetainPtr<JSStringRef> stringAtOffset(PlatformUIElement element, AtkTextBoundary boundary, int offset)
{
    if (!ATK_IS_TEXT(element.get()))
        return JSStringCreateWithCharacters(0, 0);

    gint startOffset, endOffset;
    StringBuilder builder;

    AtkTextGranularity granularity;
    switch (boundary) {
    case ATK_TEXT_BOUNDARY_CHAR:
        granularity = ATK_TEXT_GRANULARITY_CHAR;
        break;
    case ATK_TEXT_BOUNDARY_WORD_START:
        granularity = ATK_TEXT_GRANULARITY_WORD;
        break;
    case ATK_TEXT_BOUNDARY_LINE_START:
        granularity = ATK_TEXT_GRANULARITY_LINE;
        break;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
        granularity = ATK_TEXT_GRANULARITY_SENTENCE;
        break;
    default:
        return JSStringCreateWithCharacters(0, 0);
    }

    builder.append(atk_text_get_string_at_offset(ATK_TEXT(element.get()), offset, granularity, &startOffset, &endOffset));

    builder.appendLiteral(", ");
    builder.appendNumber(startOffset);
    builder.appendLiteral(", ");
    builder.appendNumber(endOffset);
    return JSStringCreateWithUTF8CString(builder.toString().utf8().data());
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::characterAtOffset(int offset)
{
    return stringAtOffset(m_element, ATK_TEXT_BOUNDARY_CHAR, offset);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::wordAtOffset(int offset)
{
    return stringAtOffset(m_element, ATK_TEXT_BOUNDARY_WORD_START, offset);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineAtOffset(int offset)
{
    return stringAtOffset(m_element, ATK_TEXT_BOUNDARY_LINE_START, offset);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sentenceAtOffset(int offset)
{
    return stringAtOffset(m_element, ATK_TEXT_BOUNDARY_SENTENCE_START, offset);
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef, int, int)
{
    notImplemented();
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef)
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    notImplemented();
    return nullptr;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
