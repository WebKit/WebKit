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

#import "BitmapImage.h"
#import "ColorCocoa.h"
#import "Font.h"
#import "LoaderNSURLExtras.h"
#import "Logging.h"
#import "PlatformNSAdaptiveImageGlyph.h"
#import "WebCoreTextAttachment.h"
#import <Foundation/Foundation.h>
#import <pal/spi/cocoa/UIFoundationSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#import <pal/spi/mac/NSTextTableSPI.h>
#else
#import "WAKAppKitStubs.h"
#import <pal/ios/UIKitSoftLink.h>
#import "UIFoundationSoftLink.h"
#endif

OBJC_CLASS NSTextTab;

namespace WebCore {

using IdentifierToTableMap = HashMap<AttributedStringTextTableID, RetainPtr<NSTextTable>>;
using IdentifierToTableBlockMap = HashMap<AttributedStringTextTableBlockID, RetainPtr<NSTextTableBlock>>;
using IdentifierToListMap = HashMap<AttributedStringTextListID, RetainPtr<NSTextList>>;


using TableToIdentifierMap = HashMap<NSTextTable *, AttributedString::TextTableID>;
using TableBlockToIdentifierMap = HashMap<NSTextTableBlock *, AttributedString::TextTableBlockID>;
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

inline static void configureNSTextBlockFromParagraphStyleCommonTableAttributes(NSTextBlock* table, const ParagraphStyleCommonTableAttributes& item)
{
    [table setValue:item.width type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockWidth];
    [table setValue:item.minimumWidth type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumWidth];
    [table setValue:item.maximumWidth type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumWidth];
    [table setValue:item.minimumHeight type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumHeight];
    [table setValue:item.maximumHeight type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumHeight];

    [table setWidth:item.paddingMinXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    [table setWidth:item.paddingMinYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    [table setWidth:item.paddingMaxXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    [table setWidth:item.paddingMaxYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];

    [table setWidth:item.borderMinXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinXEdge];
    [table setWidth:item.borderMinYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinYEdge];
    [table setWidth:item.borderMaxXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxXEdge];
    [table setWidth:item.borderMaxYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxYEdge];

    [table setWidth:item.marginMinXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge];
    [table setWidth:item.marginMinYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge];
    [table setWidth:item.marginMaxXEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge];
    [table setWidth:item.marginMaxYEdge type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge];

    if (item.backgroundColor)
        [table setBackgroundColor:item.backgroundColor.get()];

    if (item.borderMinXEdgeColor)
        [table setBorderColor:item.borderMinXEdgeColor.get() forEdge:NSMinXEdge];

    if (item.borderMinYEdgeColor)
        [table setBorderColor:item.borderMinYEdgeColor.get() forEdge:NSMinYEdge];

    if (item.borderMaxXEdgeColor)
        [table setBorderColor:item.borderMaxXEdgeColor.get() forEdge:NSMaxXEdge];

    if (item.borderMaxYEdgeColor)
        [table setBorderColor:item.borderMaxYEdgeColor.get() forEdge:NSMaxYEdge];
}

inline static NSTextAlignment reconstructNSTextAlignment(ParagraphStyleAlignment alignment)
{
    switch (alignment) {
    case ParagraphStyleAlignment::Left:
        return NSTextAlignmentLeft;
    case ParagraphStyleAlignment::Right:
        return NSTextAlignmentRight;
    case ParagraphStyleAlignment::Centre:
        return NSTextAlignmentCenter;
    case ParagraphStyleAlignment::Justified:
        return NSTextAlignmentJustified;
    case ParagraphStyleAlignment::Natural:
        return NSTextAlignmentNatural;
    };
    ASSERT_NOT_REACHED();
    return NSTextAlignmentLeft;
}

inline static NSTextBlockVerticalAlignment reconstructNSTextBlockVerticalAlignment(TextTableBlockVerticalAlignment alignment)
{
    switch (alignment) {
    case TextTableBlockVerticalAlignment::Top:
        return NSTextBlockTopAlignment;
    case TextTableBlockVerticalAlignment::Middle:
        return NSTextBlockMiddleAlignment;
    case TextTableBlockVerticalAlignment::Bottom:
        return NSTextBlockBottomAlignment;
    case TextTableBlockVerticalAlignment::Baseline:
        return NSTextBlockBaselineAlignment;
    }
    ASSERT_NOT_REACHED();
    return NSTextBlockTopAlignment;
}

inline static NSTextTableLayoutAlgorithm reconstructNSTextTableLayoutAlgorithm(TextTableLayoutAlgorithm layout)
{
    switch (layout) {
    case TextTableLayoutAlgorithm::Automatic:
        return NSTextTableAutomaticLayoutAlgorithm;
    case TextTableLayoutAlgorithm::Fixed:
        return NSTextTableFixedLayoutAlgorithm;
    }
    ASSERT_NOT_REACHED();
    return NSTextTableAutomaticLayoutAlgorithm;
}

inline static NSWritingDirection reconstructNSWritingDirection(ParagraphStyleWritingDirection writingDirection)
{
    switch (writingDirection) {
    case ParagraphStyleWritingDirection::LeftToRight:
        return NSWritingDirectionLeftToRight;
    case ParagraphStyleWritingDirection::RightToLeft:
        return NSWritingDirectionRightToLeft;
    case ParagraphStyleWritingDirection::Natural:
        return NSWritingDirectionNatural;
    }
    ASSERT_NOT_REACHED();
    return NSWritingDirectionLeftToRight;
}

inline static RetainPtr<NSParagraphStyle> reconstructStyle(const ParagraphStyle& style, IdentifierToTableMap& tables, IdentifierToTableBlockMap& tableBlocks, IdentifierToListMap& lists)
{
    for (const auto& item : style.textLists) {
        lists.ensure(item.thisID, [&] {
            RetainPtr list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:item.markerFormat options:0]);
            [list setStartingItemNumber:item.startingItemNumber];
            return list;
        });
    }

    for (const auto& item : style.textTables) {
        tables.ensure(item.thisID, [&] {
            RetainPtr table = adoptNS([PlatformNSTextTable new]);
            [table setNumberOfColumns:item.numberOfColumns];
            [table setLayoutAlgorithm:reconstructNSTextTableLayoutAlgorithm(item.layout)];
            [table setCollapsesBorders:item.collapsesBorders];
            [table setHidesEmptyCells:item.hidesEmptyCells];
            configureNSTextBlockFromParagraphStyleCommonTableAttributes(table.get(), item);
            return table;
        });
    };

    for (const auto& item : style.textTableBlocks) {
        tableBlocks.ensure(item.thisID, [&] {
            auto foundTable = tables.find(item.tableID);
            if (foundTable == tables.end()) {
                RELEASE_LOG_ERROR(Editing, "Table not found when trying to reconstruct NSParagraphStyle");
                return adoptNS([PlatformNSTextTableBlock new]);
            }

            RetainPtr tableBlock = adoptNS([[PlatformNSTextTableBlock alloc] initWithTable:foundTable->value.get() startingRow:item.startingRow rowSpan:item.rowSpan startingColumn:item.startingColumn columnSpan:item.columnSpan]);
            [tableBlock setVerticalAlignment:reconstructNSTextBlockVerticalAlignment(item.verticalAlignment)];
            configureNSTextBlockFromParagraphStyleCommonTableAttributes(tableBlock.get(), item);
            return tableBlock;
        });
    };

    RetainPtr<NSMutableParagraphStyle> mutableStyle = adoptNS([[PlatformNSParagraphStyle defaultParagraphStyle] mutableCopy]);
    [mutableStyle setDefaultTabInterval:style.defaultTabInterval];
    [mutableStyle setHyphenationFactor:style.hyphenationFactor];
    [mutableStyle setFirstLineHeadIndent:style.firstLineHeadIndent];
    [mutableStyle setHeadIndent:style.headIndent];
    [mutableStyle setHeaderLevel:style.headerLevel];
    [mutableStyle setTailIndent:style.tailIndent];
    [mutableStyle setParagraphSpacing:style.paragraphSpacing];
    [mutableStyle setAlignment:reconstructNSTextAlignment(style.alignment)];
    [mutableStyle setBaseWritingDirection:reconstructNSWritingDirection(style.writingDirection)];
    [mutableStyle setTabStops:adoptNS([NSMutableArray new]).get()];

    if (!style.textTableBlockIDs.isEmpty()) {
        RetainPtr blocks = createNSArray(style.textTableBlockIDs, [&] (auto& object) -> id {
            return tableBlocks.get(object).get();
        });
        [mutableStyle setTextBlocks:blocks.get()];
    }

    if (!style.textListIDs.isEmpty()) {
        RetainPtr textLists = createNSArray(style.textListIDs, [&] (auto& object) -> id {
            return lists.get(object).get();
        });
        [mutableStyle setTextLists:textLists.get()];
    }

    if (!style.textTabs.isEmpty()) {
        for (const auto& item : style.textTabs)
            [mutableStyle addTabStop:adoptNS([[PlatformNSTextTab alloc] initWithTextAlignment:reconstructNSTextAlignment(item.alignment) location:item.location options:@{ }]).get()];
    }

    return mutableStyle;
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)

static MultiRepresentationHEICAttachmentData toMultiRepresentationHEICAttachmentData(NSAdaptiveImageGlyph *attachment)
{
    MultiRepresentationHEICAttachmentData attachmentData;
    attachmentData.identifier = attachment.contentIdentifier;
    attachmentData.description = attachment.contentDescription;

#if HAVE(NS_EMOJI_IMAGE_STRIKE_PROVENANCE)
    if (RetainPtr<NSDictionary<NSString *, NSString *>> provenance = attachment.strikes.firstObject.provenance) {
        attachmentData.credit = [provenance objectForKey:(__bridge NSString *)kCGImagePropertyIPTCCredit];
        attachmentData.digitalSourceType = [provenance objectForKey:(__bridge NSString *)kCGImagePropertyIPTCExtDigitalSourceType];
    }
#endif

    for (NSEmojiImageStrike *strike in attachment.strikes) {
        MultiRepresentationHEICAttachmentSingleImage image;
        RefPtr nativeImage = NativeImage::create(strike.cgImage);
        image.image = BitmapImage::create(WTFMove(nativeImage));
        image.size = FloatSize { strike.alignmentInset };
        attachmentData.images.append(image);
    }

    if (auto data = bridge_cast([attachment imageContent]))
        attachmentData.data = data;

    return attachmentData;
}

static RetainPtr<NSAdaptiveImageGlyph> toWebMultiRepresentationHEICAttachment(const MultiRepresentationHEICAttachmentData& attachmentData)
{
    if (RetainPtr<NSData> data = attachmentData.data ? bridge_cast((attachmentData.data).get()) : nil) {
        RetainPtr attachment = adoptNS([[PlatformNSAdaptiveImageGlyph alloc] initWithImageContent:data.get()]);
        if (attachment)
            return attachment;
    }

    NSString *identifier = attachmentData.identifier;
    NSString *description = attachmentData.description;
    if (!description.length)
        description = @"Apple Emoji";

#if HAVE(NS_EMOJI_IMAGE_STRIKE_PROVENANCE)
    RetainPtr<NSMutableDictionary<NSString *, NSString *>> provenanceInfo = [NSMutableDictionary dictionaryWithCapacity:2];

    NSString *credit = attachmentData.credit;
    NSString *digitalSourceType = attachmentData.digitalSourceType;
    if (identifier.length && digitalSourceType.length) {
        [provenanceInfo setObject:credit forKey:(__bridge NSString *)kCGImagePropertyIPTCCredit];
        [provenanceInfo setObject:digitalSourceType forKey:(__bridge NSString *)kCGImagePropertyIPTCExtDigitalSourceType];
    }
#endif

    NSMutableArray *images = [NSMutableArray arrayWithCapacity:attachmentData.images.size()];
    for (auto& singleImage : attachmentData.images) {
#if HAVE(NS_EMOJI_IMAGE_STRIKE_PROVENANCE)
        RetainPtr strike = adoptNS([[CTEmojiImageStrike alloc] initWithImage:singleImage.image->nativeImage()->platformImage().get() alignmentInset:singleImage.size provenanceInfo:provenanceInfo.get()]);
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        RetainPtr strike = adoptNS([[CTEmojiImageStrike alloc] initWithImage:singleImage.image->nativeImage()->platformImage().get() alignmentInset:singleImage.size]);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

        [images addObject:strike.get()];
    }

    if (![images count])
        return nil;

    RetainPtr asset = adoptNS([[CTEmojiImageAsset alloc] initWithContentIdentifier:identifier shortDescription:description strikeImages:images]);
    if (![asset imageData])
        return nil;

    return adoptNS([[PlatformNSAdaptiveImageGlyph alloc] initWithImageContent:[asset imageData]]);
}

#endif

static RetainPtr<id> toNSObject(const AttributedString::AttributeValue& value, IdentifierToTableMap& tables, IdentifierToTableBlockMap& tableBlocks, IdentifierToListMap& lists)
{
    return WTF::switchOn(value.value, [] (double value) -> RetainPtr<id> {
        return adoptNS([[NSNumber alloc] initWithDouble:value]);
    }, [] (const String& value) -> RetainPtr<id> {
        return (NSString *)value;
    }, [&] (const ParagraphStyle& value) -> RetainPtr<id> {
        return reconstructStyle(value, tables, tableBlocks, lists);
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
    }, [] (const TextAttachmentMissingImage& value) -> RetainPtr<id> {
        UNUSED_PARAM(value);
        RetainPtr<NSTextAttachment> attachment = adoptNS([[PlatformNSTextAttachment alloc] initWithData:nil ofType:nil]);
        attachment.get().image = webCoreTextAttachmentMissingPlatformImage();
        return attachment;
    }, [] (const TextAttachmentFileWrapper& value) -> RetainPtr<id> {
        RetainPtr<NSData> data = value.data ? bridge_cast((value.data).get()) : nil;

        RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
        if (!value.preferredFilename.isNull())
            [fileWrapper setPreferredFilename:filenameByFixingIllegalCharacters((NSString *)value.preferredFilename)];

        auto textAttachment = adoptNS([[PlatformNSTextAttachment alloc] initWithFileWrapper:fileWrapper.get()]);
        if (!value.accessibilityLabel.isNull())
            ((NSTextAttachment*)textAttachment.get()).accessibilityLabel = (NSString *)value.accessibilityLabel;

        return textAttachment;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    }, [] (const MultiRepresentationHEICAttachmentData& value) -> RetainPtr<id> {
        return toWebMultiRepresentationHEICAttachment(value);
#endif
    }, [] (const RetainPtr<NSShadow>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSDate>& value) -> RetainPtr<id> {
        return value;
    }, [] (const Ref<Font>& font) -> RetainPtr<id> {
        return (__bridge PlatformFont *)(font->getCTFont());
    }, [] (const AttributedString::ColorFromPlatformColor& value) -> RetainPtr<id> {
        return cocoaColor(value.color);
    }, [] (const AttributedString::ColorFromCGColor& value) -> RetainPtr<id> {
        return (__bridge id)cachedCGColor(value.color).get();
    });
}

static RetainPtr<NSDictionary> toNSDictionary(const HashMap<String, AttributedString::AttributeValue>& map, IdentifierToTableMap& tables, IdentifierToTableBlockMap& tableBlocks, IdentifierToListMap& lists)
{
    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& pair : map) {
        if (auto nsObject = toNSObject(pair.value, tables, tableBlocks, lists))
            [result setObject:nsObject.get() forKey:(NSString *)pair.key];
    }
    return result;
}

bool AttributedString::isNull() const
{
    return string.isNull();
}

RetainPtr<NSDictionary> AttributedString::documentAttributesAsNSDictionary() const
{
    if (!documentAttributes)
        return nullptr;

    IdentifierToTableMap tables;
    IdentifierToTableBlockMap tableBlocks;
    IdentifierToListMap lists;
    return toNSDictionary(*documentAttributes, tables, tableBlocks, lists);
}

RetainPtr<NSAttributedString> AttributedString::nsAttributedString() const
{
    if (string.isNull())
        return nullptr;

    IdentifierToTableMap tables;
    IdentifierToTableBlockMap tableBlocks;
    IdentifierToListMap lists;
    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:(NSString *)string]);
    for (auto& pair : attributes) {
        auto& map = pair.second;
        auto& range = pair.first;
        [result addAttributes:toNSDictionary(map, tables, tableBlocks, lists).get() range:NSMakeRange(range.location, range.length)];
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
                result.append((NSString *)element);
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
                result.append([(NSNumber *)element doubleValue]);
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

inline static ParagraphStyleAlignment extractParagraphStyleAlignment(NSTextAlignment alignment)
{
    switch (alignment) {
    case NSTextAlignmentLeft:
        return ParagraphStyleAlignment::Left;
    case NSTextAlignmentRight:
        return ParagraphStyleAlignment::Right;
    case NSTextAlignmentCenter:
        return ParagraphStyleAlignment::Centre;
    case NSTextAlignmentJustified:
        return ParagraphStyleAlignment::Justified;
    case NSTextAlignmentNatural:
        return ParagraphStyleAlignment::Natural;
    }
    ASSERT_NOT_REACHED();
    return ParagraphStyleAlignment::Left;
}

inline static TextTableBlockVerticalAlignment extractTextTableBlockVerticalAlignment(NSTextBlockVerticalAlignment verticalAlignment)
{
    switch (verticalAlignment) {
    case NSTextBlockTopAlignment:
        return TextTableBlockVerticalAlignment::Top;
    case NSTextBlockMiddleAlignment:
        return TextTableBlockVerticalAlignment::Middle;
    case NSTextBlockBottomAlignment:
        return TextTableBlockVerticalAlignment::Bottom;
    case NSTextBlockBaselineAlignment:
        return TextTableBlockVerticalAlignment::Baseline;
    }
    ASSERT_NOT_REACHED();
    return TextTableBlockVerticalAlignment::Top;
}

inline static TextTableLayoutAlgorithm extractTextTableLayoutAlgorithm(NSTextTableLayoutAlgorithm layout)
{
    switch (layout) {
    case NSTextTableAutomaticLayoutAlgorithm:
        return TextTableLayoutAlgorithm::Automatic;
    case NSTextTableFixedLayoutAlgorithm:
        return TextTableLayoutAlgorithm::Fixed;
    }
    ASSERT_NOT_REACHED();
    return TextTableLayoutAlgorithm::Automatic;
}

inline static ParagraphStyleWritingDirection extractParagraphStyleWritingDirection(NSWritingDirection writingDirection)
{
    switch (writingDirection) {
    case NSWritingDirectionLeftToRight:
        return ParagraphStyleWritingDirection::LeftToRight;
    case NSWritingDirectionRightToLeft:
        return ParagraphStyleWritingDirection::RightToLeft;
    case NSWritingDirectionNatural:
        return ParagraphStyleWritingDirection::Natural;
    }
    return ParagraphStyleWritingDirection::LeftToRight;
}

inline static ParagraphStyle extractParagraphStyle(NSParagraphStyle *style, TableToIdentifierMap& tableIDs, TableBlockToIdentifierMap& tableBlockIDs, ListToIdentifierMap& listIDs)
{
    Vector<AttributedStringTextTableBlockID> sentTextTableBlockIDs;
    Vector<AttributedStringTextListID> sentTextListIDs;
    Vector<TextTableBlock> newTextTableBlocks;
    Vector<TextTable> newTextTables;
    Vector<ParagraphStyleTextList> newTextLists;
    Vector<TextTab> newTextTabs;

    for (NSTextList *list in style.textLists) {
        if (![list isKindOfClass:PlatformNSTextList])
            return { };

        auto listResult = listIDs.ensure(list, [] {
            return AttributedString::TextListID::generate();
        });
        auto listID = listResult.iterator->value;

        if (listResult.isNewEntry) {
            newTextLists.append(ParagraphStyleTextList {
                listID,
                list.markerFormat,
                list.startingItemNumber
            });
        }
        sentTextListIDs.append(listID);
    }

    for (NSTextTableBlock* item in style.textBlocks) {
        if (![item isKindOfClass:PlatformNSTextTableBlock])
            return { };

        auto tableBlock = static_cast<NSTextTableBlock *>(item);
        if (!tableBlock.table)
            return { };

        auto tableBlockEnsureResult = tableBlockIDs.ensure(tableBlock, [&] {
            return AttributedString::TextTableBlockID::generate();
        });
        auto tableBlockID = tableBlockEnsureResult.iterator->value;

        sentTextTableBlockIDs.append(tableBlockID);

        auto nsTable = tableBlock.table;
        auto tableEnsureResults = tableIDs.ensure(nsTable, [&] {
            return AttributedString::TextTableID::generate();
        });
        auto tableID = tableEnsureResults.iterator->value;

        if (tableEnsureResults.isNewEntry) {
            newTextTables.append(TextTable {
                {
                    [nsTable valueForDimension:NSTextBlockWidth],
                    [nsTable valueForDimension:NSTextBlockMinimumWidth],
                    [nsTable valueForDimension:NSTextBlockMaximumWidth],
                    [nsTable valueForDimension:NSTextBlockMinimumHeight],
                    [nsTable valueForDimension:NSTextBlockMaximumHeight],

                    [nsTable widthForLayer:NSTextBlockPadding edge:NSMinXEdge],
                    [nsTable widthForLayer:NSTextBlockPadding edge:NSMinYEdge],
                    [nsTable widthForLayer:NSTextBlockPadding edge:NSMaxXEdge],
                    [nsTable widthForLayer:NSTextBlockPadding edge:NSMaxYEdge],

                    [nsTable widthForLayer:NSTextBlockBorder edge:NSMinXEdge],
                    [nsTable widthForLayer:NSTextBlockBorder edge:NSMinYEdge],
                    [nsTable widthForLayer:NSTextBlockBorder edge:NSMaxXEdge],
                    [nsTable widthForLayer:NSTextBlockBorder edge:NSMaxYEdge],

                    [nsTable widthForLayer:NSTextBlockMargin edge:NSMinXEdge],
                    [nsTable widthForLayer:NSTextBlockMargin edge:NSMinYEdge],
                    [nsTable widthForLayer:NSTextBlockMargin edge:NSMaxXEdge],
                    [nsTable widthForLayer:NSTextBlockMargin edge:NSMaxYEdge],

                    [nsTable backgroundColor],
                    [nsTable borderColorForEdge:NSMinXEdge],
                    [nsTable borderColorForEdge:NSMinYEdge],
                    [nsTable borderColorForEdge:NSMaxXEdge],
                    [nsTable borderColorForEdge:NSMaxYEdge]
                },
                tableID,
                [nsTable numberOfColumns],
                extractTextTableLayoutAlgorithm([nsTable layoutAlgorithm]),
                !![nsTable collapsesBorders],
                !![nsTable hidesEmptyCells],
            });
        }

        if (tableBlockEnsureResult.isNewEntry) {
            newTextTableBlocks.append(TextTableBlock {
                {
                    [item valueForDimension:NSTextBlockWidth],
                    [item valueForDimension:NSTextBlockMinimumWidth],
                    [item valueForDimension:NSTextBlockMaximumWidth],
                    [item valueForDimension:NSTextBlockMinimumHeight],
                    [item valueForDimension:NSTextBlockMaximumHeight],

                    [item widthForLayer:NSTextBlockPadding edge:NSMinXEdge],
                    [item widthForLayer:NSTextBlockPadding edge:NSMinYEdge],
                    [item widthForLayer:NSTextBlockPadding edge:NSMaxXEdge],
                    [item widthForLayer:NSTextBlockPadding edge:NSMaxYEdge],

                    [item widthForLayer:NSTextBlockBorder edge:NSMinXEdge],
                    [item widthForLayer:NSTextBlockBorder edge:NSMinYEdge],
                    [item widthForLayer:NSTextBlockBorder edge:NSMaxXEdge],
                    [item widthForLayer:NSTextBlockBorder edge:NSMaxYEdge],

                    [item widthForLayer:NSTextBlockMargin edge:NSMinXEdge],
                    [item widthForLayer:NSTextBlockMargin edge:NSMinYEdge],
                    [item widthForLayer:NSTextBlockMargin edge:NSMaxXEdge],
                    [item widthForLayer:NSTextBlockMargin edge:NSMaxYEdge],

                    [item backgroundColor],
                    [item borderColorForEdge:NSMinXEdge],
                    [item borderColorForEdge:NSMinYEdge],
                    [item borderColorForEdge:NSMaxXEdge],
                    [item borderColorForEdge:NSMaxYEdge]
                },
                tableBlockID,
                tableID,
                [item startingRow],
                [item rowSpan],
                [item startingColumn],
                [item columnSpan],
                extractTextTableBlockVerticalAlignment([item verticalAlignment]),
            });
        }
    };

    auto tabStops = [style tabStops];
    newTextTabs.reserveInitialCapacity([tabStops count]);
    for (NSTextTab *textTab : tabStops) {
        newTextTabs.append(TextTab {
            [textTab location],
            extractParagraphStyleAlignment([textTab alignment])
        });
    }

    return ParagraphStyle {
        [style defaultTabInterval],
        extractParagraphStyleAlignment([style alignment]),
        extractParagraphStyleWritingDirection([style baseWritingDirection]),
        [style hyphenationFactor],
        [style firstLineHeadIndent],
        [style headIndent],
        [style headerLevel],
        [style tailIndent],
        [style paragraphSpacing],
        WTFMove(sentTextTableBlockIDs),
        WTFMove(sentTextListIDs),
        WTFMove(newTextTableBlocks),
        WTFMove(newTextTables),
        WTFMove(newTextLists),
        WTFMove(newTextTabs)
    };
}

static std::optional<AttributedString::AttributeValue> extractValue(id value, TableToIdentifierMap& tableIDs, TableBlockToIdentifierMap& tableBlockIDs, ListToIdentifierMap& listIDs)
{
    if (CFGetTypeID((CFTypeRef)value) == CGColorGetTypeID())
        return { { AttributedString::ColorFromCGColor  { Color::createAndPreserveColorSpace((CGColorRef) value) } } };
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
        return { { extractParagraphStyle(style, tableIDs, tableBlockIDs, listIDs) } };
    }
    if ([value isKindOfClass:PlatformNSPresentationIntent])
        return { { { RetainPtr { (NSPresentationIntent *)value } } } };
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    if ([value isKindOfClass:PlatformNSAdaptiveImageGlyph]) {
        auto attachment = static_cast<NSAdaptiveImageGlyph *>(value);
        return { { toMultiRepresentationHEICAttachmentData(attachment) } };
    }
#endif
    if ([value isKindOfClass:PlatformNSTextAttachment]) {
        if ([value image] == webCoreTextAttachmentMissingPlatformImage())
            return { { TextAttachmentMissingImage() } };
        TextAttachmentFileWrapper textAttachment;
        if (auto accessibilityLabel = [value accessibilityLabel])
            textAttachment.accessibilityLabel = accessibilityLabel;
#if !PLATFORM(IOS_FAMILY)
        textAttachment.ignoresOrientation = [value ignoresOrientation];
#endif
        if (auto fileWrapper = [value fileWrapper]) {
            if (auto data = bridge_cast([fileWrapper regularFileContents]))
                textAttachment.data = data;
            if (auto preferredFilename = [fileWrapper preferredFilename])
                textAttachment.preferredFilename = preferredFilename;
        }
        return { { textAttachment } };
    }
    if ([value isKindOfClass:PlatformFontClass])
        return { { { Font::create(FontPlatformData((__bridge CTFontRef)value, [(PlatformFont *)value pointSize])) } } };
    if ([value isKindOfClass:PlatformColorClass])
        return { { AttributedString::ColorFromPlatformColor { colorFromCocoaColor((PlatformColor *)value) } } };
    if (value) {
        RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed for class <%@>", NSStringFromClass([value class]));
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    return std::nullopt;
}

static HashMap<String, AttributedString::AttributeValue> extractDictionary(NSDictionary *dictionary, TableToIdentifierMap& tableIDs, TableBlockToIdentifierMap& tableBlockIDs, ListToIdentifierMap& listIDs)
{
    HashMap<String, AttributedString::AttributeValue> result;
    [dictionary enumerateKeysAndObjectsUsingBlock:[&](id key, id value, BOOL *) {
        if (![key isKindOfClass:NSString.class]) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto extractedValue = extractValue(value, tableIDs, tableBlockIDs, listIDs);
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
    __block TableBlockToIdentifierMap tableBlockIDs;
    __block ListToIdentifierMap listIDs;
    __block AttributedString result;
    result.string = [string string];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired usingBlock: ^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        result.attributes.append({ Range { range.location, range.length }, extractDictionary(attributes, tableIDs, tableBlockIDs, listIDs) });
    }];
    if (dictionary)
        result.documentAttributes = extractDictionary(dictionary.get(), tableIDs, tableBlockIDs, listIDs);
    return { WTFMove(result) };
}

}
