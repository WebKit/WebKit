/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "AttributedString.h"

#import "Font.h"
#import "Logging.h"
#import <Foundation/Foundation.h>
#import <wtf/cocoa/VectorCocoa.h>
#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <pal/ios/UIKitSoftLink.h>
#import "UIFoundationSoftLink.h"
#endif

namespace WebCore {

using IdentifierToTableMap = HashMap<AttributedString::TextTableID, NSTextTable *>;
using IdentifierToListMap = HashMap<AttributedString::TextListID, NSTextList *>;

using TableToIdentifierMap = HashMap<NSTextTable *, AttributedString::TextTableID>;
using ListToIdentifierMap = HashMap<NSTextList *, AttributedString::TextListID>;

AttributedString::AttributedString() = default;

AttributedString::~AttributedString() = default;

AttributedString::AttributedString(AttributedString&&) = default;

AttributedString& AttributedString::operator=(AttributedString&&) = default;

AttributedString::AttributedString(const AttributedString&) = default;

AttributedString& AttributedString::operator=(const AttributedString&) = default;

AttributedString::AttributedString(String&& string, Vector<std::pair<Range, HashMap<String, AttributeValue>>>&& attributes, std::optional<HashMap<String, AttributeValue>>&& documentAttributes)
    : string(WTFMove(string))
    , attributes(WTFMove(attributes))
    , documentAttributes(WTFMove(documentAttributes)) { }

bool AttributedString::rangesAreSafe(const String& string, const Vector<std::pair<Range, HashMap<String, AttributeValue>>>& vector)
{
    auto stringLength = string.length();
    for (auto& pair : vector) {
        auto& range = pair.first;
        if (range.location > stringLength
            || range.length > stringLength
            || range.location + range.length > stringLength)
            return false;
    }
    return true;
}

inline static RetainPtr<NSParagraphStyle> reconstructStyle(const AttributedString::ParagraphStyleWithTableAndListIDs& styleInfo, IdentifierToTableMap& tables, IdentifierToListMap& lists)
{
    auto style = styleInfo.style;
    auto textBlocks = [style textBlocks];
    auto textLists = [style textLists];
    RetainPtr<NSMutableArray<NSTextBlock *>> adjustedTextBlocks;
    RetainPtr<NSMutableArray<NSTextList *>> adjustedTextLists;

    if (UNLIKELY(styleInfo.tableIDs.size() != textBlocks.count || styleInfo.listIDs.size() != textLists.count)) {
        ASSERT_NOT_REACHED();
        return style;
    }

    for (size_t index = 0; index < styleInfo.tableIDs.size(); ++index) {
        auto block = textBlocks[index];
        if (![block isKindOfClass:PlatformNSTextTableBlock])
            continue;

        auto tableID = styleInfo.tableIDs[index];
        if (!tableID)
            continue;

        auto cell = static_cast<NSTextTableBlock *>(block);
        auto ensureResult = tables.ensure(*tableID, [&] {
            return cell.table;
        });
        if (!ensureResult.isNewEntry) {
            auto replacementBlock = adoptNS([[PlatformNSTextTableBlock alloc] initWithTable:ensureResult.iterator->value startingRow:cell.startingRow rowSpan:cell.rowSpan startingColumn:cell.startingColumn columnSpan:cell.columnSpan]);
            if (!adjustedTextBlocks)
                adjustedTextBlocks = adoptNS((NSMutableArray<NSTextBlock *> *)[[style textBlocks] mutableCopy]);
            [adjustedTextBlocks setObject:replacementBlock.get() atIndexedSubscript:index];
        }
    }

    for (size_t index = 0; index < styleInfo.listIDs.size(); ++index) {
        auto list = textLists[index];
        auto listID = styleInfo.listIDs[index];
        auto ensureResult = lists.ensure(listID, [&] {
            return list;
        });
        if (!ensureResult.isNewEntry) {
            if (!adjustedTextLists)
                adjustedTextLists = adoptNS((NSMutableArray<NSTextList *> *)[[style textLists] mutableCopy]);
            [adjustedTextLists setObject:ensureResult.iterator->value atIndexedSubscript:index];
        }
    }

    if (!adjustedTextBlocks && !adjustedTextLists)
        return style;

    auto mutableStyle = adoptNS([style mutableCopy]);
    if (adjustedTextBlocks)
        [mutableStyle setTextBlocks:adjustedTextBlocks.get()];
    if (adjustedTextLists)
        [mutableStyle setTextLists:adjustedTextLists.get()];
    return mutableStyle;
}

static RetainPtr<id> toNSObject(const AttributedString::AttributeValue& value, IdentifierToTableMap& tables, IdentifierToListMap& lists)
{
    return WTF::switchOn(value.value, [] (double value) -> RetainPtr<id> {
        return adoptNS([[NSNumber alloc] initWithDouble:value]);
    }, [] (const String& value) -> RetainPtr<id> {
        return (NSString *)value;
    }, [&] (const AttributedString::ParagraphStyleWithTableAndListIDs& value) -> RetainPtr<id> {
        return reconstructStyle(value, tables, lists);
    }, [] (const RetainPtr<NSPresentationIntent>& value) -> RetainPtr<id> {
        return value;
    }, [] (const URL& value) -> RetainPtr<id> {
        return (NSURL *)value;
    }, [] (const Vector<String>& value) -> RetainPtr<id> {
        return createNSArray(value, [] (const String& string) {
            return (NSString *)string;
        });
    }, [] (const Vector<double>& value) -> RetainPtr<id> {
        return createNSArray(value, [] (double number) {
            return adoptNS([[NSNumber alloc] initWithDouble:number]);
        });
    }, [] (const RetainPtr<NSTextAttachment>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSShadow>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSDate>& value) -> RetainPtr<id> {
        return value;
    }, [] (const Ref<Font>& font) -> RetainPtr<id> {
        return (__bridge PlatformFont *)(font->getCTFont());
    }, [] (const RetainPtr<PlatformColor>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<CGColorRef>& value) -> RetainPtr<id> {
        return (__bridge id)value.get();
    });
}

static RetainPtr<NSDictionary> toNSDictionary(const HashMap<String, AttributedString::AttributeValue>& map, IdentifierToTableMap& tables, IdentifierToListMap& lists)
{
    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& pair : map) {
        if (auto nsObject = toNSObject(pair.value, tables, lists))
            [result setObject:nsObject.get() forKey:(NSString *)pair.key];
    }
    return result;
}

RetainPtr<NSDictionary> AttributedString::documentAttributesAsNSDictionary() const
{
    if (!documentAttributes)
        return nullptr;

    IdentifierToTableMap tables;
    IdentifierToListMap lists;
    return toNSDictionary(*documentAttributes, tables, lists);
}

RetainPtr<NSAttributedString> AttributedString::nsAttributedString() const
{
    if (string.isNull())
        return nullptr;

    IdentifierToTableMap tables;
    IdentifierToListMap lists;
    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:(NSString *)string]);
    for (auto& pair : attributes) {
        auto& map = pair.second;
        auto& range = pair.first;
        [result addAttributes:toNSDictionary(map, tables, lists).get() range:NSMakeRange(range.location, range.length)];
    }
    return result;
}

static std::optional<AttributedString::AttributeValue> extractArray(NSArray *array)
{
    size_t arrayLength = array.count;
    if (!arrayLength)
        return { { { Vector<String>() } } };
    if ([array[0] isKindOfClass:NSString.class]) {
        Vector<String> result;
        result.reserveInitialCapacity(arrayLength);
        for (id element in array) {
            if ([element isKindOfClass:NSString.class])
                result.uncheckedAppend((NSString *)element);
            else
                RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array containing <%@>", NSStringFromClass([element class]));
        }
        return { { { WTFMove(result) } } };
    }
    if ([array[0] isKindOfClass:NSNumber.class]) {
        Vector<double> result;
        result.reserveInitialCapacity(arrayLength);
        for (id element in array) {
            if ([element isKindOfClass:NSNumber.class])
                result.uncheckedAppend([(NSNumber *)element doubleValue]);
            else
                RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array containing <%@>", NSStringFromClass([element class]));
        }
        return { { { WTFMove(result) } } };
    }
    RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array of unknown values");
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

inline static Vector<AttributedString::TextListID> extractListIDs(NSParagraphStyle *style, ListToIdentifierMap& listIDs)
{
    return makeVector(style.textLists, [&](NSTextList *list) {
        return std::optional { listIDs.ensure(list, [] {
            return AttributedString::TextListID::generate();
        }).iterator->value };
    });
}

inline static Vector<std::optional<AttributedString::TextTableID>> extractTableIDs(NSParagraphStyle *style, TableToIdentifierMap& tableIDs)
{
    return makeVector(style.textBlocks, [&](NSTextBlock *block) -> std::optional<std::optional<AttributedString::TextTableID>> {
        std::optional<AttributedString::TextTableID> result;
        if ([block isKindOfClass:PlatformNSTextTableBlock]) {
            if (auto table = [static_cast<NSTextTableBlock *>(block) table]) {
                result = tableIDs.ensure(table, [] {
                    return AttributedString::TextTableID::generate();
                }).iterator->value;
            } else
                result = std::nullopt;
        } else
            result = std::nullopt;
        return std::optional { WTFMove(result) };
    });
}

static std::optional<AttributedString::AttributeValue> extractValue(id value, TableToIdentifierMap& tableIDs, ListToIdentifierMap& listIDs)
{
    if (CFGetTypeID((CFTypeRef)value) == CGColorGetTypeID())
        return { { { RetainPtr<CGColorRef> { (CGColorRef) value } } } };
    if (auto* number = dynamic_objc_cast<NSNumber>(value))
        return { { { number.doubleValue } } };
    if (auto* string = dynamic_objc_cast<NSString>(value))
        return { { { String { string } } } };
    if (auto* url = dynamic_objc_cast<NSURL>(value))
        return { { { URL { url } } } };
    if (auto* array = dynamic_objc_cast<NSArray>(value))
        return extractArray(array);
    if (auto* date = dynamic_objc_cast<NSDate>(value))
        return { { { RetainPtr { date } } } };
    if ([value isKindOfClass:PlatformNSShadow])
        return { { { RetainPtr { (NSShadow *)value } } } };
    if ([value isKindOfClass:PlatformNSParagraphStyle]) {
        auto style = static_cast<NSParagraphStyle *>(value);
        return { { AttributedString::ParagraphStyleWithTableAndListIDs {
            RetainPtr { style },
            extractTableIDs(style, tableIDs),
            extractListIDs(style, listIDs)
        } } };
    }
    if ([value isKindOfClass:PlatformNSPresentationIntent])
        return { { { RetainPtr { (NSPresentationIntent *)value } } } };
    if ([value isKindOfClass:PlatformNSTextAttachment])
        return { { { RetainPtr { (NSTextAttachment *)value } } } };
    if ([value isKindOfClass:PlatformFontClass])
        return { { { Font::create(FontPlatformData((__bridge CTFontRef)value, [(PlatformFont *)value pointSize])) } } };
    if ([value isKindOfClass:PlatformColorClass])
        return { { { RetainPtr { (PlatformColor *)value } } } };
    if (value) {
        RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed for class <%@>", NSStringFromClass([value class]));
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return std::nullopt;
}

static HashMap<String, AttributedString::AttributeValue> extractDictionary(NSDictionary *dictionary, TableToIdentifierMap& tableIDs, ListToIdentifierMap& listIDs)
{
    HashMap<String, AttributedString::AttributeValue> result;
    [dictionary enumerateKeysAndObjectsUsingBlock:[&](id key, id value, BOOL *) {
        if (![key isKindOfClass:NSString.class]) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto extractedValue = extractValue(value, tableIDs, listIDs);
        if (!extractedValue) {
            ASSERT_NOT_REACHED();
            return;
        }
        result.set((NSString *)key, WTFMove(*extractedValue));
    }];
    return result;
}

AttributedString AttributedString::fromNSAttributedString(RetainPtr<NSAttributedString>&& string)
{
    return fromNSAttributedStringAndDocumentAttributes(WTFMove(string), nullptr);
}

AttributedString AttributedString::fromNSAttributedStringAndDocumentAttributes(RetainPtr<NSAttributedString>&& string, RetainPtr<NSDictionary>&& dictionary)
{
    __block TableToIdentifierMap tableIDs;
    __block ListToIdentifierMap listIDs;
    __block AttributedString result;
    result.string = [string string];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired usingBlock: ^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        result.attributes.append({ Range { range.location, range.length }, extractDictionary(attributes, tableIDs, listIDs) });
    }];
    if (dictionary)
        result.documentAttributes = extractDictionary(dictionary.get(), tableIDs, listIDs);
    return { WTFMove(result) };
}

}
