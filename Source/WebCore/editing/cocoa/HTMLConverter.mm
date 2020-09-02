/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
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
#import "HTMLConverter.h"

#import "ArchiveResource.h"
#import "CSSComputedStyleDeclaration.h"
#import "CSSParser.h"
#import "CSSPrimitiveValue.h"
#import "CachedImage.h"
#import "CharacterData.h"
#import "ColorCocoa.h"
#import "ColorMac.h"
#import "ComposedTreeIterator.h"
#import "Document.h"
#import "DocumentLoader.h"
#import "Editing.h"
#import "Element.h"
#import "ElementTraversal.h"
#import "File.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "HTMLAttachmentElement.h"
#import "HTMLElement.h"
#import "HTMLFrameElement.h"
#import "HTMLIFrameElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLMetaElement.h"
#import "HTMLNames.h"
#import "HTMLOListElement.h"
#import "HTMLParserIdioms.h"
#import "HTMLTableCellElement.h"
#import "HTMLTextAreaElement.h"
#import "LoaderNSURLExtras.h"
#import "RenderImage.h"
#import "RenderText.h"
#import "StyleProperties.h"
#import "StyledElement.h"
#import "TextIterator.h"
#import "VisibleSelection.h"
#import <objc/runtime.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/ASCIICType.h>
#import <wtf/text/StringBuilder.h>

#if PLATFORM(IOS_FAMILY)

#import "WAKAppKitStubs.h"
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/UIKitSPI.h>

SOFT_LINK_CLASS(UIFoundation, NSColor)
SOFT_LINK_CLASS(UIFoundation, NSShadow)
SOFT_LINK_CLASS(UIFoundation, NSTextAttachment)
SOFT_LINK_CLASS(UIFoundation, NSMutableParagraphStyle)
SOFT_LINK_CLASS(UIFoundation, NSParagraphStyle)
SOFT_LINK_CLASS(UIFoundation, NSTextList)
SOFT_LINK_CLASS(UIFoundation, NSTextBlock)
SOFT_LINK_CLASS(UIFoundation, NSTextTableBlock)
SOFT_LINK_CLASS(UIFoundation, NSTextTable)
SOFT_LINK_CLASS(UIFoundation, NSTextTab)

#define PlatformNSShadow            getNSShadowClass()
#define PlatformNSTextAttachment    getNSTextAttachmentClass()
#define PlatformNSParagraphStyle    getNSParagraphStyleClass()
#define PlatformNSTextList          getNSTextListClass()
#define PlatformNSTextTableBlock    getNSTextTableBlockClass()
#define PlatformNSTextTable         getNSTextTableClass()
#define PlatformNSTextTab           getNSTextTabClass()
#define PlatformColor               UIColor
#define PlatformColorClass          PAL::getUIColorClass()
#define PlatformNSColorClass        getNSColorClass()
#define PlatformFont                UIFont
#define PlatformFontClass           PAL::getUIFontClass()
#define PlatformImageClass          PAL::getUIImageClass()

#else

#define PlatformNSShadow            NSShadow
#define PlatformNSTextAttachment    NSTextAttachment
#define PlatformNSParagraphStyle    NSParagraphStyle
#define PlatformNSTextList          NSTextList
#define PlatformNSTextTableBlock    NSTextTableBlock
#define PlatformNSTextTable         NSTextTable
#define PlatformNSTextTab           NSTextTab
#define PlatformColor               NSColor
#define PlatformColorClass          NSColor
#define PlatformNSColorClass        NSColor
#define PlatformFont                NSFont
#define PlatformFontClass           NSFont
#define PlatformImageClass          NSImage

#endif

using namespace WebCore;
using namespace HTMLNames;

#if PLATFORM(IOS_FAMILY)

enum {
    NSTextBlockAbsoluteValueType    = 0,    // Absolute value in points
    NSTextBlockPercentageValueType  = 1     // Percentage value (out of 100)
};
typedef NSUInteger NSTextBlockValueType;

enum {
    NSTextBlockWidth            = 0,
    NSTextBlockMinimumWidth     = 1,
    NSTextBlockMaximumWidth     = 2,
    NSTextBlockHeight           = 4,
    NSTextBlockMinimumHeight    = 5,
    NSTextBlockMaximumHeight    = 6
};
typedef NSUInteger NSTextBlockDimension;

enum {
    NSTextBlockPadding  = -1,
    NSTextBlockBorder   =  0,
    NSTextBlockMargin   =  1
};
typedef NSInteger NSTextBlockLayer;

enum {
    NSTextTableAutomaticLayoutAlgorithm = 0,
    NSTextTableFixedLayoutAlgorithm     = 1
};
typedef NSUInteger NSTextTableLayoutAlgorithm;

enum {
    NSTextBlockTopAlignment         = 0,
    NSTextBlockMiddleAlignment      = 1,
    NSTextBlockBottomAlignment      = 2,
    NSTextBlockBaselineAlignment    = 3
};
typedef NSUInteger NSTextBlockVerticalAlignment;

enum {
    NSEnterCharacter                = 0x0003,
    NSBackspaceCharacter            = 0x0008,
    NSTabCharacter                  = 0x0009,
    NSNewlineCharacter              = 0x000a,
    NSFormFeedCharacter             = 0x000c,
    NSCarriageReturnCharacter       = 0x000d,
    NSBackTabCharacter              = 0x0019,
    NSDeleteCharacter               = 0x007f,
    NSLineSeparatorCharacter        = 0x2028,
    NSParagraphSeparatorCharacter   = 0x2029,
};

enum {
    NSLeftTabStopType = 0,
    NSRightTabStopType,
    NSCenterTabStopType,
    NSDecimalTabStopType
};
typedef NSUInteger NSTextTabType;

@interface NSColor : UIColor
+ (id)colorWithCalibratedRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
@end

@interface NSTextTab ()
- (id)initWithType:(NSTextTabType)type location:(CGFloat)loc;
@end

@interface NSParagraphStyle ()
- (void)setHeaderLevel:(NSInteger)level;
- (void)setTextBlocks:(NSArray *)array;
@end

@interface NSTextBlock : NSObject
- (void)setValue:(CGFloat)val type:(NSTextBlockValueType)type forDimension:(NSTextBlockDimension)dimension;
- (void)setWidth:(CGFloat)val type:(NSTextBlockValueType)type forLayer:(NSTextBlockLayer)layer edge:(NSRectEdge)edge;
- (void)setBackgroundColor:(UIColor *)color;
- (UIColor *)backgroundColor;
- (void)setBorderColor:(UIColor *)color forEdge:(NSRectEdge)edge;
- (void)setBorderColor:(UIColor *)color;        // Convenience method sets all edges at once
- (void)setVerticalAlignment:(NSTextBlockVerticalAlignment)alignment;
@end

@interface NSTextTable : NSTextBlock
- (void)setNumberOfColumns:(NSUInteger)numCols;
- (void)setCollapsesBorders:(BOOL)flag;
- (void)setHidesEmptyCells:(BOOL)flag;
- (void)setLayoutAlgorithm:(NSTextTableLayoutAlgorithm)algorithm;
- (NSUInteger)numberOfColumns;
- (void)release;
@end

@interface NSTextTableBlock : NSTextBlock
- (id)initWithTable:(NSTextTable *)table startingRow:(NSInteger)row rowSpan:(NSInteger)rowSpan startingColumn:(NSInteger)col columnSpan:(NSInteger)colSpan;     // Designated initializer
- (NSInteger)startingColumn;
- (NSInteger)startingRow;
- (NSUInteger)numberOfColumns;
- (NSInteger)columnSpan;
- (NSInteger)rowSpan;
@end

#else
static NSFileWrapper *fileWrapperForURL(DocumentLoader *, NSURL *);
static RetainPtr<NSFileWrapper> fileWrapperForElement(HTMLImageElement&);

@interface NSTextAttachment (WebCoreNSTextAttachment)
- (void)setIgnoresOrientation:(BOOL)flag;
- (void)setBounds:(CGRect)bounds;
- (BOOL)ignoresOrientation;
@end

#endif

// Additional control Unicode characters
const unichar WebNextLineCharacter = 0x0085;

static const CGFloat defaultFontSize = 12;
static const CGFloat minimumFontSize = 1;

class HTMLConverterCaches {
    WTF_MAKE_FAST_ALLOCATED;
public:
    String propertyValueForNode(Node&, CSSPropertyID );
    bool floatPropertyValueForNode(Node&, CSSPropertyID, float&);
    Color colorPropertyValueForNode(Node&, CSSPropertyID);

    bool isBlockElement(Element&);
    bool elementHasOwnBackgroundColor(Element&);

    RefPtr<CSSValue> computedStylePropertyForElement(Element&, CSSPropertyID);
    RefPtr<CSSValue> inlineStylePropertyForElement(Element&, CSSPropertyID);

    Node* cacheAncestorsOfStartToBeConverted(const Position&, const Position&);
    bool isAncestorsOfStartToBeConverted(Node& node) const { return m_ancestorsUnderCommonAncestor.contains(&node); }

private:
    HashMap<Element*, std::unique_ptr<ComputedStyleExtractor>> m_computedStyles;
    HashSet<Node*> m_ancestorsUnderCommonAncestor;
};

@interface NSTextList (WebCoreNSTextListDetails)
+ (NSDictionary *)_standardMarkerAttributesForAttributes:(NSDictionary *)attrs;
@end

@interface NSURL (WebCoreNSURLDetails)
// FIXME: What is the reason to use this Foundation method, and not +[NSURL URLWithString:relativeToURL:]?
+ (NSURL *)_web_URLWithString:(NSString *)string relativeToURL:(NSURL *)baseURL;
@end

@interface NSObject(WebMessageDocumentSimulation)
+ (void)document:(NSObject **)outDocument attachment:(NSTextAttachment **)outAttachment forURL:(NSURL *)url;
@end

class HTMLConverter {
public:
    explicit HTMLConverter(const SimpleRange&);
    ~HTMLConverter();

    AttributedString convert();

private:
    Position m_start;
    Position m_end;
    DocumentLoader* m_dataSource { nullptr };
    
    HashMap<RefPtr<Element>, RetainPtr<NSDictionary>> m_attributesForElements;
    HashMap<RetainPtr<CFTypeRef>, RefPtr<Element>> m_textTableFooters;
    HashMap<RefPtr<Element>, RetainPtr<NSDictionary>> m_aggregatedAttributesForElements;

    NSMutableAttributedString *_attrStr;
    NSMutableDictionary *_documentAttrs;
    NSURL *_baseURL;
    NSMutableArray *_textLists;
    NSMutableArray *_textBlocks;
    NSMutableArray *_textTables;
    NSMutableArray *_textTableSpacings;
    NSMutableArray *_textTablePaddings;
    NSMutableArray *_textTableRows;
    NSMutableArray *_textTableRowArrays;
    NSMutableArray *_textTableRowBackgroundColors;
    NSMutableDictionary *_fontCache;
    NSMutableArray *_writingDirectionArray;
    
    CGFloat _defaultTabInterval;
    NSUInteger _domRangeStartIndex;
    NSInteger _quoteLevel;

    std::unique_ptr<HTMLConverterCaches> _caches;

    struct {
        unsigned int isSoft:1;
        unsigned int reachedStart:1;
        unsigned int reachedEnd:1;
        unsigned int hasTrailingNewline:1;
        unsigned int pad:26;
    } _flags;
    
    PlatformColor *_colorForElement(Element&, CSSPropertyID);
    
    void _traverseNode(Node&, unsigned depth, bool embedded);
    void _traverseFooterNode(Element&, unsigned depth);
    
    NSDictionary *computedAttributesForElement(Element&);
    NSDictionary *attributesForElement(Element&);
    NSDictionary *aggregatedAttributesForAncestors(CharacterData&);
    NSDictionary* aggregatedAttributesForElementAndItsAncestors(Element&);

    Element* _blockLevelElementForNode(Node*);
    
    void _newParagraphForElement(Element&, NSString *tag, BOOL flag, BOOL suppressTrailingSpace);
    void _newLineForElement(Element&);
    void _newTabForElement(Element&);
    BOOL _addAttachmentForElement(Element&, NSURL *url, BOOL needsParagraph, BOOL usePlaceholder);
    void _addQuoteForElement(Element&, BOOL opening, NSInteger level);
    void _addValue(NSString *value, Element&);
    void _fillInBlock(NSTextBlock *block, Element&, PlatformColor *backgroundColor, CGFloat extraMargin, CGFloat extraPadding, BOOL isTable);
    
    BOOL _enterElement(Element&, BOOL embedded);
    BOOL _processElement(Element&, NSInteger depth);
    void _exitElement(Element&, NSInteger depth, NSUInteger startIndex);
    
    void _processHeadElement(Element&);
    void _processMetaElementWithName(NSString *name, NSString *content);
    
    void _addTableForElement(Element* tableElement);
    void _addTableCellForElement(Element* tableCellElement);
    void _addMarkersToList(NSTextList *list, NSRange range);
    void _processText(CharacterData&);
    void _adjustTrailingNewline();
};

HTMLConverter::HTMLConverter(const SimpleRange& range)
    : m_start(createLegacyEditingPosition(range.start))
    , m_end(createLegacyEditingPosition(range.end))
{
    _attrStr = [[NSMutableAttributedString alloc] init];
    _documentAttrs = [[NSMutableDictionary alloc] init];
    _baseURL = nil;
    _textLists = [[NSMutableArray alloc] init];
    _textBlocks = [[NSMutableArray alloc] init];
    _textTables = [[NSMutableArray alloc] init];
    _textTableSpacings = [[NSMutableArray alloc] init];
    _textTablePaddings = [[NSMutableArray alloc] init];
    _textTableRows = [[NSMutableArray alloc] init];
    _textTableRowArrays = [[NSMutableArray alloc] init];
    _textTableRowBackgroundColors = [[NSMutableArray alloc] init];
    _fontCache = [[NSMutableDictionary alloc] init];
    _writingDirectionArray = [[NSMutableArray alloc] init];

    _defaultTabInterval = 36;
    _domRangeStartIndex = 0;
    _quoteLevel = 0;
    
    _flags.isSoft = false;
    _flags.reachedStart = false;
    _flags.reachedEnd = false;
    
    _caches = makeUnique<HTMLConverterCaches>();
}

HTMLConverter::~HTMLConverter()
{
    [_attrStr release];
    [_documentAttrs release];
    [_textLists release];
    [_textBlocks release];
    [_textTables release];
    [_textTableSpacings release];
    [_textTablePaddings release];
    [_textTableRows release];
    [_textTableRowArrays release];
    [_textTableRowBackgroundColors release];
    [_fontCache release];
    [_writingDirectionArray release];
}

AttributedString HTMLConverter::convert()
{
    if (m_start > m_end)
        return { };

    Node* commonAncestorContainer = _caches->cacheAncestorsOfStartToBeConverted(m_start, m_end);
    ASSERT(commonAncestorContainer);

    m_dataSource = commonAncestorContainer->document().frame()->loader().documentLoader();

    Document& document = commonAncestorContainer->document();
    if (auto* body = document.bodyOrFrameset()) {
        if (PlatformColor *backgroundColor = _colorForElement(*body, CSSPropertyBackgroundColor))
            [_documentAttrs setObject:backgroundColor forKey:NSBackgroundColorDocumentAttribute];
    }

    _domRangeStartIndex = 0;
    _traverseNode(*commonAncestorContainer, 0, false /* embedded */);
    if (_domRangeStartIndex > 0 && _domRangeStartIndex <= [_attrStr length])
        [_attrStr deleteCharactersInRange:NSMakeRange(0, _domRangeStartIndex)];

    return { WTFMove(_attrStr), WTFMove(_documentAttrs) };
}

#if !PLATFORM(IOS_FAMILY)
// Returns the font to be used if the NSFontAttributeName doesn't exist
static NSFont *WebDefaultFont()
{
    static NSFont *defaultFont = nil;
    if (defaultFont)
        return defaultFont;

    NSFont *font = [NSFont fontWithName:@"Helvetica" size:12];
    if (!font)
        font = [NSFont systemFontOfSize:12];

    defaultFont = [font retain];
    return defaultFont;
}
#endif

static PlatformFont *_fontForNameAndSize(NSString *fontName, CGFloat size, NSMutableDictionary *cache)
{
    PlatformFont *font = [cache objectForKey:fontName];
#if PLATFORM(IOS_FAMILY)
    if (font)
        return [font fontWithSize:size];

    font = [PlatformFontClass fontWithName:fontName size:size];
#else
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    if (font) {
        font = [fontManager convertFont:font toSize:size];
        return font;
    }
    font = [fontManager fontWithFamily:fontName traits:0 weight:0 size:size];
#endif
    if (!font) {
#if PLATFORM(IOS_FAMILY)
        NSArray *availableFamilyNames = [PlatformFontClass familyNames];
#else
        NSArray *availableFamilyNames = [fontManager availableFontFamilies];
#endif
        NSRange dividingRange;
        NSRange dividingSpaceRange = [fontName rangeOfString:@" " options:NSBackwardsSearch];
        NSRange dividingDashRange = [fontName rangeOfString:@"-" options:NSBackwardsSearch];
        dividingRange = (0 < dividingSpaceRange.length && 0 < dividingDashRange.length) ? (dividingSpaceRange.location > dividingDashRange.location ? dividingSpaceRange : dividingDashRange) : (0 < dividingSpaceRange.length ? dividingSpaceRange : dividingDashRange);

        while (dividingRange.length > 0) {
            NSString *familyName = [fontName substringToIndex:dividingRange.location];
            if ([availableFamilyNames containsObject:familyName]) {
#if PLATFORM(IOS_FAMILY)
                NSString *faceName = [fontName substringFromIndex:(dividingRange.location + dividingRange.length)];
                NSArray *familyMemberFaceNames = [PlatformFontClass fontNamesForFamilyName:familyName];
                for (NSString *familyMemberFaceName in familyMemberFaceNames) {
                    if ([familyMemberFaceName compare:faceName options:NSCaseInsensitiveSearch] == NSOrderedSame) {
                        font = [PlatformFontClass fontWithName:familyMemberFaceName size:size];
                        break;
                    }
                }
                if (!font && [familyMemberFaceNames count])
                    font = [PlatformFontClass fontWithName:familyName size:size];
#else
                NSArray *familyMemberArray;
                NSString *faceName = [fontName substringFromIndex:(dividingRange.location + dividingRange.length)];
                NSArray *familyMemberArrays = [fontManager availableMembersOfFontFamily:familyName];
                NSEnumerator *familyMemberArraysEnum = [familyMemberArrays objectEnumerator];
                while ((familyMemberArray = [familyMemberArraysEnum nextObject])) {
                    NSString *familyMemberFaceName = [familyMemberArray objectAtIndex:1];
                    if ([familyMemberFaceName compare:faceName options:NSCaseInsensitiveSearch] == NSOrderedSame) {
                        NSFontTraitMask traits = [[familyMemberArray objectAtIndex:3] integerValue];
                        NSInteger weight = [[familyMemberArray objectAtIndex:2] integerValue];
                        font = [fontManager fontWithFamily:familyName traits:traits weight:weight size:size];
                        break;
                    }
                }
                if (!font) {
                    if (0 < [familyMemberArrays count]) {
                        NSArray *familyMemberArray = [familyMemberArrays objectAtIndex:0];
                        NSFontTraitMask traits = [[familyMemberArray objectAtIndex:3] integerValue];
                        NSInteger weight = [[familyMemberArray objectAtIndex:2] integerValue];
                        font = [fontManager fontWithFamily:familyName traits:traits weight:weight size:size];
                    }
                }
#endif
                break;
            } else {
                dividingSpaceRange = [familyName rangeOfString:@" " options:NSBackwardsSearch];
                dividingDashRange = [familyName rangeOfString:@"-" options:NSBackwardsSearch];
                dividingRange = (0 < dividingSpaceRange.length && 0 < dividingDashRange.length) ? (dividingSpaceRange.location > dividingDashRange.location ? dividingSpaceRange : dividingDashRange) : (0 < dividingSpaceRange.length ? dividingSpaceRange : dividingDashRange);
            }
        }
    }
#if PLATFORM(IOS_FAMILY)
    if (!font)
        font = [PlatformFontClass systemFontOfSize:size];
#else
    if (!font)
        font = [NSFont fontWithName:@"Times" size:size];
    if (!font)
        font = [NSFont userFontOfSize:size];
    if (!font)
        font = [fontManager convertFont:WebDefaultFont() toSize:size];
    if (!font)
        font = WebDefaultFont();
#endif
    [cache setObject:font forKey:fontName];

    return font;
}

static NSParagraphStyle *defaultParagraphStyle()
{
    static NSMutableParagraphStyle *defaultParagraphStyle = nil;
    if (!defaultParagraphStyle) {
        defaultParagraphStyle = [[PlatformNSParagraphStyle defaultParagraphStyle] mutableCopy];
        [defaultParagraphStyle setDefaultTabInterval:36];
        [defaultParagraphStyle setTabStops:@[]];
    }
    return defaultParagraphStyle;
}

RefPtr<CSSValue> HTMLConverterCaches::computedStylePropertyForElement(Element& element, CSSPropertyID propertyId)
{
    if (propertyId == CSSPropertyInvalid)
        return nullptr;

    auto result = m_computedStyles.add(&element, nullptr);
    if (result.isNewEntry)
        result.iterator->value = makeUnique<ComputedStyleExtractor>(&element, true);
    ComputedStyleExtractor& computedStyle = *result.iterator->value;
    return computedStyle.propertyValue(propertyId);
}

RefPtr<CSSValue> HTMLConverterCaches::inlineStylePropertyForElement(Element& element, CSSPropertyID propertyId)
{
    if (propertyId == CSSPropertyInvalid || !is<StyledElement>(element))
        return nullptr;
    const StyleProperties* properties = downcast<StyledElement>(element).inlineStyle();
    if (!properties)
        return nullptr;
    return properties->getPropertyCSSValue(propertyId);
}

static bool stringFromCSSValue(CSSValue& value, String& result)
{
    if (is<CSSPrimitiveValue>(value)) {
        // FIXME: Use isStringType(CSSUnitType)?
        CSSUnitType primitiveType = downcast<CSSPrimitiveValue>(value).primitiveType();
        if (primitiveType == CSSUnitType::CSS_STRING || primitiveType == CSSUnitType::CSS_URI
            || primitiveType == CSSUnitType::CSS_IDENT || primitiveType == CSSUnitType::CSS_ATTR) {
            String stringValue = value.cssText();
            if (stringValue.length()) {
                result = stringValue;
                return true;
            }
        }
    } else if (value.isValueList()) {
        result = value.cssText();
        return true;
    }
    return false;
}

String HTMLConverterCaches::propertyValueForNode(Node& node, CSSPropertyID propertyId)
{
    if (!is<Element>(node)) {
        if (Node* parent = node.parentInComposedTree())
            return propertyValueForNode(*parent, propertyId);
        return String();
    }

    bool inherit = false;
    Element& element = downcast<Element>(node);
    if (RefPtr<CSSValue> value = computedStylePropertyForElement(element, propertyId)) {
        String result;
        if (stringFromCSSValue(*value, result))
            return result;
    }

    if (RefPtr<CSSValue> value = inlineStylePropertyForElement(element, propertyId)) {
        String result;
        if (value->isInheritedValue())
            inherit = true;
        else if (stringFromCSSValue(*value, result))
            return result;
    }

    switch (propertyId) {
    case CSSPropertyDisplay:
        if (element.hasTagName(headTag) || element.hasTagName(scriptTag) || element.hasTagName(appletTag) || element.hasTagName(noframesTag))
            return "none";
        else if (element.hasTagName(addressTag) || element.hasTagName(blockquoteTag) || element.hasTagName(bodyTag) || element.hasTagName(centerTag)
             || element.hasTagName(ddTag) || element.hasTagName(dirTag) || element.hasTagName(divTag) || element.hasTagName(dlTag)
             || element.hasTagName(dtTag) || element.hasTagName(fieldsetTag) || element.hasTagName(formTag) || element.hasTagName(frameTag)
             || element.hasTagName(framesetTag) || element.hasTagName(hrTag) || element.hasTagName(htmlTag) || element.hasTagName(h1Tag)
             || element.hasTagName(h2Tag) || element.hasTagName(h3Tag) || element.hasTagName(h4Tag) || element.hasTagName(h5Tag)
             || element.hasTagName(h6Tag) || element.hasTagName(iframeTag) || element.hasTagName(menuTag) || element.hasTagName(noscriptTag)
             || element.hasTagName(olTag) || element.hasTagName(pTag) || element.hasTagName(preTag) || element.hasTagName(ulTag))
            return "block";
        else if (element.hasTagName(liTag))
            return "list-item";
        else if (element.hasTagName(tableTag))
            return "table";
        else if (element.hasTagName(trTag))
            return "table-row";
        else if (element.hasTagName(thTag) || element.hasTagName(tdTag))
            return "table-cell";
        else if (element.hasTagName(theadTag))
            return "table-header-group";
        else if (element.hasTagName(tbodyTag))
            return "table-row-group";
        else if (element.hasTagName(tfootTag))
            return "table-footer-group";
        else if (element.hasTagName(colTag))
            return "table-column";
        else if (element.hasTagName(colgroupTag))
            return "table-column-group";
        else if (element.hasTagName(captionTag))
            return "table-caption";
        break;
    case CSSPropertyWhiteSpace:
        if (element.hasTagName(preTag))
            return "pre";
        inherit = true;
        break;
    case CSSPropertyFontStyle:
        if (element.hasTagName(iTag) || element.hasTagName(citeTag) || element.hasTagName(emTag) || element.hasTagName(varTag) || element.hasTagName(addressTag))
            return "italic";
        inherit = true;
        break;
    case CSSPropertyFontWeight:
        if (element.hasTagName(bTag) || element.hasTagName(strongTag) || element.hasTagName(thTag))
            return "bolder";
        inherit = true;
        break;
    case CSSPropertyTextDecoration:
        if (element.hasTagName(uTag) || element.hasTagName(insTag))
            return "underline";
        else if (element.hasTagName(sTag) || element.hasTagName(strikeTag) || element.hasTagName(delTag))
            return "line-through";
        inherit = true; // FIXME: This is not strictly correct
        break;
    case CSSPropertyTextAlign:
        if (element.hasTagName(centerTag) || element.hasTagName(captionTag) || element.hasTagName(thTag))
            return "center";
        inherit = true;
        break;
    case CSSPropertyVerticalAlign:
        if (element.hasTagName(supTag))
            return "super";
        else if (element.hasTagName(subTag))
            return "sub";
        else if (element.hasTagName(theadTag) || element.hasTagName(tbodyTag) || element.hasTagName(tfootTag))
            return "middle";
        else if (element.hasTagName(trTag) || element.hasTagName(thTag) || element.hasTagName(tdTag))
            inherit = true;
        break;
    case CSSPropertyFontFamily:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyTextTransform:
    case CSSPropertyTextShadow:
    case CSSPropertyVisibility:
    case CSSPropertyBorderCollapse:
    case CSSPropertyEmptyCells:
    case CSSPropertyWordSpacing:
    case CSSPropertyListStyleType:
    case CSSPropertyDirection:
        inherit = true; // FIXME: Let classes in the css component figure this out.
        break;
    default:
        break;
    }

    if (inherit) {
        if (Node* parent = node.parentInComposedTree())
            return propertyValueForNode(*parent, propertyId);
    }
    
    return String();
}

static inline bool floatValueFromPrimitiveValue(CSSPrimitiveValue& primitiveValue, float& result)
{
    // FIXME: Use CSSPrimitiveValue::computeValue.
    switch (primitiveValue.primitiveType()) {
    case CSSUnitType::CSS_PX:
        result = primitiveValue.floatValue(CSSUnitType::CSS_PX);
        return true;
    case CSSUnitType::CSS_PT:
        result = 4 * primitiveValue.floatValue(CSSUnitType::CSS_PT) / 3;
        return true;
    case CSSUnitType::CSS_PC:
        result = 16 * primitiveValue.floatValue(CSSUnitType::CSS_PC);
        return true;
    case CSSUnitType::CSS_CM:
        result = 96 * primitiveValue.floatValue(CSSUnitType::CSS_PC) / 2.54;
        return true;
    case CSSUnitType::CSS_MM:
        result = 96 * primitiveValue.floatValue(CSSUnitType::CSS_PC) / 25.4;
        return true;
    case CSSUnitType::CSS_Q:
        result = 96 * primitiveValue.floatValue(CSSUnitType::CSS_PC) / (25.4 * 4.0);
        return true;
    case CSSUnitType::CSS_IN:
        result = 96 * primitiveValue.floatValue(CSSUnitType::CSS_IN);
        return true;
    default:
        return false;
    }
}

bool HTMLConverterCaches::floatPropertyValueForNode(Node& node, CSSPropertyID propertyId, float& result)
{
    if (!is<Element>(node)) {
        if (ContainerNode* parent = node.parentInComposedTree())
            return floatPropertyValueForNode(*parent, propertyId, result);
        return false;
    }

    Element& element = downcast<Element>(node);
    if (RefPtr<CSSValue> value = computedStylePropertyForElement(element, propertyId)) {
        if (is<CSSPrimitiveValue>(*value) && floatValueFromPrimitiveValue(downcast<CSSPrimitiveValue>(*value), result))
            return true;
    }

    bool inherit = false;
    if (RefPtr<CSSValue> value = inlineStylePropertyForElement(element, propertyId)) {
        if (is<CSSPrimitiveValue>(*value) && floatValueFromPrimitiveValue(downcast<CSSPrimitiveValue>(*value), result))
            return true;
        if (value->isInheritedValue())
            inherit = true;
    }

    switch (propertyId) {
    case CSSPropertyTextIndent:
    case CSSPropertyLetterSpacing:
    case CSSPropertyWordSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyWidows:
    case CSSPropertyOrphans:
        inherit = true;
        break;
    default:
        break;
    }

    if (inherit) {
        if (ContainerNode* parent = node.parentInComposedTree())
            return floatPropertyValueForNode(*parent, propertyId, result);
    }

    return false;
}

static inline NSShadow *_shadowForShadowStyle(NSString *shadowStyle)
{
    NSShadow *shadow = nil;
    NSUInteger shadowStyleLength = [shadowStyle length];
    NSRange openParenRange = [shadowStyle rangeOfString:@"("];
    NSRange closeParenRange = [shadowStyle rangeOfString:@")"];
    NSRange firstRange = NSMakeRange(NSNotFound, 0);
    NSRange secondRange = NSMakeRange(NSNotFound, 0);
    NSRange thirdRange = NSMakeRange(NSNotFound, 0);
    NSRange spaceRange;
    if (openParenRange.length > 0 && closeParenRange.length > 0 && NSMaxRange(openParenRange) < closeParenRange.location) {
        NSArray *components = [[shadowStyle substringWithRange:NSMakeRange(NSMaxRange(openParenRange), closeParenRange.location - NSMaxRange(openParenRange))] componentsSeparatedByString:@","];
        if ([components count] >= 3) {
            CGFloat red = [[components objectAtIndex:0] floatValue] / 255;
            CGFloat green = [[components objectAtIndex:1] floatValue] / 255;
            CGFloat blue = [[components objectAtIndex:2] floatValue] / 255;
            CGFloat alpha = ([components count] >= 4) ? [[components objectAtIndex:3] floatValue] / 255 : 1;
            NSColor *shadowColor = [PlatformNSColorClass colorWithCalibratedRed:red green:green blue:blue alpha:alpha];
            NSSize shadowOffset;
            CGFloat shadowBlurRadius;
            firstRange = [shadowStyle rangeOfString:@"px"];
            if (firstRange.length > 0 && NSMaxRange(firstRange) < shadowStyleLength)
                secondRange = [shadowStyle rangeOfString:@"px" options:0 range:NSMakeRange(NSMaxRange(firstRange), shadowStyleLength - NSMaxRange(firstRange))];
            if (secondRange.length > 0 && NSMaxRange(secondRange) < shadowStyleLength)
                thirdRange = [shadowStyle rangeOfString:@"px" options:0 range:NSMakeRange(NSMaxRange(secondRange), shadowStyleLength - NSMaxRange(secondRange))];
            if (firstRange.location > 0 && firstRange.length > 0 && secondRange.length > 0 && thirdRange.length > 0) {
                spaceRange = [shadowStyle rangeOfString:@" " options:NSBackwardsSearch range:NSMakeRange(0, firstRange.location)];
                if (spaceRange.length == 0)
                    spaceRange = NSMakeRange(0, 0);
                shadowOffset.width = [[shadowStyle substringWithRange:NSMakeRange(NSMaxRange(spaceRange), firstRange.location - NSMaxRange(spaceRange))] floatValue];
                spaceRange = [shadowStyle rangeOfString:@" " options:NSBackwardsSearch range:NSMakeRange(0, secondRange.location)];
                if (!spaceRange.length)
                    spaceRange = NSMakeRange(0, 0);
                CGFloat shadowHeight = [[shadowStyle substringWithRange:NSMakeRange(NSMaxRange(spaceRange), secondRange.location - NSMaxRange(spaceRange))] floatValue];
                // I don't know why we have this difference between the two platforms.
#if PLATFORM(IOS_FAMILY)
                shadowOffset.height = shadowHeight;
#else
                shadowOffset.height = -shadowHeight;
#endif
                spaceRange = [shadowStyle rangeOfString:@" " options:NSBackwardsSearch range:NSMakeRange(0, thirdRange.location)];
                if (!spaceRange.length)
                    spaceRange = NSMakeRange(0, 0);
                shadowBlurRadius = [[shadowStyle substringWithRange:NSMakeRange(NSMaxRange(spaceRange), thirdRange.location - NSMaxRange(spaceRange))] floatValue];
                shadow = [[(NSShadow *)[PlatformNSShadow alloc] init] autorelease];
                [shadow setShadowColor:shadowColor];
                [shadow setShadowOffset:shadowOffset];
                [shadow setShadowBlurRadius:shadowBlurRadius];
            }
        }
    }
    return shadow;
}

bool HTMLConverterCaches::isBlockElement(Element& element)
{
    String displayValue = propertyValueForNode(element, CSSPropertyDisplay);
    if (displayValue == "block" || displayValue == "list-item" || displayValue.startsWith("table"))
        return true;
    String floatValue = propertyValueForNode(element, CSSPropertyFloat);
    if (floatValue == "left" || floatValue == "right")
        return true;
    return false;
}

bool HTMLConverterCaches::elementHasOwnBackgroundColor(Element& element)
{
    if (!isBlockElement(element))
        return false;
    // In the text system, text blocks (table elements) and documents (body elements)
    // have their own background colors, which should not be inherited.
    return element.hasTagName(htmlTag) || element.hasTagName(bodyTag) || propertyValueForNode(element, CSSPropertyDisplay).startsWith("table");
}

Element* HTMLConverter::_blockLevelElementForNode(Node* node)
{
    Element* element = node->parentElement();
    if (element && !_caches->isBlockElement(*element))
        element = _blockLevelElementForNode(element->parentInComposedTree());
    return element;
}

static Color normalizedColor(Color color, bool ignoreDefaultColor, Element& element)
{
    if (!ignoreDefaultColor)
        return color;

    bool useDarkAppearance = element.document().useDarkAppearance(element.existingComputedStyle());
    if (useDarkAppearance && Color::isWhiteColor(color))
        return Color();

    if (!useDarkAppearance && Color::isBlackColor(color))
        return Color();

    return color;
}

Color HTMLConverterCaches::colorPropertyValueForNode(Node& node, CSSPropertyID propertyId)
{
    if (!is<Element>(node)) {
        if (Node* parent = node.parentInComposedTree())
            return colorPropertyValueForNode(*parent, propertyId);
        return Color();
    }

    bool ignoreDefaultColor = propertyId == CSSPropertyColor;

    Element& element = downcast<Element>(node);
    if (RefPtr<CSSValue> value = computedStylePropertyForElement(element, propertyId)) {
        if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isRGBColor())
            return normalizedColor(downcast<CSSPrimitiveValue>(*value).color(), ignoreDefaultColor, element);
    }

    bool inherit = false;
    if (RefPtr<CSSValue> value = inlineStylePropertyForElement(element, propertyId)) {
        if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isRGBColor())
            return normalizedColor(downcast<CSSPrimitiveValue>(*value).color(), ignoreDefaultColor, element);
        if (value->isInheritedValue())
            inherit = true;
    }

    switch (propertyId) {
    case CSSPropertyColor:
        inherit = true;
        break;
    case CSSPropertyBackgroundColor:
        if (!elementHasOwnBackgroundColor(element)) {
            if (Element* parentElement = node.parentElement()) {
                if (!elementHasOwnBackgroundColor(*parentElement))
                    inherit = true;
            }
        }
        break;
    default:
        break;
    }

    if (inherit) {
        if (Node* parent = node.parentInComposedTree())
            return colorPropertyValueForNode(*parent, propertyId);
    }

    return Color();
}

PlatformColor *HTMLConverter::_colorForElement(Element& element, CSSPropertyID propertyId)
{
    Color result = _caches->colorPropertyValueForNode(element, propertyId);
    if (!result.isValid())
        return nil;
    PlatformColor *platformResult = platformColor(result);
    if ([[PlatformColorClass clearColor] isEqual:platformResult] || ([platformResult alphaComponent] == 0.0))
        return nil;
    return platformResult;
}

static PlatformFont *_font(Element& element)
{
    auto* renderer = element.renderer();
    if (!renderer)
        return nil;
    return (__bridge PlatformFont *)renderer->style().fontCascade().primaryFont().getCTFont();
}

#define UIFloatIsZero(number) (fabs(number - 0) < FLT_EPSILON)

NSDictionary *HTMLConverter::computedAttributesForElement(Element& element)
{
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
#if !PLATFORM(IOS_FAMILY)
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
#endif

    PlatformFont *font = nil;
    PlatformFont *actualFont = _font(element);
    PlatformColor *foregroundColor = _colorForElement(element, CSSPropertyColor);
    PlatformColor *backgroundColor = _colorForElement(element, CSSPropertyBackgroundColor);
    PlatformColor *strokeColor = _colorForElement(element, CSSPropertyWebkitTextStrokeColor);

    float fontSize = 0;
    if (!_caches->floatPropertyValueForNode(element, CSSPropertyFontSize, fontSize) || fontSize <= 0.0)
        fontSize = defaultFontSize;
    if (fontSize < minimumFontSize)
        fontSize = minimumFontSize;
    if (fabs(floor(2.0 * fontSize + 0.5) / 2.0 - fontSize) < 0.05)
        fontSize = floor(2.0 * fontSize + 0.5) / 2;
    else if (fabs(floor(10.0 * fontSize + 0.5) / 10.0 - fontSize) < 0.005)
        fontSize = floor(10.0 * fontSize + 0.5) / 10;

    if (fontSize <= 0.0)
        fontSize = defaultFontSize;
    
#if PLATFORM(IOS_FAMILY)
    if (actualFont)
        font = [actualFont fontWithSize:fontSize];
#else
    if (actualFont)
        font = [fontManager convertFont:actualFont toSize:fontSize];
#endif
    if (!font) {
        String fontName = _caches->propertyValueForNode(element, CSSPropertyFontFamily);
        if (fontName.length())
            font = _fontForNameAndSize(fontName.convertToASCIILowercase(), fontSize, _fontCache);
        if (!font)
            font = [PlatformFontClass fontWithName:@"Times" size:fontSize];

        String fontStyle = _caches->propertyValueForNode(element, CSSPropertyFontStyle);
        if (fontStyle == "italic" || fontStyle == "oblique") {
            PlatformFont *originalFont = font;
#if PLATFORM(IOS_FAMILY)
            font = [PlatformFontClass fontWithFamilyName:[font familyName] traits:UIFontTraitItalic size:[font pointSize]];
#else
            font = [fontManager convertFont:font toHaveTrait:NSItalicFontMask];
#endif
            if (!font)
                font = originalFont;
        }

        String fontWeight = _caches->propertyValueForNode(element, CSSPropertyFontStyle);
        if (fontWeight.startsWith("bold") || fontWeight.toInt() >= 700) {
            // ??? handle weight properly using NSFontManager
            PlatformFont *originalFont = font;
#if PLATFORM(IOS_FAMILY)
            font = [PlatformFontClass fontWithFamilyName:[font familyName] traits:UIFontTraitBold size:[font pointSize]];
#else
            font = [fontManager convertFont:font toHaveTrait:NSBoldFontMask];
#endif
            if (!font)
                font = originalFont;
        }
#if !PLATFORM(IOS_FAMILY) // IJB: No small caps support on iOS
        if (_caches->propertyValueForNode(element, CSSPropertyFontVariantCaps) == "small-caps") {
            // ??? synthesize small-caps if [font isEqual:originalFont]
            NSFont *originalFont = font;
            font = [fontManager convertFont:font toHaveTrait:NSSmallCapsFontMask];
            if (!font)
                font = originalFont;
        }
#endif
    }
    if (font)
        [attrs setObject:font forKey:NSFontAttributeName];
    if (foregroundColor)
        [attrs setObject:foregroundColor forKey:NSForegroundColorAttributeName];
    if (backgroundColor && !_caches->elementHasOwnBackgroundColor(element))
        [attrs setObject:backgroundColor forKey:NSBackgroundColorAttributeName];

    float strokeWidth = 0.0;
    if (_caches->floatPropertyValueForNode(element, CSSPropertyWebkitTextStrokeWidth, strokeWidth)) {
        float textStrokeWidth = strokeWidth / ([font pointSize] * 0.01);
        [attrs setObject:@(textStrokeWidth) forKey:NSStrokeWidthAttributeName];
    }
    if (strokeColor)
        [attrs setObject:strokeColor forKey:NSStrokeColorAttributeName];

    String fontKerning = _caches->propertyValueForNode(element, CSSPropertyWebkitFontKerning);
    String letterSpacing = _caches->propertyValueForNode(element, CSSPropertyLetterSpacing);
    if (fontKerning.length() || letterSpacing.length()) {
        if (fontKerning == "none")
            [attrs setObject:@0.0 forKey:NSKernAttributeName];
        else {
            double kernVal = letterSpacing.length() ? letterSpacing.toDouble() : 0.0;
            if (UIFloatIsZero(kernVal))
                [attrs setObject:@0.0 forKey:NSKernAttributeName]; // auto and normal, the other possible values, are both "kerning enabled"
            else
                [attrs setObject:@(kernVal) forKey:NSKernAttributeName];
        }
    }

    String fontLigatures = _caches->propertyValueForNode(element, CSSPropertyFontVariantLigatures);
    if (fontLigatures.length()) {
        if (fontLigatures.contains("normal"))
            ;   // default: whatever the system decides to do
        else if (fontLigatures.contains("common-ligatures"))
            [attrs setObject:@1 forKey:NSLigatureAttributeName];   // explicitly enabled
        else if (fontLigatures.contains("no-common-ligatures"))
            [attrs setObject:@0 forKey:NSLigatureAttributeName];  // explicitly disabled
    }

    String textDecoration = _caches->propertyValueForNode(element, CSSPropertyTextDecoration);
    if (textDecoration.length()) {
        if (textDecoration.contains("underline"))
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        if (textDecoration.contains("line-through"))
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
    }

    String verticalAlign = _caches->propertyValueForNode(element, CSSPropertyVerticalAlign);
    if (verticalAlign.length()) {
        if (verticalAlign == "super")
            [attrs setObject:[NSNumber numberWithInteger:1] forKey:NSSuperscriptAttributeName];
        else if (verticalAlign == "sub")
            [attrs setObject:[NSNumber numberWithInteger:-1] forKey:NSSuperscriptAttributeName];
    }

    float baselineOffset = 0.0;
    if (_caches->floatPropertyValueForNode(element, CSSPropertyVerticalAlign, baselineOffset))
        [attrs setObject:@(baselineOffset) forKey:NSBaselineOffsetAttributeName];

    String textShadow = _caches->propertyValueForNode(element, CSSPropertyTextShadow);
    if (textShadow.length() > 4) {
        NSShadow *shadow = _shadowForShadowStyle(textShadow);
        if (shadow)
            [attrs setObject:shadow forKey:NSShadowAttributeName];
    }

    Element* blockElement = _blockLevelElementForNode(&element);
    if (&element != blockElement && [_writingDirectionArray count] > 0)
        [attrs setObject:[NSArray arrayWithArray:_writingDirectionArray] forKey:NSWritingDirectionAttributeName];

    if (blockElement) {
        Element& coreBlockElement = *blockElement;
        NSMutableParagraphStyle *paragraphStyle = [defaultParagraphStyle() mutableCopy];
        unsigned heading = 0;
        if (coreBlockElement.hasTagName(h1Tag))
            heading = 1;
        else if (coreBlockElement.hasTagName(h2Tag))
            heading = 2;
        else if (coreBlockElement.hasTagName(h3Tag))
            heading = 3;
        else if (coreBlockElement.hasTagName(h4Tag))
            heading = 4;
        else if (coreBlockElement.hasTagName(h5Tag))
            heading = 5;
        else if (coreBlockElement.hasTagName(h6Tag))
            heading = 6;
        bool isParagraph = coreBlockElement.hasTagName(pTag) || coreBlockElement.hasTagName(liTag) || heading;

        String textAlign = _caches->propertyValueForNode(coreBlockElement, CSSPropertyTextAlign);
        if (textAlign.length()) {
            // WebKit can return -khtml-left, -khtml-right, -khtml-center
            if (textAlign.endsWith("left"))
                [paragraphStyle setAlignment:NSTextAlignmentLeft];
            else if (textAlign.endsWith("right"))
                [paragraphStyle setAlignment:NSTextAlignmentRight];
            else if (textAlign.endsWith("center"))
                [paragraphStyle setAlignment:NSTextAlignmentCenter];
            else if (textAlign.endsWith("justify"))
                [paragraphStyle setAlignment:NSTextAlignmentJustified];
        }

        String direction = _caches->propertyValueForNode(coreBlockElement, CSSPropertyDirection);
        if (direction.length()) {
            if (direction == "ltr")
                [paragraphStyle setBaseWritingDirection:NSWritingDirectionLeftToRight];
            else if (direction == "rtl")
                [paragraphStyle setBaseWritingDirection:NSWritingDirectionRightToLeft];
        }

        String hyphenation = _caches->propertyValueForNode(coreBlockElement, CSSPropertyWebkitHyphens);
        if (hyphenation.length()) {
            if (hyphenation == "auto")
                [paragraphStyle setHyphenationFactor:1.0];
            else
                [paragraphStyle setHyphenationFactor:0.0];
        }
        if (heading)
            [paragraphStyle setHeaderLevel:heading];
        if (isParagraph) {
            // FIXME: Why are we ignoring margin-top?
            float marginLeft = 0.0;
            if (_caches->floatPropertyValueForNode(coreBlockElement, CSSPropertyMarginLeft, marginLeft) && marginLeft > 0.0)
                [paragraphStyle setHeadIndent:marginLeft];
            float textIndent = 0.0;
            if (_caches->floatPropertyValueForNode(coreBlockElement, CSSPropertyTextIndent, textIndent) && textIndent > 0.0)
                [paragraphStyle setFirstLineHeadIndent:[paragraphStyle headIndent] + textIndent];
            float marginRight = 0.0;
            if (_caches->floatPropertyValueForNode(coreBlockElement, CSSPropertyMarginRight, marginRight) && marginRight > 0.0)
                [paragraphStyle setTailIndent:-marginRight];
            float marginBottom = 0.0;
            if (_caches->floatPropertyValueForNode(coreBlockElement, CSSPropertyMarginRight, marginBottom) && marginBottom > 0.0)
                [paragraphStyle setParagraphSpacing:marginBottom];
        }
        if ([_textLists count] > 0)
            [paragraphStyle setTextLists:_textLists];
        if ([_textBlocks count] > 0)
            [paragraphStyle setTextBlocks:_textBlocks];
        [attrs setObject:paragraphStyle forKey:NSParagraphStyleAttributeName];
        [paragraphStyle release];
    }
    return attrs;
}


NSDictionary* HTMLConverter::attributesForElement(Element& element)
{
    auto& attributes = m_attributesForElements.add(&element, nullptr).iterator->value;
    if (!attributes)
        attributes = computedAttributesForElement(element);
    return attributes.get();
}

NSDictionary* HTMLConverter::aggregatedAttributesForAncestors(CharacterData& node)
{
    Node* ancestor = node.parentInComposedTree();
    while (ancestor && !is<Element>(*ancestor))
        ancestor = ancestor->parentInComposedTree();
    if (!ancestor)
        return nullptr;
    return aggregatedAttributesForElementAndItsAncestors(downcast<Element>(*ancestor));
}

NSDictionary* HTMLConverter::aggregatedAttributesForElementAndItsAncestors(Element& element)
{
    auto& cachedAttributes = m_aggregatedAttributesForElements.add(&element, nullptr).iterator->value;
    if (cachedAttributes)
        return cachedAttributes.get();

    NSDictionary* attributesForCurrentElement = attributesForElement(element);
    ASSERT(attributesForCurrentElement);

    Node* ancestor = element.parentInComposedTree();
    while (ancestor && !is<Element>(*ancestor))
        ancestor = ancestor->parentInComposedTree();

    if (!ancestor) {
        cachedAttributes = attributesForCurrentElement;
        return attributesForCurrentElement;
    }

    RetainPtr<NSMutableDictionary> attributesForAncestors = adoptNS([aggregatedAttributesForElementAndItsAncestors(downcast<Element>(*ancestor)) mutableCopy]);
    [attributesForAncestors addEntriesFromDictionary:attributesForCurrentElement];
    m_aggregatedAttributesForElements.set(&element, attributesForAncestors);

    return attributesForAncestors.get();
}

void HTMLConverter::_newParagraphForElement(Element& element, NSString *tag, BOOL flag, BOOL suppressTrailingSpace)
{
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    NSRange rangeToReplace = (suppressTrailingSpace && _flags.isSoft && (lastChar == ' ' || lastChar == NSLineSeparatorCharacter)) ? NSMakeRange(textLength - 1, 1) : NSMakeRange(textLength, 0);
    BOOL needBreak = (flag || lastChar != '\n');
    if (needBreak) {
        NSString *string = (([@"BODY" isEqualToString:tag] || [@"HTML" isEqualToString:tag]) ? @"" : @"\n");
        [_writingDirectionArray removeAllObjects];
        [_attrStr replaceCharactersInRange:rangeToReplace withString:string];
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += [string length] - rangeToReplace.length;
        rangeToReplace.length = [string length];
        NSDictionary *attrs = attributesForElement(element);
        if (rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs range:rangeToReplace];
        _flags.isSoft = YES;
    }
}

void HTMLConverter::_newLineForElement(Element& element)
{
    unichar c = NSLineSeparatorCharacter;
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    NSDictionary *attrs = attributesForElement(element);
    if (rangeToReplace.length > 0)
        [_attrStr setAttributes:attrs range:rangeToReplace];
    _flags.isSoft = YES;
}

void HTMLConverter::_newTabForElement(Element& element)
{
    NSString *string = @"\t";
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    NSRange rangeToReplace = (_flags.isSoft && lastChar == ' ') ? NSMakeRange(textLength - 1, 1) : NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    NSDictionary *attrs = attributesForElement(element);
    if (rangeToReplace.length > 0)
        [_attrStr setAttributes:attrs range:rangeToReplace];
    _flags.isSoft = YES;
}

static Class _WebMessageDocumentClass()
{
    static Class _WebMessageDocumentClass = Nil;
    static BOOL lookedUpClass = NO;
    if (!lookedUpClass) {
        // If the class is not there, we don't want to try again
#if PLATFORM(MAC)
        _WebMessageDocumentClass = objc_lookUpClass("EditableWebMessageDocument");
#endif
        if (!_WebMessageDocumentClass)
            _WebMessageDocumentClass = objc_lookUpClass("WebMessageDocument");

        if (_WebMessageDocumentClass && ![_WebMessageDocumentClass respondsToSelector:@selector(document:attachment:forURL:)])
            _WebMessageDocumentClass = Nil;
        lookedUpClass = YES;
    }
    return _WebMessageDocumentClass;
}

BOOL HTMLConverter::_addAttachmentForElement(Element& element, NSURL *url, BOOL needsParagraph, BOOL usePlaceholder)
{
    BOOL retval = NO;
    BOOL notFound = NO;
    NSFileWrapper *fileWrapper = nil;
    Frame* frame = element.document().frame();
    DocumentLoader *dataSource = frame->loader().frameHasLoaded() ? frame->loader().documentLoader() : 0;
    BOOL ignoreOrientation = YES;

    if ([url isFileURL]) {
        NSString *path = [[url path] stringByStandardizingPath];
        if (path)
            fileWrapper = [[[NSFileWrapper alloc] initWithURL:url options:0 error:NULL] autorelease];
    }
    if (!fileWrapper && dataSource) {
        RefPtr<ArchiveResource> resource = dataSource->subresource(url);
        if (!resource)
            resource = dataSource->subresource(url);

        const String& mimeType = resource->mimeType();
        if (usePlaceholder && resource && mimeType == "text/html")
            notFound = YES;
        if (resource && !notFound) {
            fileWrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:resource->data().createNSData().get()] autorelease];
            [fileWrapper setPreferredFilename:suggestedFilenameWithMIMEType(url, mimeType)];
        }
    }
#if !PLATFORM(IOS_FAMILY)
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(dataSource, url);
        if (usePlaceholder && fileWrapper && [[[[fileWrapper preferredFilename] pathExtension] lowercaseString] hasPrefix:@"htm"])
            notFound = YES;
        if (notFound)
            fileWrapper = nil;
    }
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(m_dataSource, url);
        if (usePlaceholder && fileWrapper && [[[[fileWrapper preferredFilename] pathExtension] lowercaseString] hasPrefix:@"htm"])
            notFound = YES;
        if (notFound)
            fileWrapper = nil;
    }
#endif
    if (!fileWrapper && !notFound && url) {
        // Special handling for Mail attachments, until WebKit provides a standard way to get the data.
        Class WebMessageDocumentClass = _WebMessageDocumentClass();
        if (WebMessageDocumentClass) {
            NSTextAttachment *mimeTextAttachment = nil;
            [WebMessageDocumentClass document:NULL attachment:&mimeTextAttachment forURL:url];
            if (mimeTextAttachment && [mimeTextAttachment respondsToSelector:@selector(fileWrapper)]) {
                fileWrapper = [mimeTextAttachment performSelector:@selector(fileWrapper)];
                ignoreOrientation = NO;
            }
        }
    }
    if (fileWrapper || usePlaceholder) {
        NSUInteger textLength = [_attrStr length];
        RetainPtr<NSTextAttachment> attachment = adoptNS([[PlatformNSTextAttachment alloc] initWithFileWrapper:fileWrapper]);
#if PLATFORM(IOS_FAMILY)
        float verticalAlign = 0.0;
        _caches->floatPropertyValueForNode(element, CSSPropertyVerticalAlign, verticalAlign);
        attachment.get().bounds = CGRectMake(0, (verticalAlign / 100) * element.clientHeight(), element.clientWidth(), element.clientHeight());
#endif
        RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithFormat:(needsParagraph ? @"%C\n" : @"%C"), static_cast<unichar>(NSAttachmentCharacter)]);
        NSRange rangeToReplace = NSMakeRange(textLength, 0);
        NSDictionary *attrs;
        if (fileWrapper) {
#if !PLATFORM(IOS_FAMILY)
            if (ignoreOrientation)
                [attachment setIgnoresOrientation:YES];
#endif
        } else {
            NSBundle *webCoreBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
#if PLATFORM(IOS_FAMILY)
            UIImage *missingImage = [PlatformImageClass imageNamed:@"missingImage" inBundle:webCoreBundle compatibleWithTraitCollection:nil];
#else
            NSImage *missingImage = [webCoreBundle imageForResource:@"missingImage"];
#endif
            ASSERT_WITH_MESSAGE(missingImage != nil, "Unable to find missingImage.");
            attachment = adoptNS([[PlatformNSTextAttachment alloc] initWithData:nil ofType:nil]);
            attachment.get().image = missingImage;
        }
        [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
        rangeToReplace.length = [string length];
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += rangeToReplace.length;
        attrs = attributesForElement(element);
        if (rangeToReplace.length > 0) {
            [_attrStr setAttributes:attrs range:rangeToReplace];
            rangeToReplace.length = 1;
            [_attrStr addAttribute:NSAttachmentAttributeName value:attachment.get() range:rangeToReplace];
        }
        _flags.isSoft = NO;
        retval = YES;
    }
    return retval;
}

void HTMLConverter::_addQuoteForElement(Element& element, BOOL opening, NSInteger level)
{
    unichar c = ((level % 2) == 0) ? (opening ? 0x201c : 0x201d) : (opening ? 0x2018 : 0x2019);
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    RetainPtr<NSDictionary> attrs = attributesForElement(element);
    if (rangeToReplace.length > 0)
        [_attrStr setAttributes:attrs.get() range:rangeToReplace];
    _flags.isSoft = NO;
}

void HTMLConverter::_addValue(NSString *value, Element& element)
{
    NSUInteger textLength = [_attrStr length];
    NSUInteger valueLength = [value length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    if (valueLength) {
        [_attrStr replaceCharactersInRange:rangeToReplace withString:value];
        rangeToReplace.length = valueLength;
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += rangeToReplace.length;
        RetainPtr<NSDictionary> attrs = attributesForElement(element);
        if (rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs.get() range:rangeToReplace];
        _flags.isSoft = NO;
    }
}

void HTMLConverter::_fillInBlock(NSTextBlock *block, Element& element, PlatformColor *backgroundColor, CGFloat extraMargin, CGFloat extraPadding, BOOL isTable)
{
    float result = 0;
    
    NSString *width = element.getAttribute(widthAttr);
    if ((width && [width length]) || !isTable) {
        if (_caches->floatPropertyValueForNode(element, CSSPropertyWidth, result))
            [block setValue:result type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockWidth];
    }
    
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMinWidth, result))
        [block setValue:result type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumWidth];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMaxWidth, result))
        [block setValue:result type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumWidth];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMinHeight, result))
        [block setValue:result type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumHeight];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMaxHeight, result))
        [block setValue:result type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumHeight];

    if (_caches->floatPropertyValueForNode(element, CSSPropertyPaddingLeft, result))
        [block setWidth:result + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    else
        [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    
    if (_caches->floatPropertyValueForNode(element, CSSPropertyPaddingTop, result))
        [block setWidth:result + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    else
        [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    
    if (_caches->floatPropertyValueForNode(element, CSSPropertyPaddingRight, result))
        [block setWidth:result + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    else
        [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    
    if (_caches->floatPropertyValueForNode(element, CSSPropertyPaddingBottom, result))
        [block setWidth:result + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];
    else
        [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];
    
    if (_caches->floatPropertyValueForNode(element, CSSPropertyBorderLeftWidth, result))
        [block setWidth:result type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinXEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyBorderTopWidth, result))
        [block setWidth:result type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinYEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyBorderRightWidth, result))
        [block setWidth:result type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxXEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyBorderBottomWidth, result))
        [block setWidth:result type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxYEdge];

    if (_caches->floatPropertyValueForNode(element, CSSPropertyMarginLeft, result))
        [block setWidth:result + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge];
    else
        [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMarginTop, result))
        [block setWidth:result + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge];
    else
        [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMarginRight, result))
        [block setWidth:result + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge];
    else
        [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge];
    if (_caches->floatPropertyValueForNode(element, CSSPropertyMarginBottom, result))
        [block setWidth:result + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge];
    else
        [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge];
    
    PlatformColor *color = nil;
    if ((color = _colorForElement(element, CSSPropertyBackgroundColor)))
        [block setBackgroundColor:color];
    if (!color && backgroundColor)
        [block setBackgroundColor:backgroundColor];
    
    if ((color = _colorForElement(element, CSSPropertyBorderLeftColor)))
        [block setBorderColor:color forEdge:NSMinXEdge];
    
    if ((color = _colorForElement(element, CSSPropertyBorderTopColor)))
        [block setBorderColor:color forEdge:NSMinYEdge];
    if ((color = _colorForElement(element, CSSPropertyBorderRightColor)))
        [block setBorderColor:color forEdge:NSMaxXEdge];
    if ((color = _colorForElement(element, CSSPropertyBorderBottomColor)))
        [block setBorderColor:color forEdge:NSMaxYEdge];
}

static inline BOOL read2DigitNumber(const char **pp, int8_t *outval)
{
    BOOL result = NO;
    char c1 = *(*pp)++, c2;
    if (isASCIIDigit(c1)) {
        c2 = *(*pp)++;
        if (isASCIIDigit(c2)) {
            *outval = 10 * (c1 - '0') + (c2 - '0');
            result = YES;
        }
    }
    return result;
}

static inline NSDate *_dateForString(NSString *string)
{
    const char *p = [string UTF8String];
    RetainPtr<NSDateComponents> dateComponents = adoptNS([[NSDateComponents alloc] init]);

    // Set the time zone to GMT
    [dateComponents setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    NSInteger year = 0;
    while (*p && isASCIIDigit(*p))
        year = 10 * year + *p++ - '0';
    if (*p++ != '-')
        return nil;
    [dateComponents setYear:year];

    int8_t component;
    if (!read2DigitNumber(&p, &component) || *p++ != '-')
        return nil;
    [dateComponents setMonth:component];

    if (!read2DigitNumber(&p, &component) || *p++ != 'T')
        return nil;
    [dateComponents setDay:component];

    if (!read2DigitNumber(&p, &component) || *p++ != ':')
        return nil;
    [dateComponents setHour:component];

    if (!read2DigitNumber(&p, &component) || *p++ != ':')
        return nil;
    [dateComponents setMinute:component];

    if (!read2DigitNumber(&p, &component) || *p++ != 'Z')
        return nil;
    [dateComponents setSecond:component];
    
    return [[[[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease] dateFromComponents:dateComponents.get()];
}

static NSInteger _colCompare(id block1, id block2, void *)
{
    NSInteger col1 = [(NSTextTableBlock *)block1 startingColumn];
    NSInteger col2 = [(NSTextTableBlock *)block2 startingColumn];
    return ((col1 < col2) ? NSOrderedAscending : ((col1 == col2) ? NSOrderedSame : NSOrderedDescending));
}

void HTMLConverter::_processMetaElementWithName(NSString *name, NSString *content)
{
    NSString *key = nil;
    if (NSOrderedSame == [@"CocoaVersion" compare:name options:NSCaseInsensitiveSearch]) {
        CGFloat versionNumber = [content doubleValue];
        if (versionNumber > 0.0) {
            // ??? this should be keyed off of version number in future
            [_documentAttrs removeObjectForKey:NSConvertedDocumentAttribute];
            [_documentAttrs setObject:@(versionNumber) forKey:NSCocoaVersionDocumentAttribute];
        }
#if PLATFORM(IOS_FAMILY)
    } else if (NSOrderedSame == [@"Generator" compare:name options:NSCaseInsensitiveSearch]) {
        key = NSGeneratorDocumentAttribute;
#endif
    } else if (NSOrderedSame == [@"Keywords" compare:name options:NSCaseInsensitiveSearch]) {
        if (content && [content length] > 0) {
            NSArray *array;
            // ??? need better handling here and throughout
            if ([content rangeOfString:@", "].length == 0 && [content rangeOfString:@","].length > 0)
                array = [content componentsSeparatedByString:@","];
            else if ([content rangeOfString:@", "].length == 0 && [content rangeOfString:@" "].length > 0)
                array = [content componentsSeparatedByString:@" "];
            else
                array = [content componentsSeparatedByString:@", "];
            [_documentAttrs setObject:array forKey:NSKeywordsDocumentAttribute];
        }
    } else if (NSOrderedSame == [@"Author" compare:name options:NSCaseInsensitiveSearch])
        key = NSAuthorDocumentAttribute;
    else if (NSOrderedSame == [@"LastAuthor" compare:name options:NSCaseInsensitiveSearch])
        key = NSEditorDocumentAttribute;
    else if (NSOrderedSame == [@"Company" compare:name options:NSCaseInsensitiveSearch])
        key = NSCompanyDocumentAttribute;
    else if (NSOrderedSame == [@"Copyright" compare:name options:NSCaseInsensitiveSearch])
        key = NSCopyrightDocumentAttribute;
    else if (NSOrderedSame == [@"Subject" compare:name options:NSCaseInsensitiveSearch])
        key = NSSubjectDocumentAttribute;
    else if (NSOrderedSame == [@"Description" compare:name options:NSCaseInsensitiveSearch] || NSOrderedSame == [@"Comment" compare:name options:NSCaseInsensitiveSearch])
        key = NSCommentDocumentAttribute;
    else if (NSOrderedSame == [@"CreationTime" compare:name options:NSCaseInsensitiveSearch]) {
        if (content && [content length] > 0) {
            NSDate *date = _dateForString(content);
            if (date)
                [_documentAttrs setObject:date forKey:NSCreationTimeDocumentAttribute];
        }
    } else if (NSOrderedSame == [@"ModificationTime" compare:name options:NSCaseInsensitiveSearch]) {
        if (content && [content length] > 0) {
            NSDate *date = _dateForString(content);
            if (date)
                [_documentAttrs setObject:date forKey:NSModificationTimeDocumentAttribute];
        }
    }
#if PLATFORM(IOS_FAMILY)
    else if (NSOrderedSame == [@"DisplayName" compare:name options:NSCaseInsensitiveSearch] || NSOrderedSame == [@"IndexTitle" compare:name options:NSCaseInsensitiveSearch])
        key = NSDisplayNameDocumentAttribute;
    else if (NSOrderedSame == [@"robots" compare:name options:NSCaseInsensitiveSearch]) {
        if ([content rangeOfString:@"noindex" options:NSCaseInsensitiveSearch].length > 0)
            [_documentAttrs setObject:[NSNumber numberWithInteger:1] forKey:NSNoIndexDocumentAttribute];
    }
#endif
    if (key && content && [content length] > 0)
        [_documentAttrs setObject:content forKey:key];
}

void HTMLConverter::_processHeadElement(Element& element)
{
    // FIXME: Should gather data from other sources e.g. Word, but for that we would need to be able to get comments from DOM
    
    for (HTMLMetaElement* child = Traversal<HTMLMetaElement>::firstChild(element); child; child = Traversal<HTMLMetaElement>::nextSibling(*child)) {
        NSString *name = child->name();
        NSString *content = child->content();
        if (name && content)
            _processMetaElementWithName(name, content);
    }
}

BOOL HTMLConverter::_enterElement(Element& element, BOOL embedded)
{
    String displayValue = _caches->propertyValueForNode(element, CSSPropertyDisplay);

    if (element.hasTagName(headTag) && !embedded)
        _processHeadElement(element);
    else if (!displayValue.length() || !(displayValue == "none" || displayValue == "table-column" || displayValue == "table-column-group")) {
        if (_caches->isBlockElement(element) && !element.hasTagName(brTag) && !(displayValue == "table-cell" && [_textTables count] == 0)
            && !([_textLists count] > 0 && displayValue == "block" && !element.hasTagName(liTag) && !element.hasTagName(ulTag) && !element.hasTagName(olTag)))
            _newParagraphForElement(element, element.tagName(), NO, YES);
        return YES;
    }
    return NO;
}

void HTMLConverter::_addTableForElement(Element *tableElement)
{
    RetainPtr<NSTextTable> table = adoptNS([(NSTextTable *)[PlatformNSTextTable alloc] init]);
    CGFloat cellSpacingVal = 1;
    CGFloat cellPaddingVal = 1;
    [table setNumberOfColumns:1];
    [table setLayoutAlgorithm:NSTextTableAutomaticLayoutAlgorithm];
    [table setCollapsesBorders:NO];
    [table setHidesEmptyCells:NO];
    
    if (tableElement) {
        ASSERT(tableElement);
        Element& coreTableElement = *tableElement;
        
        NSString *cellSpacing = coreTableElement.getAttribute(cellspacingAttr);
        if (cellSpacing && [cellSpacing length] > 0 && ![cellSpacing hasSuffix:@"%"])
            cellSpacingVal = [cellSpacing floatValue];
        NSString *cellPadding = coreTableElement.getAttribute(cellpaddingAttr);
        if (cellPadding && [cellPadding length] > 0 && ![cellPadding hasSuffix:@"%"])
            cellPaddingVal = [cellPadding floatValue];
        
        _fillInBlock(table.get(), coreTableElement, nil, 0, 0, YES);

        if (_caches->propertyValueForNode(coreTableElement, CSSPropertyBorderCollapse) == "collapse") {
            [table setCollapsesBorders:YES];
            cellSpacingVal = 0;
        }
        if (_caches->propertyValueForNode(coreTableElement, CSSPropertyEmptyCells) == "hide")
            [table setHidesEmptyCells:YES];
        if (_caches->propertyValueForNode(coreTableElement, CSSPropertyTableLayout) == "fixed")
            [table setLayoutAlgorithm:NSTextTableFixedLayoutAlgorithm];
    }
    
    [_textTables addObject:table.get()];
    [_textTableSpacings addObject:@(cellSpacingVal)];
    [_textTablePaddings addObject:@(cellPaddingVal)];
    [_textTableRows addObject:[NSNumber numberWithInteger:0]];
    [_textTableRowArrays addObject:[NSMutableArray array]];
}

void HTMLConverter::_addTableCellForElement(Element* element)
{
    NSTextTable *table = [_textTables lastObject];
    NSInteger rowNumber = [[_textTableRows lastObject] integerValue];
    NSInteger columnNumber = 0;
    NSInteger rowSpan = 1;
    NSInteger colSpan = 1;
    NSMutableArray *rowArray = [_textTableRowArrays lastObject];
    NSUInteger count = [rowArray count];
    PlatformColor *color = ([_textTableRowBackgroundColors count] > 0) ? [_textTableRowBackgroundColors lastObject] : nil;
    NSTextTableBlock *previousBlock;
    CGFloat cellSpacingVal = [[_textTableSpacings lastObject] floatValue];
    if ([color isEqual:[PlatformColorClass clearColor]]) color = nil;
    for (NSUInteger i = 0; i < count; i++) {
        previousBlock = [rowArray objectAtIndex:i];
        if (columnNumber >= [previousBlock startingColumn] && columnNumber < [previousBlock startingColumn] + [previousBlock columnSpan])
            columnNumber = [previousBlock startingColumn] + [previousBlock columnSpan];
    }
    
    RetainPtr<NSTextTableBlock> block;
    
    if (element) {
        if (is<HTMLTableCellElement>(*element)) {
            HTMLTableCellElement& tableCellElement = downcast<HTMLTableCellElement>(*element);
            
            rowSpan = tableCellElement.rowSpan();
            if (rowSpan < 1)
                rowSpan = 1;
            colSpan = tableCellElement.colSpan();
            if (colSpan < 1)
                colSpan = 1;
        }
        
        block = adoptNS([[PlatformNSTextTableBlock alloc] initWithTable:table startingRow:rowNumber rowSpan:rowSpan startingColumn:columnNumber columnSpan:colSpan]);
        
        String verticalAlign = _caches->propertyValueForNode(*element, CSSPropertyVerticalAlign);
        
        _fillInBlock(block.get(), *element, color, cellSpacingVal / 2, 0, NO);
        if (verticalAlign == "middle")
            [block setVerticalAlignment:NSTextBlockMiddleAlignment];
        else if (verticalAlign == "bottom")
            [block setVerticalAlignment:NSTextBlockBottomAlignment];
        else if (verticalAlign == "baseline")
            [block setVerticalAlignment:NSTextBlockBaselineAlignment];
        else if (verticalAlign == "top")
            [block setVerticalAlignment:NSTextBlockTopAlignment];
    } else {
        block = adoptNS([[PlatformNSTextTableBlock alloc] initWithTable:table startingRow:rowNumber rowSpan:rowSpan startingColumn:columnNumber columnSpan:colSpan]);
    }
    
    [_textBlocks addObject:block.get()];
    [rowArray addObject:block.get()];
    [rowArray sortUsingFunction:_colCompare context:NULL];
}

BOOL HTMLConverter::_processElement(Element& element, NSInteger depth)
{
    BOOL retval = YES;
    BOOL isBlockLevel = _caches->isBlockElement(element);
    String displayValue = _caches->propertyValueForNode(element, CSSPropertyDisplay);
    if (isBlockLevel)
        [_writingDirectionArray removeAllObjects];
    else {
        String bidi = _caches->propertyValueForNode(element, CSSPropertyUnicodeBidi);
        if (bidi == "embed") {
            NSUInteger val = NSWritingDirectionEmbedding;
            if (_caches->propertyValueForNode(element, CSSPropertyDirection) == "rtl")
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        } else if (bidi == "bidi-override") {
            NSUInteger val = NSWritingDirectionOverride;
            if (_caches->propertyValueForNode(element, CSSPropertyDirection) == "rtl")
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        }
    }
    if (displayValue == "table" || ([_textTables count] == 0 && displayValue == "table-row-group")) {
        Element* tableElement = &element;
        if (displayValue == "table-row-group") {
            // If we are starting in medias res, the first thing we see may be the tbody, so go up to the table
            tableElement = _blockLevelElementForNode(element.parentInComposedTree());
            if (!tableElement || _caches->propertyValueForNode(*tableElement, CSSPropertyDisplay) != "table")
                tableElement = &element;
        }
        while ([_textTables count] > [_textBlocks count])
            _addTableCellForElement(nil);
        _addTableForElement(tableElement);
    } else if (displayValue == "table-footer-group" && [_textTables count] > 0) {
        m_textTableFooters.add((__bridge CFTypeRef)[_textTables lastObject], &element);
        retval = NO;
    } else if (displayValue == "table-row" && [_textTables count] > 0) {
        PlatformColor *color = _colorForElement(element, CSSPropertyBackgroundColor);
        if (!color)
            color = (PlatformColor *)[PlatformColorClass clearColor];
        [_textTableRowBackgroundColors addObject:color];
    } else if (displayValue == "table-cell") {
        while ([_textTables count] < [_textBlocks count] + 1)
            _addTableForElement(nil);
        _addTableCellForElement(&element);
#if ENABLE(ATTACHMENT_ELEMENT)
    } else if (is<HTMLAttachmentElement>(element)) {
        HTMLAttachmentElement& attachment = downcast<HTMLAttachmentElement>(element);
        if (attachment.file()) {
            NSURL *url = [NSURL fileURLWithPath:attachment.file()->path()];
            if (url)
                _addAttachmentForElement(element, url, isBlockLevel, NO);
        }
        retval = NO;
#endif
    } else if (element.hasTagName(imgTag)) {
        NSString *urlString = element.imageSourceURL();
        if (urlString && [urlString length] > 0) {
            NSURL *url = element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
#if PLATFORM(IOS_FAMILY)
            BOOL usePlaceholderImage = NO;
#else
            BOOL usePlaceholderImage = YES;
#endif
            if (url)
                _addAttachmentForElement(element, url, isBlockLevel, usePlaceholderImage);
        }
        retval = NO;
    } else if (element.hasTagName(objectTag)) {
        NSString *baseString = element.getAttribute(codebaseAttr);
        NSString *urlString = element.getAttribute(dataAttr);
        NSString *declareString = element.getAttribute(declareAttr);
        if (urlString && [urlString length] > 0 && ![@"true" isEqualToString:declareString]) {
            NSURL *baseURL = nil;
            NSURL *url = nil;
            if (baseString && [baseString length] > 0) {
                baseURL = element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(baseString));
                if (!baseURL)
                    baseURL = [NSURL _web_URLWithString:[baseString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
            }
            if (baseURL)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:baseURL];
            if (!url)
                url = element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
            if (url)
                retval = !_addAttachmentForElement(element, url, isBlockLevel, NO);
        }
    } else if (is<HTMLFrameElementBase>(element)) {
        if (Document* contentDocument = downcast<HTMLFrameElementBase>(element).contentDocument()) {
            _traverseNode(*contentDocument, depth + 1, true /* embedded */);
            retval = NO;
        }
    } else if (element.hasTagName(brTag)) {
        Element* blockElement = _blockLevelElementForNode(element.parentInComposedTree());
        NSString *breakClass = element.getAttribute(classAttr);
        NSString *blockTag = blockElement ? (NSString *)blockElement->tagName() : nil;
        BOOL isExtraBreak = [@"Apple-interchange-newline" isEqualToString:breakClass];
        BOOL blockElementIsParagraph = ([@"P" isEqualToString:blockTag] || [@"LI" isEqualToString:blockTag] || ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]));
        if (isExtraBreak)
            _flags.hasTrailingNewline = YES;
        else {
            if (blockElement && blockElementIsParagraph)
                _newLineForElement(element);
            else
                _newParagraphForElement(element, element.tagName(), YES, NO);
        }
    } else if (element.hasTagName(ulTag)) {
        RetainPtr<NSTextList> list;
        String listStyleType = _caches->propertyValueForNode(element, CSSPropertyListStyleType);
        if (!listStyleType.length())
            listStyleType = @"disc";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:String("{" + listStyleType + "}") options:0]);
        [_textLists addObject:list.get()];
    } else if (element.hasTagName(olTag)) {
        RetainPtr<NSTextList> list;
        String listStyleType = _caches->propertyValueForNode(element, CSSPropertyListStyleType);
        if (!listStyleType.length())
            listStyleType = "decimal";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:String("{" + listStyleType + "}") options:0]);
        if (is<HTMLOListElement>(element)) {
            NSInteger startingItemNumber = downcast<HTMLOListElement>(element).start();
            [list setStartingItemNumber:startingItemNumber];
        }
        [_textLists addObject:list.get()];
    } else if (element.hasTagName(qTag)) {
        _addQuoteForElement(element, YES, _quoteLevel++);
    } else if (element.hasTagName(inputTag)) {
        if (is<HTMLInputElement>(element)) {
            HTMLInputElement& inputElement = downcast<HTMLInputElement>(element);
            if (inputElement.type() == "text") {
                NSString *value = inputElement.value();
                if (value && [value length] > 0)
                    _addValue(value, element);
            }
        }
    } else if (element.hasTagName(textareaTag)) {
        if (is<HTMLTextAreaElement>(element)) {
            HTMLTextAreaElement& textAreaElement = downcast<HTMLTextAreaElement>(element);
            NSString *value = textAreaElement.value();
            if (value && [value length] > 0)
                _addValue(value, element);
        }
        retval = NO;
    }
    return retval;
}

void HTMLConverter::_addMarkersToList(NSTextList *list, NSRange range)
{
    NSInteger itemNum = [list startingItemNumber];
    NSString *string = [_attrStr string];
    NSString *stringToInsert;
    NSDictionary *attrsToInsert = nil;
    PlatformFont *font;
    NSParagraphStyle *paragraphStyle;
    NSMutableParagraphStyle *newStyle;
    NSTextTab *tab = nil;
    NSTextTab *tabToRemove;
    NSRange paragraphRange;
    NSRange styleRange;
    NSUInteger textLength = [_attrStr length];
    NSUInteger listIndex;
    NSUInteger insertLength;
    NSUInteger i;
    NSUInteger count;
    NSArray *textLists;
    CGFloat markerLocation;
    CGFloat listLocation;
    
    if (range.length == 0 || range.location >= textLength)
        return;
    if (NSMaxRange(range) > textLength)
        range.length = textLength - range.location;
    paragraphStyle = [_attrStr attribute:NSParagraphStyleAttributeName atIndex:range.location effectiveRange:NULL];
    if (paragraphStyle) {
        textLists = [paragraphStyle textLists];
        listIndex = [textLists indexOfObject:list];
        if (textLists && listIndex != NSNotFound) {
            for (NSUInteger idx = range.location; idx < NSMaxRange(range);) {
                paragraphRange = [string paragraphRangeForRange:NSMakeRange(idx, 0)];
                paragraphStyle = [_attrStr attribute:NSParagraphStyleAttributeName atIndex:idx effectiveRange:&styleRange];
                font = [_attrStr attribute:NSFontAttributeName atIndex:idx effectiveRange:NULL];
                if ([[paragraphStyle textLists] count] == listIndex + 1) {
                    stringToInsert = [NSString stringWithFormat:@"\t%@\t", [list markerForItemNumber:itemNum++]];
                    insertLength = [stringToInsert length];
                    attrsToInsert = [PlatformNSTextList _standardMarkerAttributesForAttributes:[_attrStr attributesAtIndex:paragraphRange.location effectiveRange:NULL]];

                    [_attrStr replaceCharactersInRange:NSMakeRange(paragraphRange.location, 0) withString:stringToInsert];
                    [_attrStr setAttributes:attrsToInsert range:NSMakeRange(paragraphRange.location, insertLength)];
                    range.length += insertLength;
                    paragraphRange.length += insertLength;
                    if (paragraphRange.location < _domRangeStartIndex)
                        _domRangeStartIndex += insertLength;
                    
                    newStyle = [paragraphStyle mutableCopy];
                    listLocation = (listIndex + 1) * 36;
                    markerLocation = listLocation - 25;
                    [newStyle setFirstLineHeadIndent:0];
                    [newStyle setHeadIndent:listLocation];
                    while ((count = [[newStyle tabStops] count]) > 0) {
                        for (i = 0, tabToRemove = nil; !tabToRemove && i < count; i++) {
                            tab = [[newStyle tabStops] objectAtIndex:i];
                            if ([tab location] <= listLocation)
                                tabToRemove = tab;
                        }
                        if (tabToRemove)
                            [newStyle removeTabStop:tab];
                        else
                            break;
                    }
                    tab = [[PlatformNSTextTab alloc] initWithType:NSLeftTabStopType location:markerLocation];
                    [newStyle addTabStop:tab];
                    [tab release];
                    tab = [[PlatformNSTextTab alloc] initWithTextAlignment:NSTextAlignmentNatural location:listLocation options:@{ }];
                    [newStyle addTabStop:tab];
                    [tab release];
                    [_attrStr addAttribute:NSParagraphStyleAttributeName value:newStyle range:paragraphRange];
                    [newStyle release];
                    
                    idx = NSMaxRange(paragraphRange);
                } else {
                    // skip any deeper-nested lists
                    idx = NSMaxRange(styleRange);
                }
            }
        }
    }
}

void HTMLConverter::_exitElement(Element& element, NSInteger depth, NSUInteger startIndex)
{
    String displayValue = _caches->propertyValueForNode(element, CSSPropertyDisplay);
    NSRange range = NSMakeRange(startIndex, [_attrStr length] - startIndex);
    if (range.length > 0 && element.hasTagName(aTag)) {
        NSString *urlString = element.getAttribute(hrefAttr);
        NSString *strippedString = [urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (urlString && [urlString length] > 0 && strippedString && [strippedString length] > 0 && ![strippedString hasPrefix:@"#"]) {
            NSURL *url = element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(strippedString));
            if (!url)
                url = [NSURL _web_URLWithString:strippedString relativeToURL:_baseURL];
            [_attrStr addAttribute:NSLinkAttributeName value:url ? (id)url : (id)urlString range:range];
        }
    }
    if (!_flags.reachedEnd && _caches->isBlockElement(element)) {
        [_writingDirectionArray removeAllObjects];
        if (displayValue == "table-cell" && [_textBlocks count] == 0) {
            _newTabForElement(element);
        } else if ([_textLists count] > 0 && displayValue == "block" && !element.hasTagName(liTag) && !element.hasTagName(ulTag) && !element.hasTagName(olTag)) {
            _newLineForElement(element);
        } else {
            _newParagraphForElement(element, element.tagName(), (range.length == 0), YES);
        }
    } else if ([_writingDirectionArray count] > 0) {
        String bidi = _caches->propertyValueForNode(element, CSSPropertyUnicodeBidi);
        if (bidi == "embed" || bidi == "bidi-override")
            [_writingDirectionArray removeLastObject];
    }
    range = NSMakeRange(startIndex, [_attrStr length] - startIndex);
    if (displayValue == "table" && [_textTables count] > 0) {
        NSTextTable *key = [_textTables lastObject];
        Element* footer = m_textTableFooters.get((__bridge CFTypeRef)key);
        while ([_textTables count] < [_textBlocks count] + 1)
            [_textBlocks removeLastObject];
        if (footer) {
            _traverseFooterNode(*footer, depth + 1);
            m_textTableFooters.remove((__bridge CFTypeRef)key);
        }
        [_textTables removeLastObject];
        [_textTableSpacings removeLastObject];
        [_textTablePaddings removeLastObject];
        [_textTableRows removeLastObject];
        [_textTableRowArrays removeLastObject];
    } else if (displayValue == "table-row" && [_textTables count] > 0) {
        NSTextTable *table = [_textTables lastObject];
        NSTextTableBlock *block;
        NSMutableArray *rowArray = [_textTableRowArrays lastObject], *previousRowArray;
        NSUInteger i, count;
        NSInteger numberOfColumns = [table numberOfColumns];
        NSInteger openColumn;
        NSInteger rowNumber = [[_textTableRows lastObject] integerValue];
        do {
            rowNumber++;
            previousRowArray = rowArray;
            rowArray = [NSMutableArray array];
            count = [previousRowArray count];
            for (i = 0; i < count; i++) {
                block = [previousRowArray objectAtIndex:i];
                if ([block startingColumn] + [block columnSpan] > numberOfColumns) numberOfColumns = [block startingColumn] + [block columnSpan];
                if ([block startingRow] + [block rowSpan] > rowNumber) [rowArray addObject:block];
            }
            count = [rowArray count];
            openColumn = 0;
            for (i = 0; i < count; i++) {
                block = [rowArray objectAtIndex:i];
                if (openColumn >= [block startingColumn] && openColumn < [block startingColumn] + [block columnSpan]) openColumn = [block startingColumn] + [block columnSpan];
            }
        } while (openColumn >= numberOfColumns);
        if ((NSUInteger)numberOfColumns > [table numberOfColumns])
            [table setNumberOfColumns:numberOfColumns];
        [_textTableRows removeLastObject];
        [_textTableRows addObject:[NSNumber numberWithInteger:rowNumber]];
        [_textTableRowArrays removeLastObject];
        [_textTableRowArrays addObject:rowArray];
        if ([_textTableRowBackgroundColors count] > 0)
            [_textTableRowBackgroundColors removeLastObject];
    } else if (displayValue == "table-cell" && [_textBlocks count] > 0) {
        while ([_textTables count] > [_textBlocks count]) {
            [_textTables removeLastObject];
            [_textTableSpacings removeLastObject];
            [_textTablePaddings removeLastObject];
            [_textTableRows removeLastObject];
            [_textTableRowArrays removeLastObject];
        }
        [_textBlocks removeLastObject];
    } else if ((element.hasTagName(ulTag) || element.hasTagName(olTag)) && [_textLists count] > 0) {
        NSTextList *list = [_textLists lastObject];
        _addMarkersToList(list, range);
        [_textLists removeLastObject];
    } else if (element.hasTagName(qTag)) {
        _addQuoteForElement(element, NO, --_quoteLevel);
    } else if (element.hasTagName(spanTag)) {
        NSString *className = element.getAttribute(classAttr);
        NSMutableString *mutableString;
        NSUInteger i, count = 0;
        unichar c;
        if ([@"Apple-converted-space" isEqualToString:className]) {
            mutableString = [_attrStr mutableString];
            for (i = range.location; i < NSMaxRange(range); i++) {
                c = [mutableString characterAtIndex:i];
                if (0xa0 == c)
                    [mutableString replaceCharactersInRange:NSMakeRange(i, 1) withString:@" "];
            }
        } else if ([@"Apple-converted-tab" isEqualToString:className]) {
            mutableString = [_attrStr mutableString];
            for (i = range.location; i < NSMaxRange(range); i++) {
                NSRange rangeToReplace = NSMakeRange(NSNotFound, 0);
                c = [mutableString characterAtIndex:i];
                if (' ' == c || 0xa0 == c) {
                    count++;
                    if (count >= 4 || i + 1 >= NSMaxRange(range))
                        rangeToReplace = NSMakeRange(i + 1 - count, count);
                } else {
                    if (count > 0)
                        rangeToReplace = NSMakeRange(i - count, count);
                }
                if (rangeToReplace.length > 0) {
                    [mutableString replaceCharactersInRange:rangeToReplace withString:@"\t"];
                    range.length -= (rangeToReplace.length - 1);
                    i -= (rangeToReplace.length - 1);
                    if (NSMaxRange(rangeToReplace) <= _domRangeStartIndex) {
                        _domRangeStartIndex -= (rangeToReplace.length - 1);
                    } else if (rangeToReplace.location < _domRangeStartIndex) {
                        _domRangeStartIndex = rangeToReplace.location;
                    }
                    count = 0;
                }
            }
        }
    }
}

void HTMLConverter::_processText(CharacterData& characterData)
{
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    BOOL suppressLeadingSpace = ((_flags.isSoft && lastChar == ' ') || lastChar == '\n' || lastChar == '\r' || lastChar == '\t' || lastChar == NSParagraphSeparatorCharacter || lastChar == NSLineSeparatorCharacter || lastChar == NSFormFeedCharacter || lastChar == WebNextLineCharacter);
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    CFMutableStringRef mutstr = NULL;

    String originalString = characterData.data();
    unsigned startOffset = 0;
    unsigned endOffset = originalString.length();
    if (&characterData == m_start.containerNode()) {
        startOffset = m_start.offsetInContainerNode();
        _domRangeStartIndex = [_attrStr length];
        _flags.reachedStart = YES;
    }
    if (&characterData == m_end.containerNode()) {
        endOffset = m_end.offsetInContainerNode();
        _flags.reachedEnd = YES;
    }
    if ((startOffset > 0 || endOffset < originalString.length()) && endOffset >= startOffset)
        originalString = originalString.substring(startOffset, endOffset - startOffset);
    String outputString = originalString;

    // FIXME: Use RenderText's content instead.
    bool wasSpace = false;
    if (_caches->propertyValueForNode(characterData, CSSPropertyWhiteSpace).startsWith("pre")) {
        if (textLength && originalString.length() && _flags.isSoft) {
            unichar c = originalString.characterAt(0);
            if (c == '\n' || c == '\r' || c == NSParagraphSeparatorCharacter || c == NSLineSeparatorCharacter || c == NSFormFeedCharacter || c == WebNextLineCharacter)
                rangeToReplace = NSMakeRange(textLength - 1, 1);
        }
    } else {
        unsigned count = originalString.length();
        bool wasLeading = true;
        StringBuilder builder;
        LChar noBreakSpaceRepresentation = 0;
        for (unsigned i = 0; i < count; i++) {
            UChar c = originalString.characterAt(i);
            bool isWhitespace = c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == 0xc || c == 0x200b;
            if (isWhitespace)
                wasSpace = (!wasLeading || !suppressLeadingSpace);
            else {
                if (wasSpace)
                    builder.append(' ');
                if (c != noBreakSpace)
                    builder.append(c);
                else {
                    if (!noBreakSpaceRepresentation)
                        noBreakSpaceRepresentation = _caches->propertyValueForNode(characterData, CSSPropertyWebkitNbspMode) == "space" ? ' ' : noBreakSpace;
                    builder.append(noBreakSpaceRepresentation);
                }
                wasSpace = false;
                wasLeading = false;
            }
        }
        if (wasSpace)
            builder.append(' ');
        outputString = builder.toString();
    }

    if (outputString.length()) {
        String textTransform = _caches->propertyValueForNode(characterData, CSSPropertyTextTransform);
        if (textTransform == "capitalize")
            outputString = capitalize(outputString, ' '); // FIXME: Needs to take locale into account to work correctly.
        else if (textTransform == "uppercase")
            outputString = outputString.convertToUppercaseWithoutLocale(); // FIXME: Needs locale to work correctly.
        else if (textTransform == "lowercase")
            outputString = outputString.convertToLowercaseWithoutLocale(); // FIXME: Needs locale to work correctly.

        [_attrStr replaceCharactersInRange:rangeToReplace withString:outputString];
        rangeToReplace.length = outputString.length();
        if (rangeToReplace.length)
            [_attrStr setAttributes:aggregatedAttributesForAncestors(characterData) range:rangeToReplace];
        _flags.isSoft = wasSpace;
    }
    if (mutstr)
        CFRelease(mutstr);
}

void HTMLConverter::_traverseNode(Node& node, unsigned depth, bool embedded)
{
    if (_flags.reachedEnd)
        return;
    if (!_flags.reachedStart && !_caches->isAncestorsOfStartToBeConverted(node))
        return;

    unsigned startOffset = 0;
    unsigned endOffset = UINT_MAX;
    bool isStart = false;
    bool isEnd = false;
    if (&node == m_start.containerNode()) {
        startOffset = m_start.computeOffsetInContainerNode();
        isStart = true;
        _flags.reachedStart = YES;
    }
    if (&node == m_end.containerNode()) {
        endOffset = m_end.computeOffsetInContainerNode();
        isEnd = true;
    }
    
    if (node.isDocumentNode() || node.isDocumentFragment()) {
        Node* child = node.firstChild();
        ASSERT(child == firstChildInComposedTreeIgnoringUserAgentShadow(node));
        for (NSUInteger i = 0; child; i++) {
            if (isStart && i == startOffset)
                _domRangeStartIndex = [_attrStr length];
            if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                _traverseNode(*child, depth + 1, embedded);
            if (isEnd && i + 1 >= endOffset)
                _flags.reachedEnd = YES;
            if (_flags.reachedEnd)
                break;
            ASSERT(child->nextSibling() == nextSiblingInComposedTreeIgnoringUserAgentShadow(*child));
            child = child->nextSibling();
        }
    } else if (is<Element>(node)) {
        Element& element = downcast<Element>(node);
        if (_enterElement(element, embedded)) {
            NSUInteger startIndex = [_attrStr length];
            if (_processElement(element, depth)) {
                if (auto* shadowRoot = shadowRootIgnoringUserAgentShadow(element)) // Traverse through shadow root to detect start and end.
                    _traverseNode(*shadowRoot, depth + 1, embedded);
                else {
                    auto* child = firstChildInComposedTreeIgnoringUserAgentShadow(node);
                    for (NSUInteger i = 0; child; i++) {
                        if (isStart && i == startOffset)
                            _domRangeStartIndex = [_attrStr length];
                        if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                            _traverseNode(*child, depth + 1, embedded);
                        if (isEnd && i + 1 >= endOffset)
                            _flags.reachedEnd = YES;
                        if (_flags.reachedEnd)
                            break;
                        child = nextSiblingInComposedTreeIgnoringUserAgentShadow(*child);
                    }
                }
                _exitElement(element, depth, startIndex);
            }
        }
    } else if (node.nodeType() == Node::TEXT_NODE)
        _processText(downcast<CharacterData>(node));

    if (isEnd)
        _flags.reachedEnd = YES;
}

void HTMLConverter::_traverseFooterNode(Element& element, unsigned depth)
{
    if (_flags.reachedEnd)
        return;
    if (!_flags.reachedStart && !_caches->isAncestorsOfStartToBeConverted(element))
        return;

    unsigned startOffset = 0;
    unsigned endOffset = UINT_MAX;
    bool isStart = false;
    bool isEnd = false;
    if (&element == m_start.containerNode()) {
        startOffset = m_start.computeOffsetInContainerNode();
        isStart = true;
        _flags.reachedStart = YES;
    }
    if (&element == m_end.containerNode()) {
        endOffset = m_end.computeOffsetInContainerNode();
        isEnd = true;
    }
    
    if (_enterElement(element, YES)) {
        NSUInteger startIndex = [_attrStr length];
        if (_processElement(element, depth)) {
            auto* child = firstChildInComposedTreeIgnoringUserAgentShadow(element);
            for (NSUInteger i = 0; child; i++) {
                if (isStart && i == startOffset)
                    _domRangeStartIndex = [_attrStr length];
                if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                    _traverseNode(*child, depth + 1, true /* embedded */);
                if (isEnd && i + 1 >= endOffset)
                    _flags.reachedEnd = YES;
                if (_flags.reachedEnd)
                    break;
                child = nextSiblingInComposedTreeIgnoringUserAgentShadow(*child);
            }
            _exitElement(element, depth, startIndex);
        }
    }
    if (isEnd)
        _flags.reachedEnd = YES;
}

void HTMLConverter::_adjustTrailingNewline()
{
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : 0;
    BOOL alreadyHasTrailingNewline = (lastChar == '\n' || lastChar == '\r' || lastChar == NSParagraphSeparatorCharacter || lastChar == NSLineSeparatorCharacter || lastChar == WebNextLineCharacter);
    if (_flags.hasTrailingNewline && !alreadyHasTrailingNewline)
        [_attrStr replaceCharactersInRange:NSMakeRange(textLength, 0) withString:@"\n"];
}

Node* HTMLConverterCaches::cacheAncestorsOfStartToBeConverted(const Position& start, const Position& end)
{
    auto commonAncestor = commonInclusiveAncestor(start, end);
    Node* ancestor = start.containerNode();

    while (ancestor) {
        m_ancestorsUnderCommonAncestor.add(ancestor);
        if (ancestor == commonAncestor)
            break;
        ancestor = ancestor->parentInComposedTree();
    }

    return commonAncestor.get();
}

#if !PLATFORM(IOS_FAMILY)

static NSFileWrapper *fileWrapperForURL(DocumentLoader* dataSource, NSURL *URL)
{
    if ([URL isFileURL])
        return [[[NSFileWrapper alloc] initWithURL:[URL URLByResolvingSymlinksInPath] options:0 error:nullptr] autorelease];

    if (dataSource) {
        if (RefPtr<ArchiveResource> resource = dataSource->subresource(URL)) {
            NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:resource->data().createNSData().get()] autorelease];
            NSString *filename = resource->response().suggestedFilename();
            if (!filename || ![filename length])
                filename = suggestedFilenameWithMIMEType(resource->url(), resource->mimeType());
            [wrapper setPreferredFilename:filename];
            return wrapper;
        }
    }
    
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];

    NSCachedURLResponse *cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    
    if (cachedResponse) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
}

static RetainPtr<NSFileWrapper> fileWrapperForElement(HTMLImageElement& element)
{
    if (CachedImage* cachedImage = element.cachedImage()) {
        if (SharedBuffer* sharedBuffer = cachedImage->resourceBuffer())
            return adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:sharedBuffer->createNSData().get()]);
    }

    auto* renderer = element.renderer();
    if (is<RenderImage>(renderer)) {
        auto* image = downcast<RenderImage>(*renderer).cachedImage();
        if (image && !image->errorOccurred()) {
            RetainPtr<NSFileWrapper> wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:(__bridge NSData *)image->imageForRenderer(renderer)->tiffRepresentation()]);
            [wrapper setPreferredFilename:@"image.tiff"];
            return wrapper;
        }
    }

    return nil;
}

#endif

namespace WebCore {

// This function supports more HTML features than the editing variant below, such as tables.
AttributedString attributedString(const SimpleRange& range)
{
    return HTMLConverter { range }.convert();
}

#if PLATFORM(MAC)

// This function uses TextIterator, which makes offsets in its result compatible with HTML editing.
AttributedString editingAttributedString(const SimpleRange& range, IncludeImages includeImages)
{
    auto fontManager = [NSFontManager sharedFontManager];
    auto string = adoptNS([[NSMutableAttributedString alloc] init]);
    auto attrs = adoptNS([[NSMutableDictionary alloc] init]);
    NSUInteger stringLength = 0;
    for (TextIterator it(range); !it.atEnd(); it.advance()) {
        auto node = it.node();

        if (includeImages == IncludeImages::Yes && is<HTMLImageElement>(node)) {
            auto fileWrapper = fileWrapperForElement(downcast<HTMLImageElement>(*node));
            auto attachment = adoptNS([[NSTextAttachment alloc] initWithFileWrapper:fileWrapper.get()]);
            [string appendAttributedString:[NSAttributedString attributedStringWithAttachment:attachment.get()]];
        }

        auto currentTextLength = it.text().length();
        if (!currentTextLength)
            continue;

        // In some cases the text iterator emits text that is not associated with a node.
        // In those cases, base the style on the container.
        if (!node)
            node = it.range().start.container.ptr();
        auto renderer = node->renderer();
        ASSERT(renderer);
        if (!renderer)
            continue;
        auto& style = renderer->style();
        if (style.textDecorationsInEffect() & TextDecoration::Underline)
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        if (style.textDecorationsInEffect() & TextDecoration::LineThrough)
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
        if (auto font = style.fontCascade().primaryFont().getCTFont())
            [attrs setObject:toNSFont(font) forKey:NSFontAttributeName];
        else
            [attrs setObject:[fontManager convertFont:WebDefaultFont() toSize:style.fontCascade().primaryFont().platformData().size()] forKey:NSFontAttributeName];

        auto textAlignment = NSTextAlignmentNatural;
        switch (style.textAlign()) {
        case TextAlignMode::Right:
        case TextAlignMode::WebKitRight:
            textAlignment = NSTextAlignmentRight;
            break;
        case TextAlignMode::Left:
        case TextAlignMode::WebKitLeft:
            textAlignment = NSTextAlignmentLeft;
            break;
        case TextAlignMode::Center:
        case TextAlignMode::WebKitCenter:
            textAlignment = NSTextAlignmentCenter;
            break;
        case TextAlignMode::Justify:
            textAlignment = NSTextAlignmentJustified;
            break;
        case TextAlignMode::Start:
            if (style.hasExplicitlySetDirection())
                textAlignment = style.isLeftToRightDirection() ? NSTextAlignmentLeft : NSTextAlignmentRight;
            break;
        case TextAlignMode::End:
            textAlignment = style.isLeftToRightDirection() ? NSTextAlignmentRight : NSTextAlignmentLeft;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        if (textAlignment != NSTextAlignmentNatural) {
            auto paragraphStyle = adoptNS(NSParagraphStyle.defaultParagraphStyle.mutableCopy);
            [paragraphStyle setAlignment:textAlignment];
            [attrs setObject:paragraphStyle.get() forKey:NSParagraphStyleAttributeName];
        }

        Color foregroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
        if (foregroundColor.isVisible())
            [attrs setObject:nsColor(foregroundColor) forKey:NSForegroundColorAttributeName];
        else
            [attrs removeObjectForKey:NSForegroundColorAttributeName];

        Color backgroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        if (backgroundColor.isVisible())
            [attrs setObject:nsColor(backgroundColor) forKey:NSBackgroundColorAttributeName];
        else
            [attrs removeObjectForKey:NSBackgroundColorAttributeName];

        RetainPtr<NSString> text;
        if (style.nbspMode() == NBSPMode::Normal)
            text = it.text().createNSStringWithoutCopying();
        else
            text = it.text().toString().replace(noBreakSpace, ' ');

        [string replaceCharactersInRange:NSMakeRange(stringLength, 0) withString:text.get()];
        [string setAttributes:attrs.get() range:NSMakeRange(stringLength, currentTextLength)];
        stringLength += currentTextLength;
    }

    return { WTFMove(string), nil };
}

#endif
    
}
