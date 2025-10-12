/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DecomposedAttributedText.h"

#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

static DecomposedAttributedText::ListMarker convertMarker(NSTextListMarkerFormat marker)
{
    if ([marker isEqualToString:NSTextListMarkerDecimal])
        return DecomposedAttributedText::ListMarker::Decimal;
    if ([marker isEqualToString:NSTextListMarkerDisc])
        return DecomposedAttributedText::ListMarker::Disc;
    if ([marker isEqualToString:NSTextListMarkerCircle])
        return DecomposedAttributedText::ListMarker::Circle;
    if ([marker isEqualToString:NSTextListMarkerLowercaseRoman])
        return DecomposedAttributedText::ListMarker::LowercaseRoman;

    return DecomposedAttributedText::ListMarker { marker };
}

#if PLATFORM(MAC)
using PlatformFont = NSFont;
using PlatformFontDescriptorSymbolicTraits = NSFontDescriptorSymbolicTraits;

constexpr auto PlatformFontDescriptorTraitBold = NSFontDescriptorTraitBold;
constexpr auto PlatformFontDescriptorTraitItalic = NSFontDescriptorTraitItalic;
#else
using PlatformFont = UIFont;
using PlatformFontDescriptorSymbolicTraits = UIFontDescriptorSymbolicTraits;

constexpr auto PlatformFontDescriptorTraitBold = UIFontDescriptorTraitBold;
constexpr auto PlatformFontDescriptorTraitItalic = UIFontDescriptorTraitItalic;
#endif

// Merge the last element of `children` and `child` iff they are both Strings and do not have newlines in between;
// otherwise, add `child` as a new element.
// FIXME: This is incomplete currently, and should ideally merge any `Element`s if they are structurally the same.
static void addTextChild(Vector<DecomposedAttributedText::Element>& children, const DecomposedAttributedText::Element& child)
{
    if (children.isEmpty()) {
        children.append(child);
        return;
    }

    auto& last = children.last();
    if (last.index() != child.index() || !std::holds_alternative<String>(child)) {
        children.append(child);
        return;
    }

    auto lastString = std::get<String>(last);
    auto childString = std::get<String>(child);

    if (lastString.endsWith('\n') || childString.startsWith('\n')) {
        children.append(child);
        return;
    }

    last = makeString(lastString, childString);
}

static void decomposeChildren(HashMap<NSTextList *, DecomposedAttributedText::ListID>& textListToIdentifiers, NSDictionary<NSAttributedStringKey, id> *attributes, size_t index, Vector<DecomposedAttributedText::Element>& children, NSString *text)
{
    RetainPtr<NSArray<NSTextList *>> textLists = [attributes[NSParagraphStyleAttributeName] textLists];
    RetainPtr<PlatformFont> font = attributes[NSFontAttributeName];

    if (!textLists || index == [textLists count]) {
        PlatformFontDescriptorSymbolicTraits traits = [font fontDescriptor].symbolicTraits;

        DecomposedAttributedText::Element child = text;
        if (traits & PlatformFontDescriptorTraitBold)
            child = DecomposedAttributedText::Bold { { child } };
        if (traits & PlatformFontDescriptorTraitItalic)
            child = DecomposedAttributedText::Italic { { child } };

        addTextChild(children, child);
        return;
    }

    auto textList = [textLists objectAtIndex:index];
    if (textListToIdentifiers.find(textList) == textListToIdentifiers.end()) {
        auto identifier = DecomposedAttributedText::ListID::generate();
        textListToIdentifiers.set(textList, identifier);

        std::optional<DecomposedAttributedText::Element> element;

        if (textList.isOrdered) {
            DecomposedAttributedText::OrderedList list { identifier };
            list.startingItemNumber = textList.startingItemNumber;
            list.marker = convertMarker(textList.markerFormat);
            element = list;
        } else {
            DecomposedAttributedText::UnorderedList list { identifier };
            list.marker = convertMarker(textList.markerFormat);
            element = list;
        }

        children.append(*element);
    }

    auto listID = textListToIdentifiers.get(textList);
    for (auto& child : children) {
        if (auto *ul = std::get_if<DecomposedAttributedText::UnorderedList>(&child); ul && ul->identifier == listID)
            decomposeChildren(textListToIdentifiers, attributes, index + 1, ul->children, text);
        else if (auto *ol = std::get_if<DecomposedAttributedText::OrderedList>(&child); ol && ol->identifier == listID)
            decomposeChildren(textListToIdentifiers, attributes, index + 1, ol->children, text);
    }
}

using Runs = Vector<std::pair<NSString *, NSDictionary<NSAttributedStringKey, id> *>>;

DecomposedAttributedText decomposeAttributedTextRuns(const Runs& runs)
{
    HashMap<NSTextList *, DecomposedAttributedText::ListID> listToId;
    Vector<DecomposedAttributedText::Element> children;

    for (const auto& [text, attributes] : runs)
        decomposeChildren(listToId, attributes, 0, children, text);

    return { children };
}

Runs runsForAttributedText(NSAttributedString *string)
{
    __block Runs allTextAttributes;
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        RetainPtr substring = [[string string] substringWithRange:range];
        allTextAttributes.append(std::pair { substring, attributes });
    }];
    return allTextAttributes;
}

DecomposedAttributedText decompose(NSAttributedString *string)
{
    return decomposeAttributedTextRuns(runsForAttributedText(string));
}

bool operator==(const DecomposedAttributedText::ListMarker& lhs, const DecomposedAttributedText::ListMarker& rhs)
{
    return lhs.data == rhs.data;
}

bool operator==(const DecomposedAttributedText::OrderedList& lhs, const DecomposedAttributedText::OrderedList& rhs)
{
    return lhs.marker == rhs.marker && lhs.startingItemNumber == rhs.startingItemNumber && lhs.children == rhs.children;
}

bool operator==(const DecomposedAttributedText::UnorderedList& lhs, const DecomposedAttributedText::UnorderedList& rhs)
{
    return lhs.marker == rhs.marker && lhs.children == rhs.children;
}

bool operator==(const DecomposedAttributedText::Bold& lhs, const DecomposedAttributedText::Bold& rhs)
{
    return lhs.children == rhs.children;
}

bool operator==(const DecomposedAttributedText::Italic& lhs, const DecomposedAttributedText::Italic& rhs)
{
    return lhs.children == rhs.children;
}

bool operator==(const DecomposedAttributedText& lhs, const DecomposedAttributedText& rhs)
{
    return lhs.children == rhs.children;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText::Bold& value)
{
    stream << "b";
    stream.dumpProperty("children", value.children);
    return stream;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText::Italic& value)
{
    stream << "i";
    stream.dumpProperty("children", value.children);
    return stream;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText::OrderedList& value)
{
    stream << "ol";
    stream.dumpProperty("marker", value.marker);
    stream.dumpProperty("start", value.startingItemNumber);
    stream.dumpProperty("children", value.children);
    return stream;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText::UnorderedList& value)
{
    stream << "ul";
    stream.dumpProperty("marker", value.marker);
    stream.dumpProperty("children", value.children);
    return stream;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText& value)
{
    stream << value.children;
    return stream;
}

TextStream& operator<<(TextStream& stream, const DecomposedAttributedText::ListMarker& value)
{
    WTF::switchOn(value.data, [&](DecomposedAttributedText::ListMarker::Type type) {
        switch (type) {
        case DecomposedAttributedText::ListMarker::Circle:
            stream << "circle";
            break;
        case DecomposedAttributedText::ListMarker::Decimal:
            stream << "decimal";
            break;
        case DecomposedAttributedText::ListMarker::Disc:
            stream << "disc";
            break;
        case DecomposedAttributedText::ListMarker::LowercaseRoman:
            stream << "lowercase-roman";
            break;
        }
    }, [&](const String& string) {
        stream << string;
    });

    return stream;
}
