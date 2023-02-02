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
#include "AccessibilityAtspi.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityObject.h"
#include "AccessibilityObjectInterface.h"
#include "Editing.h"
#include "PlatformScreen.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include <gio/gio.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

AccessibilityObjectAtspi::TextGranularity AccessibilityObjectAtspi::atspiBoundaryToTextGranularity(Atspi::TextBoundaryType boundaryType)
{
    switch (boundaryType) {
    case Atspi::TextBoundaryType::CharBoundary:
        return TextGranularity::Character;
    case Atspi::TextBoundaryType::WordStartBoundary:
        return TextGranularity::WordStart;
    case Atspi::TextBoundaryType::WordEndBoundary:
        return TextGranularity::WordEnd;
    case Atspi::TextBoundaryType::SentenceStartBoundary:
        return TextGranularity::SentenceStart;
    case Atspi::TextBoundaryType::SentenceEndBoundary:
        return TextGranularity::SentenceEnd;
    case Atspi::TextBoundaryType::LineStartBoundary:
        return TextGranularity::LineStart;
    case Atspi::TextBoundaryType::LineEndBoundary:
        return TextGranularity::LineEnd;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

AccessibilityObjectAtspi::TextGranularity AccessibilityObjectAtspi::atspiGranularityToTextGranularity(Atspi::TextGranularityType boundaryType)
{
    switch (boundaryType) {
    case Atspi::TextGranularityType::CharGranularity:
        return TextGranularity::Character;
    case Atspi::TextGranularityType::WordGranularity:
        return TextGranularity::WordStart;
    case Atspi::TextGranularityType::SentenceGranularity:
        return TextGranularity::SentenceStart;
    case Atspi::TextGranularityType::LineGranularity:
        return TextGranularity::LineStart;
    case Atspi::TextGranularityType::ParagraphGranularity:
        return TextGranularity::Paragraph;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

GDBusInterfaceVTable AccessibilityObjectAtspi::s_textFunctions = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(methodName, "GetStringAtOffset")) {
            int offset;
            uint32_t granularityType;
            g_variant_get(parameters, "(iu)", &offset, &granularityType);
            int start = 0, end = 0;
            auto text = atspiObject->textAtOffset(offset, atspiGranularityToTextGranularity(static_cast<Atspi::TextGranularityType>(granularityType)), start, end);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(sii)", text.isNull() ? "" : text.data(), start, end));
        } else if (!g_strcmp0(methodName, "GetText")) {
            int start, end;
            g_variant_get(parameters, "(ii)", &start, &end);
            auto text = atspiObject->text(start, end);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", text.isNull() ? "" : text.data()));
        } else if (!g_strcmp0(methodName, "SetCaretOffset")) {
            int offset;
            g_variant_get(parameters, "(i)", &offset);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->selectRange(offset, offset)));
        } else if (!g_strcmp0(methodName, "GetTextBeforeOffset")) {
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        } else if (!g_strcmp0(methodName, "GetTextAtOffset")) {
            int offset;
            uint32_t boundaryType;
            g_variant_get(parameters, "(iu)", &offset, &boundaryType);
            int start = 0, end = 0;
            auto text = atspiObject->textAtOffset(offset, atspiBoundaryToTextGranularity(static_cast<Atspi::TextBoundaryType>(boundaryType)), start, end);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(sii)", text.isNull() ? "" : text.data(), start, end));
        } else if (!g_strcmp0(methodName, "GetTextAfterOffset"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
        else if (!g_strcmp0(methodName, "GetCharacterAtOffset")) {
            int offset;
            g_variant_get(parameters, "(i)", &offset);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", atspiObject->characterAtOffset(offset)));
        } else if (!g_strcmp0(methodName, "GetAttributeValue")) {
            int offset;
            const char* name;
            g_variant_get(parameters, "(i&s)", &offset, &name);
            auto attributes = atspiObject->textAttributesWithUTF8Offset(offset);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", attributes.attributes.get(String::fromUTF8(name)).utf8().data()));
        } else if (!g_strcmp0(methodName, "GetAttributes")) {
            int offset;
            g_variant_get(parameters, "(i)", &offset);
            auto attributes = atspiObject->textAttributesWithUTF8Offset(offset);
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a{ss}"));
            for (const auto& it : attributes.attributes)
                g_variant_builder_add(&builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a{ss}ii)", &builder, attributes.startOffset, attributes.endOffset));
        } else if (!g_strcmp0(methodName, "GetAttributeRun")) {
            int offset;
            gboolean includeDefaults;
            g_variant_get(parameters, "(ib)", &offset, &includeDefaults);
            auto attributes = atspiObject->textAttributesWithUTF8Offset(offset, includeDefaults);
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a{ss}"));
            for (const auto& it : attributes.attributes)
                g_variant_builder_add(&builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a{ss}ii)", &builder, attributes.startOffset, attributes.endOffset));
        } else if (!g_strcmp0(methodName, "GetDefaultAttributes") || !g_strcmp0(methodName, "GetDefaultAttributeSet")) {
            auto attributes = atspiObject->textAttributesWithUTF8Offset();
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("a{ss}"));
            for (const auto& it : attributes.attributes)
                g_variant_builder_add(&builder, "{ss}", it.key.utf8().data(), it.value.utf8().data());
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(a{ss})", &builder));
        } else if (!g_strcmp0(methodName, "GetCharacterExtents")) {
            int offset;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iu)", &offset, &coordinateType);
            auto extents = atspiObject->textExtents(offset, offset + 1, static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(iiii)", extents.x(), extents.y(), extents.width(), extents.height()));
        } else if (!g_strcmp0(methodName, "GetRangeExtents")) {
            int start, end;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iiu)", &start, &end, &coordinateType);
            auto extents = atspiObject->textExtents(start, end, static_cast<Atspi::CoordinateType>(coordinateType));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(iiii)", extents.x(), extents.y(), extents.width(), extents.height()));
        } else if (!g_strcmp0(methodName, "GetOffsetAtPoint")) {
            int x, y;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iiu)", &x, &y, &coordinateType);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", atspiObject->offsetAtPoint(IntPoint(x, y), static_cast<Atspi::CoordinateType>(coordinateType))));
        } else if (!g_strcmp0(methodName, "GetNSelections")) {
            int start, end;
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", atspiObject->selectionBounds(start, end) && start != end ? 1 : 0));
        } else if (!g_strcmp0(methodName, "GetSelection")) {
            int selectionNumber;
            g_variant_get(parameters, "(i)", &selectionNumber);
            if (selectionNumber)
                g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", selectionNumber);
            else {
                int start = 0, end = 0;
                if (atspiObject->selectionBounds(start, end) && start == end)
                    start = end = 0;
                g_dbus_method_invocation_return_value(invocation, g_variant_new("(ii)", start, end));
            }
        } else if (!g_strcmp0(methodName, "AddSelection")) {
            int start, end;
            g_variant_get(parameters, "(ii)", &start, &end);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->selectRange(start, end)));
        } else if (!g_strcmp0(methodName, "SetSelection")) {
            int start, end, selectionNumber;
            g_variant_get(parameters, "(iii)", &selectionNumber, &start, &end);
            if (selectionNumber)
                g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Not a valid selection: %d", selectionNumber);
            else
                g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->selectRange(start, end)));
        } else if (!g_strcmp0(methodName, "RemoveSelection")) {
            int selectionNumber;
            g_variant_get(parameters, "(i)", &selectionNumber);
            int caretOffset = -1;
            if (!selectionNumber) {
                int start, end;
                if (atspiObject->selectionBounds(start, end))
                    caretOffset = end;
            }
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", caretOffset != -1 ? atspiObject->selectRange(caretOffset, caretOffset) : FALSE));
        } else if (!g_strcmp0(methodName, "ScrollSubstringTo")) {
            int start, end;
            uint32_t scrollType;
            g_variant_get(parameters, "(iiu)", &start, &end, &scrollType);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->scrollToMakeVisible(start, end, static_cast<Atspi::ScrollType>(scrollType))));
        } else if (!g_strcmp0(methodName, "ScrollSubstringToPoint")) {
            int start, end, x, y;
            uint32_t coordinateType;
            g_variant_get(parameters, "(iiuii)", &start, &end, &coordinateType, &x, &y);
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", atspiObject->scrollToPoint(start, end, static_cast<Atspi::CoordinateType>(coordinateType), x, y)));
        } else if (!g_strcmp0(methodName, "GetBoundedRanges") || !g_strcmp0(methodName, "ScrollSubstringToPoint"))
            g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "");
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer userData) -> GVariant* {
        auto atspiObject = Ref { *static_cast<AccessibilityObjectAtspi*>(userData) };
        atspiObject->updateBackingStore();

        if (!g_strcmp0(propertyName, "CharacterCount"))
            return g_variant_new_int32(g_utf8_strlen(atspiObject->text().utf8().data(), -1));
        if (!g_strcmp0(propertyName, "CaretOffset")) {
            int start = 0, end = 0;
            return g_variant_new_int32(atspiObject->selectionBounds(start, end) ? end : -1);
        }

        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

static Vector<unsigned, 128> offsetMapping(const String& text)
{
    if (text.is8Bit())
        return { };

    Vector<unsigned, 128> offsets;
    SurrogatePairAwareTextIterator iterator(text.characters16(), 0, text.length(), text.length());
    UChar32 character;
    unsigned clusterLength = 0;
    unsigned i;
    for (i = 0; iterator.consume(character, clusterLength); iterator.advance(clusterLength), ++i) {
        for (unsigned j = 0; j < clusterLength; ++j)
            offsets.append(i);
    }
    offsets.append(i++);
    return offsets;
}

static inline unsigned UTF16OffsetToUTF8(const Vector<unsigned, 128>& mapping, unsigned offset)
{
    return mapping.isEmpty() ? offset : mapping[offset];
}

static inline unsigned UTF8OffsetToUTF16(const Vector<unsigned, 128>& mapping, unsigned offset)
{
    if (mapping.isEmpty())
        return offset;

    for (unsigned i = offset; i < mapping.size(); ++i) {
        if (mapping[i] == offset)
            return i;
    }
    return mapping.size();
}

String AccessibilityObjectAtspi::text() const
{
    if (!m_coreObject)
        return { };

    m_hasListMarkerAtStart = false;

#if ENABLE(INPUT_TYPE_COLOR)
    if (m_coreObject->roleValue() == AccessibilityRole::ColorWell) {
        auto color = convertColor<SRGBA<float>>(m_coreObject->colorValue()).resolved();
        GUniquePtr<char> colorString(g_strdup_printf("rgb %7.5f %7.5f %7.5f 1", color.red, color.green, color.blue));
        return String::fromUTF8(colorString.get());
    }
#endif

    if (m_coreObject->isTextControl())
        return m_coreObject->doAXStringForRange({ 0, String::MaxLength });

    auto value = m_coreObject->stringValue();
    if (!value.isNull())
        return value;

    auto text = m_coreObject->textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren));
    if (auto* renderer = m_coreObject->renderer()) {
        if (is<RenderListItem>(*renderer) && downcast<RenderListItem>(*renderer).markerRenderer()) {
            if (renderer->style().direction() == TextDirection::LTR) {
                text = makeString(objectReplacementCharacter, text);
                m_hasListMarkerAtStart = true;
            } else
                text = makeString(text, objectReplacementCharacter);
        }
    }
    return text;
}

unsigned AccessibilityObject::getLengthForTextRange() const
{
    // FIXME: this should probably be in sync with AccessibilityObjectAtspi::text().
    unsigned textLength = text().length();
    if (textLength)
        return textLength;

    Node* node = this->node();
    RenderObject* renderer = node ? node->renderer() : nullptr;
    if (is<RenderText>(renderer))
        textLength = downcast<RenderText>(*renderer).text().length();

    if (!textLength && allowsTextRanges())
        textLength = textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren)).length();

    return textLength;
}

bool AccessibilityObject::allowsTextRanges() const
{
    return true;
}

CString AccessibilityObjectAtspi::text(int startOffset, int endOffset) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return { };

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    if (endOffset == -1)
        endOffset = length;

    if (startOffset < 0 || endOffset < 0)
        return { };

    if (endOffset <= startOffset)
        return { };

    if (!startOffset && endOffset == length)
        return utf8Text;

    GUniquePtr<char> substring(g_utf8_substring(utf8Text.data(), startOffset, endOffset));
    return substring.get();
}

static inline int adjustInputOffset(unsigned utf16Offset, bool hasListMarkerAtStart)
{
    return hasListMarkerAtStart && utf16Offset ? utf16Offset - 1 : utf16Offset;
}

static inline int adjustOutputOffset(unsigned utf16Offset, bool hasListMarkerAtStart)
{
    return hasListMarkerAtStart ? utf16Offset + 1 : utf16Offset;
}

void AccessibilityObjectAtspi::textInserted(const String& insertedText, const VisiblePosition& position)
{
    if (!m_interfaces.contains(Interface::Text))
        return;

    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    auto utf16Offset = adjustOutputOffset(m_coreObject->indexForVisiblePosition(position), m_hasListMarkerAtStart);
    String maskedText = m_coreObject->isPasswordField() ? utf16Text.substring(utf16Offset - insertedText.length(), insertedText.length()) : String();
    auto mapping = offsetMapping(utf16Text);
    auto offset = UTF16OffsetToUTF8(mapping, utf16Offset);
    auto utf8InsertedText = maskedText.isNull() ? insertedText.utf8() : maskedText.utf8();
    auto insertedTextLength = g_utf8_strlen(utf8InsertedText.data(), -1);
    AccessibilityAtspi::singleton().textChanged(*this, "insert", WTFMove(utf8InsertedText), offset - insertedTextLength, insertedTextLength);
}

void AccessibilityObjectAtspi::textDeleted(const String& deletedText, const VisiblePosition& position)
{
    if (!m_interfaces.contains(Interface::Text))
        return;

    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    auto utf16Offset = adjustOutputOffset(m_coreObject->indexForVisiblePosition(position), m_hasListMarkerAtStart);
    auto mapping = offsetMapping(utf16Text);
    auto offset = UTF16OffsetToUTF8(mapping, utf16Offset);
    auto utf8DeletedText = deletedText.utf8();
    auto deletedTextLength = g_utf8_strlen(utf8DeletedText.data(), -1);
    AccessibilityAtspi::singleton().textChanged(*this, "delete", WTFMove(utf8DeletedText), offset, deletedTextLength);
}

IntPoint AccessibilityObjectAtspi::boundaryOffset(unsigned utf16Offset, TextGranularity granularity) const
{
    if (!m_coreObject)
        return { };

    VisiblePosition offsetPosition = m_coreObject->visiblePositionForIndex(adjustInputOffset(utf16Offset, m_hasListMarkerAtStart));
    VisiblePosition startPosition, endPostion;
    switch (granularity) {
    case TextGranularity::Character:
        RELEASE_ASSERT_NOT_REACHED();
    case TextGranularity::WordStart: {
        if (!utf16Offset && m_hasListMarkerAtStart)
            return { 0, 1 };

        startPosition = isStartOfWord(offsetPosition) && deprecatedIsEditingWhitespace(offsetPosition.characterBefore()) ? offsetPosition : startOfWord(offsetPosition, LeftWordIfOnBoundary);
        endPostion = nextWordPosition(startPosition);
        auto positionAfterSpacingAndFollowingWord = nextWordPosition(endPostion);
        if (positionAfterSpacingAndFollowingWord != endPostion) {
            auto previousPosition = previousWordPosition(positionAfterSpacingAndFollowingWord);
            if (previousPosition == startPosition)
                endPostion = positionAfterSpacingAndFollowingWord;
            else
                endPostion = previousPosition;
        }
        break;
    }
    case TextGranularity::WordEnd: {
        if (!utf16Offset && m_hasListMarkerAtStart)
            return { 0, 1 };

        startPosition = previousWordPosition(offsetPosition);
        auto positionBeforeSpacingAndPreviousWord = previousWordPosition(startPosition);
        if (positionBeforeSpacingAndPreviousWord != startPosition)
            startPosition = nextWordPosition(positionBeforeSpacingAndPreviousWord);
        endPostion = endOfWord(offsetPosition);
        break;
    }
    case TextGranularity::SentenceStart: {
        startPosition = startOfSentence(offsetPosition);
        endPostion = endOfSentence(startPosition);
        if (offsetPosition == endPostion) {
            startPosition = nextSentencePosition(startPosition);
            endPostion = endOfSentence(startPosition);
        }
        break;
    }
    case TextGranularity::SentenceEnd:
        startPosition = previousSentencePosition(offsetPosition);
        endPostion = endOfSentence(offsetPosition);
        break;
    case TextGranularity::LineStart:
        startPosition = logicalStartOfLine(offsetPosition);
        endPostion = nextLinePosition(offsetPosition, 0);
        break;
    case TextGranularity::LineEnd:
        startPosition = logicalStartOfLine(offsetPosition);
        endPostion = logicalEndOfLine(offsetPosition);
        break;
    case TextGranularity::Paragraph:
        startPosition = startOfParagraph(offsetPosition);
        endPostion = endOfParagraph(offsetPosition);
        break;
    }

    auto startOffset = m_coreObject->indexForVisiblePosition(startPosition);
    // For no word boundaries, include the list marker if start offset is 0.
    if (!startOffset && m_hasListMarkerAtStart && (granularity == TextGranularity::WordStart || granularity == TextGranularity::WordEnd))
        startOffset = adjustOutputOffset(startOffset, m_hasListMarkerAtStart);
    return { startOffset, adjustOutputOffset(m_coreObject->indexForVisiblePosition(endPostion), m_hasListMarkerAtStart) };
}

CString AccessibilityObjectAtspi::textAtOffset(int offset, TextGranularity granularity, int& startOffset, int& endOffset) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return { };

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    if (offset < 0 || offset > length)
        return { };

    if (granularity == TextGranularity::Character) {
        startOffset = offset;
        endOffset = std::min(offset + 1, length);
    } else {
        auto mapping = offsetMapping(utf16Text);
        auto utf16Offset = UTF8OffsetToUTF16(mapping, offset);
        auto boundaryOffset = this->boundaryOffset(utf16Offset, granularity);
        startOffset = UTF16OffsetToUTF8(mapping, std::max<int>(boundaryOffset.x(), 0));
        endOffset = UTF16OffsetToUTF8(mapping, std::min<int>(boundaryOffset.y(), utf16Text.length()));
    }

    GUniquePtr<char> substring(g_utf8_substring(utf8Text.data(), startOffset, endOffset));
    return substring.get();
}

int AccessibilityObjectAtspi::characterAtOffset(int offset) const
{
    auto utf8Text = text().utf8();
    if (utf8Text.isNull())
        return 0;

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    if (offset < 0 || offset >= length)
        return 0;

    return g_utf8_get_char(g_utf8_offset_to_pointer(utf8Text.data(), offset));
}

std::optional<unsigned> AccessibilityObjectAtspi::characterOffset(UChar character, int index) const
{
    auto utf16Text = text();
    unsigned start = 0;
    size_t offset;
    while ((offset = utf16Text.find(character, start)) != notFound) {
        start = offset + 1;
        if (!index)
            break;
        index--;
    }

    if (offset == notFound)
        return std::nullopt;

    auto mapping = offsetMapping(utf16Text);
    return UTF16OffsetToUTF8(mapping, offset);
}

std::optional<unsigned> AccessibilityObjectAtspi::characterIndex(UChar character, unsigned offset) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return std::nullopt;

    auto length = static_cast<unsigned>(g_utf8_strlen(utf8Text.data(), -1));
    if (offset >= length)
        return std::nullopt;

    auto mapping = offsetMapping(utf16Text);
    auto utf16Offset = UTF8OffsetToUTF16(mapping, offset);
    if (utf16Text[utf16Offset] != character)
        return std::nullopt;

    unsigned start = 0;
    int index = -1;
    size_t position;
    while ((position = utf16Text.find(character, start)) != notFound) {
        index++;
        if (static_cast<unsigned>(position) == utf16Offset)
            break;
        start = position + 1;
    }

    if (index == -1)
        return std::nullopt;

    return index;
}

IntRect AccessibilityObjectAtspi::boundsForRange(unsigned utf16Offset, unsigned length, Atspi::CoordinateType coordinateType) const
{
    if (!m_coreObject)
        return { };

    auto extents = m_coreObject->doAXBoundsForRange(PlainTextRange(utf16Offset, length));

    auto* frameView = m_coreObject->documentFrameView();
    if (!frameView)
        return extents;

    switch (coordinateType) {
    case Atspi::CoordinateType::ScreenCoordinates:
        return frameView->contentsToScreen(extents);
    case Atspi::CoordinateType::WindowCoordinates:
        return frameView->contentsToWindow(extents);
    case Atspi::CoordinateType::ParentCoordinates:
        return extents;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

IntRect AccessibilityObjectAtspi::textExtents(int startOffset, int endOffset, Atspi::CoordinateType coordinateType) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return { };

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    startOffset = std::clamp(startOffset, 0, length);
    if (endOffset == -1)
        endOffset = length;
    else
        endOffset = std::clamp(endOffset, 0, length);
    if (endOffset <= startOffset)
        return { };

    auto mapping = offsetMapping(utf16Text);
    auto utf16StartOffset = UTF8OffsetToUTF16(mapping, startOffset);
    auto utf16EndOffset = UTF8OffsetToUTF16(mapping, endOffset);
    return boundsForRange(utf16StartOffset, utf16EndOffset - utf16StartOffset, coordinateType);
}

int AccessibilityObjectAtspi::offsetAtPoint(const IntPoint& point, Atspi::CoordinateType coordinateType) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return -1;

    auto convertedPoint = point;
    if (auto* frameView = m_coreObject->documentFrameView()) {
        switch (coordinateType) {
        case Atspi::CoordinateType::ScreenCoordinates:
            convertedPoint = frameView->screenToContents(point);
            break;
        case Atspi::CoordinateType::WindowCoordinates:
            convertedPoint = frameView->windowToContents(point);
            break;
        case Atspi::CoordinateType::ParentCoordinates:
            break;
        }
    }

    auto position = m_coreObject->visiblePositionForPoint(convertedPoint);
    if (position.isNull())
        return -1;

    auto utf16Offset = adjustOutputOffset(m_coreObject->indexForVisiblePosition(position), m_hasListMarkerAtStart);
    if (utf16Offset == -1)
        return -1;

    return UTF16OffsetToUTF8(offsetMapping(utf16Text), utf16Offset);
}

IntPoint AccessibilityObjectAtspi::boundsForSelection(const VisibleSelection& selection) const
{
    if (selection.isNone())
        return { -1, -1 };

    Node* node = nullptr;
    if (!m_coreObject->isNativeTextControl())
        node = m_coreObject->node();
    else {
        auto positionInTextControlInnerElement = m_coreObject->visiblePositionForIndex(0);
        if (auto* innerMostNode = positionInTextControlInnerElement.deepEquivalent().anchorNode())
            node = innerMostNode->parentNode();
    }
    if (!node)
        return { -1, -1 };

    // We need to limit our search to positions that fall inside the domain of the current object.
    auto firstValidPosition = firstPositionInOrBeforeNode(node->firstDescendant());
    auto lastValidPosition = lastPositionInOrAfterNode(node->lastDescendant());

    if (!intersects(makeVisiblePositionRange(makeSimpleRange(firstValidPosition, lastValidPosition)), VisiblePositionRange(selection)))
        return { -1, -1 };

    // Find the proper range for the selection that falls inside the object.
    auto nodeRangeStart = std::max(selection.start(), firstValidPosition);
    auto nodeRangeEnd = std::min(selection.end(), lastValidPosition);

    // Calculate position of the selected range inside the object.
    auto parentFirstPosition = firstPositionInOrBeforeNode(node);
    auto rangeInParent = *makeSimpleRange(parentFirstPosition, nodeRangeStart);

    // Set values for start offsets and calculate initial range length.
    int startOffset = characterCount(rangeInParent, TextIteratorBehavior::EmitsObjectReplacementCharacters);
    auto nodeRange = *makeSimpleRange(nodeRangeStart, nodeRangeEnd);
    int rangeLength = characterCount(nodeRange, TextIteratorBehavior::EmitsObjectReplacementCharacters);
    return { startOffset, startOffset + rangeLength };
}

IntPoint AccessibilityObjectAtspi::selectedRange() const
{
    if (!m_coreObject)
        return { -1, -1 };

    return boundsForSelection(m_coreObject->selection());
}

bool AccessibilityObjectAtspi::selectionBounds(int& startOffset, int& endOffset) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return false;

    auto bounds = selectedRange();
    if (bounds.x() < 0)
        return false;

    auto mapping = offsetMapping(utf16Text);
    startOffset = UTF16OffsetToUTF8(mapping, bounds.x());
    endOffset = UTF16OffsetToUTF8(mapping, bounds.y());

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    endOffset = std::clamp(endOffset, 0, length);
    if (endOffset < startOffset) {
        startOffset = endOffset = 0;
        return false;
    }

    return true;
}

void AccessibilityObjectAtspi::setSelectedRange(unsigned utf16Offset, unsigned length)
{
    if (!m_coreObject)
        return;

    auto range = m_coreObject->visiblePositionRangeForRange(PlainTextRange(utf16Offset, length));
    m_coreObject->setSelectedVisiblePositionRange(range);
}

bool AccessibilityObjectAtspi::selectRange(int startOffset, int endOffset)
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return false;

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    startOffset = std::clamp(startOffset, 0, length);
    if (endOffset == -1)
        endOffset = length;
    else
        endOffset = std::clamp(endOffset, 0, length);

    auto mapping = offsetMapping(utf16Text);
    auto utf16StartOffset = UTF8OffsetToUTF16(mapping, startOffset);
    auto utf16EndOffset = startOffset == endOffset ? utf16StartOffset : UTF8OffsetToUTF16(mapping, endOffset);
    setSelectedRange(utf16StartOffset, utf16EndOffset - utf16StartOffset);

    return true;
}

void AccessibilityObjectAtspi::selectionChanged(const VisibleSelection& selection)
{
    if (!m_interfaces.contains(Interface::Text))
        return;

    if (selection.isNone())
        return;

    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return;

    auto bounds = boundsForSelection(selection);
    if (bounds.y() < 0)
        return;

    auto length = static_cast<unsigned>(g_utf8_strlen(utf8Text.data(), -1));
    auto mapping = offsetMapping(utf16Text);
    auto caretOffset = UTF16OffsetToUTF8(mapping, bounds.y());
    if (caretOffset <= length)
        AccessibilityAtspi::singleton().textCaretMoved(*this, caretOffset);

    if (selection.isRange())
        AccessibilityAtspi::singleton().textSelectionChanged(*this);
}

AccessibilityObjectAtspi::TextAttributes AccessibilityObjectAtspi::textAttributes(std::optional<unsigned> utf16Offset, bool includeDefault) const
{
    if (!m_coreObject || !m_coreObject->renderer())
        return { };

    auto accessibilityTextAttributes = [this](AXCoreObject* axObject, const HashMap<String, String>& defaultAttributes) -> HashMap<String, String> {
        HashMap<String, String> attributes;
        auto& style = axObject->renderer()->style();

        auto addAttributeIfNeeded = [&](const String& name, const String& value) {
            if (defaultAttributes.isEmpty() || defaultAttributes.get(name) != value)
                attributes.add(name, value);
        };

        auto bgColor = style.visitedDependentColor(CSSPropertyBackgroundColor);
        if (bgColor.isValid() && bgColor.isVisible()) {
            auto [r, g, b, a] = bgColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
            addAttributeIfNeeded("bg-color"_s, makeString(r, ',', g, ',', b));
        }

        auto fgColor = style.visitedDependentColor(CSSPropertyColor);
        if (fgColor.isValid() && fgColor.isVisible()) {
            auto [r, g, b, a] = fgColor.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
            addAttributeIfNeeded("fg-color"_s, makeString(r, ',', g, ',', b));
        }

        addAttributeIfNeeded("family-name"_s, style.fontCascade().firstFamily());
        addAttributeIfNeeded("size"_s, makeString(std::round(style.computedFontPixelSize() * 72 / WebCore::screenDPI()), "pt"));
        addAttributeIfNeeded("weight"_s, makeString(static_cast<float>(style.fontCascade().weight())));
        addAttributeIfNeeded("style"_s, style.fontCascade().italic() ? "italic"_s : "normal"_s);
        addAttributeIfNeeded("strikethrough"_s, style.textDecorationLine() & TextDecorationLine::LineThrough ? "true"_s : "false"_s);
        addAttributeIfNeeded("underline"_s, style.textDecorationLine() & TextDecorationLine::Underline ? "single"_s : "none"_s);
        addAttributeIfNeeded("invisible"_s, style.visibility() == Visibility::Hidden ? "true"_s : "false"_s);
        addAttributeIfNeeded("editable"_s, m_coreObject->canSetValueAttribute() ? "true"_s : "false"_s);
        addAttributeIfNeeded("direction"_s, style.direction() == TextDirection::LTR ? "ltr"_s : "rtl"_s);

        if (!style.textIndent().isUndefined())
            addAttributeIfNeeded("indent"_s, makeString(valueForLength(style.textIndent(), m_coreObject->size().width()).toInt()));

        switch (style.textAlign()) {
        case TextAlignMode::Start:
        case TextAlignMode::End:
            break;
        case TextAlignMode::Left:
        case TextAlignMode::WebKitLeft:
            addAttributeIfNeeded("justification"_s, "left"_s);
            break;
        case TextAlignMode::Right:
        case TextAlignMode::WebKitRight:
            addAttributeIfNeeded("justification"_s, "right"_s);
            break;
        case TextAlignMode::Center:
        case TextAlignMode::WebKitCenter:
            addAttributeIfNeeded("justification"_s, "center"_s);
            break;
        case TextAlignMode::Justify:
            addAttributeIfNeeded("justification"_s, "fill"_s);
            break;
        }

        String invalidStatus = m_coreObject->invalidStatus();
        if (invalidStatus != "false"_s)
            addAttributeIfNeeded("invalid"_s, invalidStatus);

        String language = m_coreObject->language();
        if (!language.isEmpty())
            addAttributeIfNeeded("language"_s, language);

        return attributes;
    };

    auto defaultAttributes = accessibilityTextAttributes(m_coreObject, { });
    if (!utf16Offset)
        return { WTFMove(defaultAttributes), -1, -1 };

    if (is<RenderListMarker>(*m_coreObject->renderer()))
        return { WTFMove(defaultAttributes), 0, static_cast<int>(m_coreObject->stringValue().length()) };

    if (!m_coreObject->node())
        return { WTFMove(defaultAttributes), -1, -1 };

    if (!*utf16Offset && m_hasListMarkerAtStart) {
        // Always consider list marker an independent run.
        auto attributes = accessibilityTextAttributes(m_coreObject->children()[0].get(), defaultAttributes);
        if (!includeDefault)
            return { WTFMove(attributes), 0, 1 };

        for (const auto& it : attributes)
            defaultAttributes.set(it.key, it.value);
        return { WTFMove(defaultAttributes), 0, 1 };
    }

    VisiblePosition offsetPosition = m_coreObject->visiblePositionForIndex(adjustInputOffset(*utf16Offset, m_hasListMarkerAtStart));
    auto* childNode = offsetPosition.deepEquivalent().deprecatedNode();
    if (!childNode)
        return { WTFMove(defaultAttributes), -1, -1 };

    auto* childRenderer = childNode->renderer();
    if (!childRenderer)
        return { WTFMove(defaultAttributes), -1, -1 };

    auto* childAxObject = childRenderer->document().axObjectCache()->get(childRenderer);
    if (!childAxObject || childAxObject == m_coreObject)
        return { WTFMove(defaultAttributes), -1, -1 };

    auto attributes = accessibilityTextAttributes(childAxObject, defaultAttributes);
    auto firstValidPosition = firstPositionInOrBeforeNode(m_coreObject->node()->firstDescendant());
    auto lastValidPosition = lastPositionInOrAfterNode(m_coreObject->node()->lastDescendant());

    auto* startRenderer = childRenderer;
    auto startPosition = firstPositionInOrBeforeNode(startRenderer->node());
    for (RenderObject* r = childRenderer->previousInPreOrder(); r && startPosition > firstValidPosition; r = r->previousInPreOrder()) {
        if (r->firstChildSlow())
            continue;

        auto childAttributes = accessibilityTextAttributes(r->document().axObjectCache()->get(r), defaultAttributes);
        if (childAttributes != attributes)
            break;

        startRenderer = r;
        startPosition = firstPositionInOrBeforeNode(startRenderer->node());
    }

    auto* endRenderer = childRenderer;
    auto endPosition = lastPositionInOrAfterNode(endRenderer->node());
    for (RenderObject* r = childRenderer->nextInPreOrder(); r && endPosition < lastValidPosition; r = r->nextInPreOrder()) {
        if (r->firstChildSlow())
            continue;

        auto childAttributes = accessibilityTextAttributes(r->document().axObjectCache()->get(r), defaultAttributes);
        if (childAttributes != attributes)
            break;

        endRenderer = r;
        endPosition = lastPositionInOrAfterNode(endRenderer->node());
    }

    auto startOffset = adjustOutputOffset(m_coreObject->indexForVisiblePosition(startPosition), m_hasListMarkerAtStart);
    auto endOffset = adjustOutputOffset(m_coreObject->indexForVisiblePosition(endPosition), m_hasListMarkerAtStart);
    if (!includeDefault)
        return { WTFMove(attributes), startOffset, endOffset };

    for (const auto& it : attributes)
        defaultAttributes.set(it.key, it.value);

    return { WTFMove(defaultAttributes), startOffset, endOffset };
}

AccessibilityObjectAtspi::TextAttributes AccessibilityObjectAtspi::textAttributesWithUTF8Offset(std::optional<int> offset, bool includeDefault) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return { };

    auto mapping = offsetMapping(utf16Text);
    std::optional<unsigned> utf16Offset;
    if (offset) {
        auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
        if (*offset < 0 || *offset >= length)
            return { };

        utf16Offset = UTF8OffsetToUTF16(mapping, *offset);
    }

    auto attributes = textAttributes(utf16Offset, includeDefault);
    if (attributes.startOffset != -1)
        attributes.startOffset = UTF16OffsetToUTF8(mapping, attributes.startOffset);
    if (attributes.endOffset != -1)
        attributes.endOffset = UTF16OffsetToUTF8(mapping, attributes.endOffset);

    return attributes;
}

void AccessibilityObjectAtspi::textAttributesChanged()
{
    if (!m_interfaces.contains(Interface::Text))
        return;

    AccessibilityAtspi::singleton().textAttributesChanged(*this);
}

bool AccessibilityObjectAtspi::scrollToMakeVisible(int startOffset, int endOffset, Atspi::ScrollType scrollType) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return false;

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    if (startOffset < 0 || startOffset > length)
        return false;
    if (endOffset < 0 || endOffset > length)
        return false;
    if (endOffset < startOffset)
        std::swap(startOffset, endOffset);

    auto mapping = offsetMapping(utf16Text);
    auto utf16StartOffset = UTF8OffsetToUTF16(mapping, startOffset);
    auto utf16EndOffset = UTF8OffsetToUTF16(mapping, endOffset);
    if (!m_coreObject->renderer())
        return true;

    IntRect rect = m_coreObject->doAXBoundsForRange(PlainTextRange(utf16StartOffset, utf16EndOffset - utf16StartOffset));

    if (m_coreObject->isScrollView()) {
        if (auto* parent = m_coreObject->parentObject())
            parent->scrollToMakeVisible();
    }

    ScrollAlignment alignX;
    ScrollAlignment alignY;
    switch (scrollType) {
    case Atspi::ScrollType::TopLeft:
        alignX = ScrollAlignment::alignLeftAlways;
        alignY = ScrollAlignment::alignTopAlways;
        break;
    case Atspi::ScrollType::BottomRight:
        alignX = ScrollAlignment::alignRightAlways;
        alignY = ScrollAlignment::alignBottomAlways;
        break;
    case Atspi::ScrollType::TopEdge:
    case Atspi::ScrollType::BottomEdge:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignToEdgeIfNeeded;
        break;
    case Atspi::ScrollType::LeftEdge:
    case Atspi::ScrollType::RightEdge:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignToEdgeIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    case Atspi::ScrollType::Anywhere:
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    }

    FrameView::scrollRectToVisible(rect, *m_coreObject->renderer(), false, { SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::Yes });
    return true;
}

bool AccessibilityObjectAtspi::scrollToPoint(int startOffset, int endOffset, Atspi::CoordinateType coordinateType, int x, int y) const
{
    auto utf16Text = text();
    auto utf8Text = utf16Text.utf8();
    if (utf8Text.isNull())
        return false;

    auto length = static_cast<int>(g_utf8_strlen(utf8Text.data(), -1));
    if (startOffset < 0 || startOffset > length)
        return false;
    if (endOffset < 0 || endOffset > length)
        return false;
    if (endOffset < startOffset)
        std::swap(startOffset, endOffset);

    auto mapping = offsetMapping(utf16Text);
    auto utf16StartOffset = UTF8OffsetToUTF16(mapping, startOffset);
    auto utf16EndOffset = UTF8OffsetToUTF16(mapping, endOffset);
    IntPoint point(x, y);
    if (coordinateType == Atspi::CoordinateType::ScreenCoordinates) {
        if (auto* frameView = m_coreObject->documentFrameView())
            point = frameView->contentsToWindow(frameView->screenToContents(point));
    }

    IntRect rect = m_coreObject->doAXBoundsForRange(PlainTextRange(utf16StartOffset, utf16EndOffset - utf16StartOffset));
    point.move(-rect.x(), -rect.y());
    m_coreObject->scrollToGlobalPoint(point);
    return true;
}

} // namespace WebCore

#endif // USE(ATSPI)
