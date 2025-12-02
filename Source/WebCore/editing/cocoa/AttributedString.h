/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#import <WebCore/Color.h>
#import <WebCore/FontPlatformData.h>
#import <WebCore/TextAttachmentForSerialization.h>
#import <pal/spi/cf/CoreTextSPI.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/Platform.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>

#if PLATFORM(MAC)
#define PlatformColor                   NSColor
#define PlatformColorClass              NSColor.class
#define PlatformFont                    NSFont
#define PlatformFontClass               NSFont.class
#define PlatformImageClass              NSImage
#define PlatformNSColorClass            NSColor
#define PlatformNSParagraphStyle        NSParagraphStyle.class
#define PlatformNSPresentationIntent    NSPresentationIntent.class
#define PlatformNSShadow                NSShadow.class
#define PlatformNSTextAttachment        NSTextAttachment.class
#define PlatformNSTextList              NSTextList.class
#define PlatformNSTextTab               NSTextTab
#define PlatformNSTextTable             NSTextTable
#define PlatformNSTextTableBlock        NSTextTableBlock.class
#else
#define PlatformColor                   UIColor
#define PlatformColorClass              PAL::getUIColorClassSingleton()
#define PlatformFont                    UIFont
#define PlatformFontClass               PAL::getUIFontClassSingleton()
#define PlatformImageClass              PAL::getUIImageClassSingleton()
#define PlatformNSColorClass            getNSColorClassSingleton()
#define PlatformNSParagraphStyle        PAL::getNSParagraphStyleClassSingleton()
#define PlatformNSPresentationIntent    PAL::getNSPresentationIntentClassSingleton()
#define PlatformNSShadow                PAL::getNSShadowClassSingleton()
#define PlatformNSTextAttachment        getNSTextAttachmentClassSingleton()
#define PlatformNSTextList              getNSTextListClassSingleton()
#define PlatformNSTextTab               getNSTextTabClassSingleton()
#define PlatformNSTextTable             getNSTextTableClassSingleton()
#define PlatformNSTextTableBlock        getNSTextTableBlockClassSingleton()
#endif

OBJC_CLASS NSAttributedString;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSParagraphStyle;
OBJC_CLASS NSPresentationIntent;
OBJC_CLASS NSShadow;
OBJC_CLASS NSTextAttachment;
OBJC_CLASS PlatformColor;

namespace WebCore {

class Font;

struct AttributedStringTextTableIDType;
using AttributedStringTextTableID = ObjectIdentifier<AttributedStringTextTableIDType>;

struct AttributedStringTextTableBlockIDType;
using AttributedStringTextTableBlockID = ObjectIdentifier<AttributedStringTextTableBlockIDType>;

struct AttributedStringTextListIDType;
using AttributedStringTextListID = ObjectIdentifier<AttributedStringTextListIDType>;

enum class TextTableLayoutAlgorithm : bool {
    Automatic,
    Fixed
};

enum class TextTableBlockVerticalAlignment : uint8_t {
    Top,
    Middle,
    Bottom,
    Baseline,
};

enum class ParagraphStyleAlignment : uint8_t {
    Natural,
    Left,
    Right,
    Centre,
    Justified,
};

enum class ParagraphStyleWritingDirection: uint8_t {
    Natural,
    LeftToRight,
    RightToLeft,
};

struct ParagraphStyleTextList {
    AttributedStringTextListID thisID;
    String markerFormat;
    int64_t startingItemNumber { 0 };
};

struct ParagraphStyleCommonTableAttributes {
    double width { 0 };
    double minimumWidth { 0 };
    double maximumWidth { 0 };
    double minimumHeight { 0 };
    double maximumHeight { 0 };

    double paddingMinXEdge { 0 };
    double paddingMinYEdge { 0 };
    double paddingMaxXEdge { 0 };
    double paddingMaxYEdge { 0 };

    double borderMinXEdge { 0 };
    double borderMinYEdge { 0 };
    double borderMaxXEdge { 0 };
    double borderMaxYEdge { 0 };

    double marginMinXEdge { 0 };
    double marginMinYEdge { 0 };
    double marginMaxXEdge { 0 };
    double marginMaxYEdge { 0 };

    RetainPtr<PlatformColor> backgroundColor;
    RetainPtr<PlatformColor> borderMinXEdgeColor;
    RetainPtr<PlatformColor> borderMinYEdgeColor;
    RetainPtr<PlatformColor> borderMaxXEdgeColor;
    RetainPtr<PlatformColor> borderMaxYEdgeColor;
};

struct TextTable : ParagraphStyleCommonTableAttributes {
    AttributedStringTextTableID thisID;
    uint64_t numberOfColumns { 0 };
    TextTableLayoutAlgorithm layout { TextTableLayoutAlgorithm::Automatic };
    bool collapsesBorders { false };
    bool hidesEmptyCells { false };
};

struct TextTableBlock : ParagraphStyleCommonTableAttributes {
    AttributedStringTextTableBlockID thisID;
    AttributedStringTextTableID tableID;
    int64_t startingRow { 0 };
    int64_t rowSpan { 1 };
    int64_t startingColumn { 0 };
    int64_t columnSpan { 1 };
    TextTableBlockVerticalAlignment verticalAlignment { TextTableBlockVerticalAlignment::Top };
};

struct TextTab {
    double location { 0 };
    ParagraphStyleAlignment alignment { ParagraphStyleAlignment::Natural };
};

struct ParagraphStyle {
    double defaultTabInterval { 36 };
    ParagraphStyleAlignment alignment { ParagraphStyleAlignment::Natural };
    ParagraphStyleWritingDirection writingDirection { ParagraphStyleWritingDirection::Natural };
    float hyphenationFactor { 0 };
    double firstLineHeadIndent { 0 };
    double headIndent { 0 };
    int64_t headerLevel { 0 };
    double tailIndent { 0 };
    double paragraphSpacing { 0 };
    Vector<AttributedStringTextTableBlockID> textTableBlockIDs;
    Vector<AttributedStringTextListID> textListIDs;
    Vector<TextTableBlock> textTableBlocks;
    Vector<TextTable> textTables;
    Vector<ParagraphStyleTextList> textLists;
    Vector<TextTab> textTabs;
};

struct WEBCORE_EXPORT AttributedString {
    struct Range {
        uint64_t location { 0 };
        uint64_t length { 0 };
    };

    using TextTableID = AttributedStringTextTableID;
    using TextTableBlockID = AttributedStringTextTableBlockID;
    using TextListID = AttributedStringTextListID;

    struct ColorFromCGColor {
        Color color;
    };

    struct ColorFromPlatformColor {
        Color color;
    };

    struct AttributeValue {

        using AttributeType = Variant<
            double,
            String,
            URL,
            InstalledFont,
            Vector<String>,
            Vector<double>,
            ParagraphStyle,
            RetainPtr<NSPresentationIntent>,
            RetainPtr<NSShadow>,
            RetainPtr<NSDate>,
            ColorFromCGColor,
            ColorFromPlatformColor,
#if ENABLE(MULTI_REPRESENTATION_HEIC)
            MultiRepresentationHEICAttachmentData,
#endif
            TextAttachmentFileWrapper,
            TextAttachmentMissingImage
            >;

        AttributeType value;
    };

    String string;
    Vector<std::pair<Range, HashMap<String, AttributeValue>>> attributes;
    std::optional<HashMap<String, AttributeValue>> documentAttributes;

    AttributedString();
    AttributedString(String&&, Vector<std::pair<Range, HashMap<String, AttributeValue>>>&&, std::optional<HashMap<String, AttributeValue>>&&);
    AttributedString(AttributedString&&);
    AttributedString(const AttributedString&);
    AttributedString& operator=(AttributedString&&);
    AttributedString& operator=(const AttributedString&);
    ~AttributedString();

    static AttributedString fromNSAttributedStringAndDocumentAttributes(RetainPtr<NSAttributedString>&&, RetainPtr<NSDictionary>&& documentAttributes);
    static AttributedString fromNSAttributedString(RetainPtr<NSAttributedString>&&);
    static bool rangesAreSafe(const String&, const Vector<std::pair<Range, HashMap<String, AttributeValue>>>&);
    RetainPtr<NSDictionary> documentAttributesAsNSDictionary() const;
    RetainPtr<NSAttributedString> nsAttributedString() const;
    bool isNull() const;
};

} // namespace WebCore
