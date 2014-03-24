/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#import "CachedImage.h"
#import "ColorMac.h"
#import "CSSComputedStyleDeclaration.h"
#import "CSSParser.h"
#import "CSSPrimitiveValue.h"
#import "Document.h"
#import "DocumentLoader.h"
#import "DOMCSSPrimitiveValueInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMHTMLTableCellElement.h"
#import "DOMNodeInternal.h"
#import "DOMPrivate.h"
#import "DOMRGBColorInternal.h"
#import "DOMRangeInternal.h"
#import "Element.h"
#import "Font.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "HTMLParserIdioms.h"
#import "LoaderNSURLExtras.h"
#import "RGBColor.h"
#import "RenderImage.h"
#import "SoftLinking.h"
#import "StyleProperties.h"
#import "StyledElement.h"
#import "TextIterator.h"
#import <objc/runtime.h>
#import <wtf/ASCIICType.h>

#if PLATFORM(IOS)

SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK_CLASS(UIKit, UIColor)

SOFT_LINK_PRIVATE_FRAMEWORK(UIFoundation)
SOFT_LINK_CLASS(UIFoundation, UIFont)
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

SOFT_LINK_CONSTANT(UIFoundation, NSFontAttributeName, NSString *)
#define NSFontAttributeName getNSFontAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSForegroundColorAttributeName, NSString *)
#define NSForegroundColorAttributeName getNSForegroundColorAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSBackgroundColorAttributeName, NSString *)
#define NSBackgroundColorAttributeName getNSBackgroundColorAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSStrokeColorAttributeName, NSString *)
#define NSStrokeColorAttributeName getNSStrokeColorAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSStrokeWidthAttributeName, NSString *)
#define NSStrokeWidthAttributeName getNSStrokeWidthAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSShadowAttributeName, NSString *)
#define NSShadowAttributeName getNSShadowAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSKernAttributeName, NSString *)
#define NSKernAttributeName getNSKernAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSLigatureAttributeName, NSString *)
#define NSLigatureAttributeName getNSLigatureAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSUnderlineStyleAttributeName, NSString *)
#define NSUnderlineStyleAttributeName getNSUnderlineStyleAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSStrikethroughStyleAttributeName, NSString *)
#define NSStrikethroughStyleAttributeName getNSStrikethroughStyleAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSBaselineOffsetAttributeName, NSString *)
#define NSBaselineOffsetAttributeName getNSBaselineOffsetAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSWritingDirectionAttributeName, NSString *)
#define NSWritingDirectionAttributeName getNSWritingDirectionAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSParagraphStyleAttributeName, NSString *)
#define NSParagraphStyleAttributeName getNSParagraphStyleAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSAttachmentAttributeName, NSString *)
#define NSAttachmentAttributeName getNSAttachmentAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSLinkAttributeName, NSString *)
#define NSLinkAttributeName getNSLinkAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSAuthorDocumentAttribute, NSString *)
#define NSAuthorDocumentAttribute getNSAuthorDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSEditorDocumentAttribute, NSString *)
#define NSEditorDocumentAttribute getNSEditorDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSGeneratorDocumentAttribute, NSString *)
#define NSGeneratorDocumentAttribute getNSGeneratorDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSCompanyDocumentAttribute, NSString *)
#define NSCompanyDocumentAttribute getNSCompanyDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSDisplayNameDocumentAttribute, NSString *)
#define NSDisplayNameDocumentAttribute getNSDisplayNameDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSCopyrightDocumentAttribute, NSString *)
#define NSCopyrightDocumentAttribute getNSCopyrightDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSSubjectDocumentAttribute, NSString *)
#define NSSubjectDocumentAttribute getNSSubjectDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSCommentDocumentAttribute, NSString *)
#define NSCommentDocumentAttribute getNSCommentDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSNoIndexDocumentAttribute, NSString *)
#define NSNoIndexDocumentAttribute getNSNoIndexDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSKeywordsDocumentAttribute, NSString *)
#define NSKeywordsDocumentAttribute getNSKeywordsDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSCreationTimeDocumentAttribute, NSString *)
#define NSCreationTimeDocumentAttribute getNSCreationTimeDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSModificationTimeDocumentAttribute, NSString *)
#define NSModificationTimeDocumentAttribute getNSModificationTimeDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSConvertedDocumentAttribute, NSString *)
#define NSConvertedDocumentAttribute getNSConvertedDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSCocoaVersionDocumentAttribute, NSString *)
#define NSCocoaVersionDocumentAttribute getNSCocoaVersionDocumentAttribute()

#define PlatformNSShadow            getNSShadowClass()
#define PlatformNSTextAttachment    getNSTextAttachmentClass()
#define PlatformNSParagraphStyle    getNSParagraphStyleClass()
#define PlatformNSTextList          getNSTextListClass()
#define PlatformNSTextTableBlock    getNSTextTableBlockClass()
#define PlatformNSTextTable         getNSTextTableClass()
#define PlatformNSTextTab           getNSTextTabClass()
#define PlatformColor               UIColor
#define PlatformColorClass          getUIColorClass()
#define PlatformNSColorClass        getNSColorClass()
#define PlatformFont                UIFont
#define PlatformFontClass           getUIFontClass()

// We don't softlink NSSuperscriptAttributeName because UIFoundation stopped exporting it.
// This attribute is being deprecated at the API level, but internally UIFoundation
// will continue to support it.
static NSString *const NSSuperscriptAttributeName = @"NSSuperscript";
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

#define NSTextAlignmentLeft         NSLeftTextAlignment
#define NSTextAlignmentRight        NSRightTextAlignment
#define NSTextAlignmentCenter       NSCenterTextAlignment
#define NSTextAlignmentJustified    NSJustifiedTextAlignment
#endif

using namespace WebCore;
using namespace HTMLNames;

#if PLATFORM(IOS)

typedef enum {
    UIFontTraitPlain       = 0x00000000,
    UIFontTraitItalic      = 0x00000001, // 1 << 0
    UIFontTraitBold        = 0x00000002, // 1 << 1
    UIFontTraitThin        = (1 << 2),
    UIFontTraitLight       = (1 << 3),
    UIFontTraitUltraLight  = (1 << 4)
} UIFontTrait;

typedef NS_ENUM(NSInteger, NSUnderlineStyle) {
    NSUnderlineStyleNone                                = 0x00,
    NSUnderlineStyleSingle                              = 0x01,
    NSUnderlineStyleThick NS_ENUM_AVAILABLE_IOS(7_0)    = 0x02,
    NSUnderlineStyleDouble NS_ENUM_AVAILABLE_IOS(7_0)   = 0x09,

    NSUnderlinePatternSolid NS_ENUM_AVAILABLE_IOS(7_0)      = 0x0000,
    NSUnderlinePatternDot NS_ENUM_AVAILABLE_IOS(7_0)        = 0x0100,
    NSUnderlinePatternDash NS_ENUM_AVAILABLE_IOS(7_0)       = 0x0200,
    NSUnderlinePatternDashDot NS_ENUM_AVAILABLE_IOS(7_0)    = 0x0300,
    NSUnderlinePatternDashDotDot NS_ENUM_AVAILABLE_IOS(7_0) = 0x0400,

    NSUnderlineByWord NS_ENUM_AVAILABLE_IOS(7_0) = 0x8000
};

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

typedef NS_ENUM(NSInteger, NSTextAlignment) {
    NSTextAlignmentLeft      = 0,    // Visually left aligned
    NSTextAlignmentCenter    = 1,    // Visually centered
    NSTextAlignmentRight     = 2,    // Visually right aligned
    NSTextAlignmentJustified = 3,    // Fully-justified. The last line in a paragraph is natural-aligned.
    NSTextAlignmentNatural   = 4,    // Indicates the default alignment for script
} NS_ENUM_AVAILABLE_IOS(6_0);

typedef NS_ENUM(NSInteger, NSWritingDirection) {
    NSWritingDirectionNatural       = -1,    // Determines direction using the Unicode Bidi Algorithm rules P2 and P3
    NSWritingDirectionLeftToRight   =  0,    // Left to right writing direction
    NSWritingDirectionRightToLeft   =  1     // Right to left writing direction
} NS_ENUM_AVAILABLE_IOS(6_0);

typedef NS_ENUM(NSInteger, NSTextWritingDirection) {
    NSTextWritingDirectionEmbedding     = (0 << 1),
    NSTextWritingDirectionOverride      = (1 << 1)
} NS_ENUM_AVAILABLE_IOS(7_0);

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
    NSAttachmentCharacter           = 0xFFFC // Replacement character is used for attachments
};

enum {
    NSLeftTabStopType = 0,
    NSRightTabStopType,
    NSCenterTabStopType,
    NSDecimalTabStopType
};
typedef NSUInteger NSTextTabType;

@interface UIColor : NSObject
+ (UIColor *)clearColor;
- (CGFloat)alphaComponent;
+ (UIColor *)_disambiguated_due_to_CIImage_colorWithCGColor:(CGColorRef)cgColor;
@end

@interface NSColor : UIColor
+ (id)colorWithCalibratedRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
@end

@interface UIFont
+ (UIFont *)fontWithName:(NSString *)fontName size:(CGFloat)fontSize;
+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;
- (NSString *)familyName;
- (CGFloat)pointSize;
- (UIFont *)fontWithSize:(CGFloat)fontSize;
+ (NSArray *)familyNames;
+ (NSArray *)fontNamesForFamilyName:(NSString *)familyName;
+ (UIFont *)systemFontOfSize:(CGFloat)fontSize;
@end

@interface NSTextTab
- (id)initWithType:(NSTextTabType)type location:(CGFloat)loc;
- (id)initWithTextAlignment:(NSTextAlignment)alignment location:(CGFloat)loc options:(NSDictionary *)options;
- (CGFloat)location;
- (void)release;
@end

@interface NSParagraphStyle : NSObject
+ (NSParagraphStyle *)defaultParagraphStyle;
- (void)setAlignment:(NSTextAlignment)alignment;
- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection;
- (void)setHeadIndent:(CGFloat)aFloat;
- (CGFloat)headIndent;
- (void)setHeaderLevel:(NSInteger)level;
- (void)setFirstLineHeadIndent:(CGFloat)aFloat;
- (void)setTailIndent:(CGFloat)aFloat;
- (void)setParagraphSpacing:(CGFloat)paragraphSpacing;
- (void)setTextLists:(NSArray *)array;
- (void)setTextBlocks:(NSArray *)array;
- (void)setMinimumLineHeight:(CGFloat)aFloat;
- (NSArray *)textLists;
- (void)removeTabStop:(NSTextTab *)anObject;
- (void)addTabStop:(NSTextTab *)anObject;
- (NSArray *)tabStops;
- (void)setHyphenationFactor:(float)aFactor;
@end

@interface NSShadow
- (void)setShadowOffset:(CGSize)size;
- (void)setShadowBlurRadius:(CGFloat)radius;
- (void)setShadowColor:(UIColor *)color;
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

@interface NSTextList
- (id)initWithMarkerFormat:(NSString *)format options:(NSUInteger)mask;
- (void)setStartingItemNumber:(NSInteger)itemNum;
- (NSInteger)startingItemNumber;
- (NSString *)markerForItemNumber:(NSInteger)itemNum;
- (void)release;
@end

@interface NSMutableParagraphStyle : NSParagraphStyle
- (void)setDefaultTabInterval:(CGFloat)aFloat;
- (void)setTabStops:(NSArray *)array;
@end

@interface NSTextAttachment : NSObject
- (id)initWithFileWrapper:(NSFileWrapper *)fileWrapper;
#if PLATFORM(IOS)
- (void)setBounds:(CGRect)bounds;
#endif
- (void)release;
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
static NSFileWrapper *fileWrapperForElement(Element*);

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
public:
    String propertyValueForNode(Node&, CSSPropertyID );
    bool floatPropertyValueForNode(Node&, CSSPropertyID, float&);
    Color colorPropertyValueForNode(Node&, CSSPropertyID);

    bool isBlockElement(Element&);
    bool elementHasOwnBackgroundColor(Element&);

    PassRefPtr<CSSValue> computedStylePropertyForElement(Element&, CSSPropertyID);
    PassRefPtr<CSSValue> inlineStylePropertyForElement(Element&, CSSPropertyID);

private:
    HashMap<Element*, std::unique_ptr<ComputedStyleExtractor>> m_computedStyles;
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
    HTMLConverter(DOMRange*);
    ~HTMLConverter();
    
    NSAttributedString* convert()
    {
        _loadFromDOMRange();
        if (_errorCode)
            return nil;
        return [[_attrStr retain] autorelease];
    }
    
private:
    NSMutableAttributedString *_attrStr;
    NSMutableDictionary *_documentAttrs;
    NSURL *_baseURL;
    DOMDocument *_document;
    DOMRange *_domRange;
    NSMutableArray *_domStartAncestors;
    WebCore::DocumentLoader *_dataSource;
    NSMutableArray *_textLists;
    NSMutableArray *_textBlocks;
    NSMutableArray *_textTables;
    NSMutableDictionary *_textTableFooters;
    NSMutableArray *_textTableSpacings;
    NSMutableArray *_textTablePaddings;
    NSMutableArray *_textTableRows;
    NSMutableArray *_textTableRowArrays;
    NSMutableArray *_textTableRowBackgroundColors;
    NSMutableDictionary *_attributesForElements;
    NSMutableDictionary *_fontCache;
    NSMutableArray *_writingDirectionArray;
    
    CGFloat _defaultTabInterval;
    NSUInteger _domRangeStartIndex;
    NSInteger _errorCode;
    NSInteger _quoteLevel;

    std::unique_ptr<HTMLConverterCaches> _caches;

    struct {
        unsigned int isSoft:1;
        unsigned int reachedStart:1;
        unsigned int reachedEnd:1;
        unsigned int hasTrailingNewline:1;
        unsigned int pad:26;
    } _flags;
    
    void _loadFromDOMRange();
    
    NSString *_stringForNode(DOMNode *, CSSPropertyID);
    PlatformColor *_colorForNode(DOMNode *, CSSPropertyID);
    BOOL _getFloat(CGFloat *val, DOMNode *, CSSPropertyID);

    void _traverseNode(DOMNode *node, NSInteger depth, BOOL embedded);
    void _traverseFooterNode(DOMNode *node, NSInteger depth);
    
    NSDictionary *_computedAttributesForElement(DOMElement *);
    NSDictionary *_attributesForElement(DOMElement *);
    
    bool _elementIsBlockLevel(DOMElement *);
    bool _elementHasOwnBackgroundColor(DOMElement *);
    DOMElement * _blockLevelElementForNode(DOMNode *);
    
    void _newParagraphForElement(DOMElement *element, NSString *tag, BOOL flag, BOOL suppressTrailingSpace);
    void _newLineForElement(DOMElement *element);
    void _newTabForElement(DOMElement *element);
    BOOL _addAttachmentForElement(DOMElement *element, NSURL *url, BOOL needsParagraph, BOOL usePlaceholder);
    void _addQuoteForElement(DOMElement *element, BOOL opening, NSInteger level);
    void _addValue(NSString *value, DOMElement *element);
    void _fillInBlock(NSTextBlock *block, DOMElement *element, PlatformColor *backgroundColor, CGFloat extraMargin, CGFloat extraPadding, BOOL isTable);
    void _processMetaElementWithName(NSString *name, NSString *content);
    void _processHeadElement(DOMElement *element);
    BOOL _enterElement(DOMElement *element, NSString *tag, NSString *displayVal, BOOL embedded);
    void _addTableForElement(DOMElement *tableElement);
    void _addTableCellForElement(DOMElement *tableCellElement);
    BOOL _processElement(DOMElement *element, NSString *tag, NSString *displayVal, NSInteger depth);
    void _addMarkersToList(NSTextList *list, NSRange range);
    void _exitElement(DOMElement *element, NSString *tag, NSString *displayVal, NSInteger depth, NSUInteger startIndex);
    void _processText(DOMCharacterData *text);
    void _adjustTrailingNewline();
};

HTMLConverter::HTMLConverter(DOMRange* domRange)
{
    _domRange = [domRange retain];
    _attrStr = [[NSMutableAttributedString alloc] init];
    _documentAttrs = [[NSMutableDictionary alloc] init];
    _baseURL = nil;
    _document = nil;
    _domStartAncestors = nil;
    _dataSource = nullptr;
    _textLists = [[NSMutableArray alloc] init];
    _textBlocks = [[NSMutableArray alloc] init];
    _textTables = [[NSMutableArray alloc] init];
    _textTableFooters = [[NSMutableDictionary alloc] init];
    _textTableSpacings = [[NSMutableArray alloc] init];
    _textTablePaddings = [[NSMutableArray alloc] init];
    _textTableRows = [[NSMutableArray alloc] init];
    _textTableRowArrays = [[NSMutableArray alloc] init];
    _textTableRowBackgroundColors = [[NSMutableArray alloc] init];
    _attributesForElements = [[NSMutableDictionary alloc] init];
    _fontCache = [[NSMutableDictionary alloc] init];
    _writingDirectionArray = [[NSMutableArray alloc] init];

    _defaultTabInterval = 36;
    _domRangeStartIndex = 0;
    _errorCode = -1;
    _quoteLevel = 0;
    
    _flags.isSoft = false;
    _flags.reachedStart = false;
    _flags.reachedEnd = false;
    
    _caches = std::make_unique<HTMLConverterCaches>();
}

HTMLConverter::~HTMLConverter()
{
    [_attrStr release];
    [_documentAttrs release];
    [_domRange release];
    [_domStartAncestors release];
    [_textLists release];
    [_textBlocks release];
    [_textTables release];
    [_textTableFooters release];
    [_textTableSpacings release];
    [_textTablePaddings release];
    [_textTableRows release];
    [_textTableRowArrays release];
    [_textTableRowBackgroundColors release];
    [_attributesForElements release];
    [_fontCache release];
    [_writingDirectionArray release];
}

#if !PLATFORM(IOS)
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
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
                NSString *faceName = [fontName substringFromIndex:(dividingRange.location + dividingRange.length)];
                NSArray *familyMemberFaceNames = [PlatformFontClass fontNamesForFamilyName:familyName];
                for (NSString *familyMemberFaceName in familyMemberFaceNames) {
                    if ([familyMemberFaceName compare:faceName options:NSCaseInsensitiveSearch] == NSOrderedSame) {
                        font = [PlatformFontClass fontWithName:familyMemberFaceName size:size];
                        break;
                    }
                }
                if (!font && [familyMemberFaceNames count])
                    font = [getUIFontClass() fontWithName:familyName size:size];
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
#if PLATFORM(IOS)
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
        [defaultParagraphStyle setTabStops:[NSArray array]];
    }
    return defaultParagraphStyle;
}


static NSArray *_childrenForNode(DOMNode *node)
{
    NSMutableArray *array = [NSMutableArray array];
    DOMNode *child = [node firstChild];
    while (child) {
        [array addObject:child];
        child = [child nextSibling];
    }
    return array;
}

PassRefPtr<CSSValue> HTMLConverterCaches::computedStylePropertyForElement(Element& element, CSSPropertyID propertyId)
{
    if (propertyId == CSSPropertyInvalid)
        return nullptr;

    auto result = m_computedStyles.add(&element, nullptr);
    if (result.isNewEntry)
        result.iterator->value = std::make_unique<ComputedStyleExtractor>(&element, true);
    ComputedStyleExtractor& computedStyle = *result.iterator->value;
    return computedStyle.propertyValue(propertyId);
}

PassRefPtr<CSSValue> HTMLConverterCaches::inlineStylePropertyForElement(Element& element, CSSPropertyID propertyId)
{
    if (propertyId == CSSPropertyInvalid || !element.isStyledElement())
        return nullptr;
    const StyleProperties* properties = toStyledElement(element).inlineStyle();
    if (!properties)
        return nullptr;
    return properties->getPropertyCSSValue(propertyId);
}

static bool stringFromCSSValue(CSSValue& value, String& result)
{
    if (value.isPrimitiveValue()) {
        unsigned short primitiveType = toCSSPrimitiveValue(value).primitiveType();
        if (primitiveType == CSSPrimitiveValue::CSS_STRING || primitiveType == CSSPrimitiveValue::CSS_URI ||
            primitiveType == CSSPrimitiveValue::CSS_IDENT || primitiveType == CSSPrimitiveValue::CSS_ATTR) {
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
    if (!node.isElementNode()) {
        if (Node* parent = node.parentNode())
            return propertyValueForNode(*parent, propertyId);
        return String();
    }

    bool inherit = false;
    Element& element = toElement(node);
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
    case CSSPropertyFontVariant:
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
        if (Node* parent = node.parentNode())
            return propertyValueForNode(*parent, propertyId);
    }
    
    return String();
}

NSString *HTMLConverter::_stringForNode(DOMNode *node, CSSPropertyID propertyId)
{
    Node* coreNode = core(node);
    if (!coreNode)
        return nil;
    String result = _caches->propertyValueForNode(*coreNode, propertyId);
    if (!result.length())
        return nil;
    return result;
}

static inline bool floatValueFromPrimitiveValue(CSSPrimitiveValue& primitiveValue, float& result)
{
    // FIXME: Use CSSPrimitiveValue::computeValue.
    switch (primitiveValue.primitiveType()) {
    case CSSPrimitiveValue::CSS_PX:
        result = primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_PX);
        return true;
    case CSSPrimitiveValue::CSS_PT:
        result = 4 * primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_PT) / 3;
        return true;
    case CSSPrimitiveValue::CSS_PC:
        result = 16 * primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_PC);
        return true;
    case CSSPrimitiveValue::CSS_CM:
        result = 96 * primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_PC) / 2.54;
        return true;
    case CSSPrimitiveValue::CSS_MM:
        result = 96 * primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_PC) / 25.4;
        return true;
    case CSSPrimitiveValue::CSS_IN:
        result = 96 * primitiveValue.getFloatValue(CSSPrimitiveValue::CSS_IN);
        return true;
    default:
        return false;
    }
}

bool HTMLConverterCaches::floatPropertyValueForNode(Node& node, CSSPropertyID propertyId, float& result)
{
    if (!node.isElementNode()) {
        if (ContainerNode* parent = node.parentNode())
            return floatPropertyValueForNode(*parent, propertyId, result);
        return false;
    }

    Element& element = toElement(node);
    if (RefPtr<CSSValue> value = computedStylePropertyForElement(element, propertyId)) {
        if (value->isPrimitiveValue() && floatValueFromPrimitiveValue(toCSSPrimitiveValue(*value), result))
            return true;
    }

    bool inherit = false;
    if (RefPtr<CSSValue> value = inlineStylePropertyForElement(element, propertyId)) {
        if (value->isPrimitiveValue() && floatValueFromPrimitiveValue(toCSSPrimitiveValue(*value), result))
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
        if (ContainerNode* parent = node.parentNode())
            return floatPropertyValueForNode(*parent, propertyId, result);
    }

    return false;
}

BOOL HTMLConverter::_getFloat(CGFloat *val, DOMNode *node, CSSPropertyID propertyId)
{
    Node* coreNode = core(node);
    if (!coreNode)
        return NO;
    float result;
    if (!_caches->floatPropertyValueForNode(*coreNode, propertyId, result))
        return NO;
    if (val)
        *val = result;
    return YES;
}


#if PLATFORM(IOS)
static NSString *_NSFirstPathForDirectoriesInDomains(NSSearchPathDirectory directory, NSSearchPathDomainMask domainMask, BOOL expandTilde)
{
    NSArray *array = NSSearchPathForDirectoriesInDomains(directory, domainMask, expandTilde);
    return [array count] >= 1 ? [array objectAtIndex:0] : nil;
}

static NSString *_NSSystemLibraryPath(void)
{
    return _NSFirstPathForDirectoriesInDomains(NSLibraryDirectory, NSSystemDomainMask, YES);
}

static NSBundle *_webKitBundle()
{
    // FIXME: This should probably use the WebCore bundle to avoid the layering violation.
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    if (!bundle)
        bundle = [NSBundle bundleWithPath:[_NSSystemLibraryPath() stringByAppendingPathComponent:@"Frameworks/WebKit.framework"]];
    return bundle;
}

static inline UIColor *_platformColor(Color color)
{
    return [getUIColorClass() _disambiguated_due_to_CIImage_colorWithCGColor:cachedCGColor(color, WebCore::ColorSpaceDeviceRGB)];
}
#else
static inline NSColor *_platformColor(Color color)
{
    return nsColor(color);
}
#endif

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
#if PLATFORM(IOS)
                shadowOffset.height = shadowHeight;
#else
                shadowOffset.height = -shadowHeight;
#endif
                spaceRange = [shadowStyle rangeOfString:@" " options:NSBackwardsSearch range:NSMakeRange(0, thirdRange.location)];
                if (!spaceRange.length)
                    spaceRange = NSMakeRange(0, 0);
                shadowBlurRadius = [[shadowStyle substringWithRange:NSMakeRange(NSMaxRange(spaceRange), thirdRange.location - NSMaxRange(spaceRange))] floatValue];
                shadow = [[[PlatformNSShadow alloc] init] autorelease];
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

bool HTMLConverter::_elementIsBlockLevel(DOMElement *element)
{
    return element && _caches->isBlockElement(*core(element));
}

bool HTMLConverter::_elementHasOwnBackgroundColor(DOMElement *element)
{
    return element && _caches->elementHasOwnBackgroundColor(*core(element));
}

DOMElement * HTMLConverter::_blockLevelElementForNode(DOMNode *node)
{
    DOMElement *element = (DOMElement *)node;
    while (element && [element nodeType] != DOM_ELEMENT_NODE)
        element = (DOMElement *)[element parentNode];
    if (element && !_elementIsBlockLevel(element))
        element = _blockLevelElementForNode([element parentNode]);
    return element;
}

static Color normalizedColor(Color color, bool ignoreBlack)
{
    const double ColorEpsilon = 1 / (2 * (double)255.0);
    
    double red, green, blue, alpha;
    color.getRGBA(red, green, blue, alpha);
    if (red < ColorEpsilon && green < ColorEpsilon && blue < ColorEpsilon && (ignoreBlack || alpha < ColorEpsilon))
        return Color();
    
    return color;
}

Color HTMLConverterCaches::colorPropertyValueForNode(Node& node, CSSPropertyID propertyId)
{
    if (!node.isElementNode()) {
        if (Node* parent = node.parentNode())
            return colorPropertyValueForNode(*parent, propertyId);
        return Color();
    }

    Element& element = toElement(node);
    if (RefPtr<CSSValue> value = computedStylePropertyForElement(element, propertyId)) {
        if (value->isPrimitiveValue() && toCSSPrimitiveValue(*value).isRGBColor())
            return normalizedColor(Color(toCSSPrimitiveValue(*value).getRGBA32Value()), propertyId == CSSPropertyColor);
    }

    bool inherit = false;
    if (RefPtr<CSSValue> value = inlineStylePropertyForElement(element, propertyId)) {
        if (value->isPrimitiveValue() && toCSSPrimitiveValue(*value).isRGBColor())
            return normalizedColor(Color(toCSSPrimitiveValue(*value).getRGBA32Value()), propertyId == CSSPropertyColor);
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
        if (Node* parent = node.parentNode())
            return colorPropertyValueForNode(*parent, propertyId);
    }

    return Color();
}

PlatformColor *HTMLConverter::_colorForNode(DOMNode *node, CSSPropertyID propertyId)
{
    Node* coreNode = core(node);
    if (!coreNode)
        return nil;
    Color result = _caches->colorPropertyValueForNode(*coreNode, propertyId);
    if (!result.isValid())
        return nil;
    PlatformColor *platformResult = _platformColor(result);
    if ([[PlatformColorClass clearColor] isEqual:platformResult] || ([platformResult alphaComponent] == 0.0))
        return nil;
    return platformResult;
}

#define UIFloatIsZero(number) (fabs(number - 0) < FLT_EPSILON)

NSDictionary *HTMLConverter::_computedAttributesForElement(DOMElement *element)
{
    DOMElement *blockElement = _blockLevelElementForNode(element);
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
#if !PLATFORM(IOS)
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
#endif
    NSString *textDecoration = _stringForNode(element, CSSPropertyTextDecoration);
    NSString *verticalAlign = _stringForNode(element, CSSPropertyVerticalAlign);
    NSString *textShadow = _stringForNode(element, CSSPropertyTextShadow);
    NSString *fontLigatures = _stringForNode(element, CSSPropertyWebkitFontVariantLigatures);
    NSString *fontKerning = _stringForNode(element, CSSPropertyWebkitFontKerning);
    NSString *letterSpacing = _stringForNode(element, CSSPropertyLetterSpacing);
    CGFloat fontSize = 0;
    CGFloat baselineOffset = 0;
    CGFloat strokeWidth = 0.0;
    PlatformFont *font = nil;
    PlatformFont *actualFont = (PlatformFont *)[element _font];
    PlatformColor *foregroundColor = _colorForNode(element, CSSPropertyColor);
    PlatformColor *backgroundColor = _colorForNode(element, CSSPropertyBackgroundColor);
    PlatformColor *strokeColor = _colorForNode(element, CSSPropertyWebkitTextStrokeColor);

    if (!_getFloat(&fontSize, element, CSSPropertyFontSize) || fontSize <= 0.0)
        fontSize = defaultFontSize;
    if (fontSize < minimumFontSize)
        fontSize = minimumFontSize;
    if (fabs(floor(2.0 * fontSize + 0.5) / 2.0 - fontSize) < 0.05)
        fontSize = (CGFloat)floor(2.0 * fontSize + 0.5) / 2;
    else if (fabs(floor(10.0 * fontSize + 0.5) / 10.0 - fontSize) < 0.005)
        fontSize = (CGFloat)floor(10.0 * fontSize + 0.5) / 10;

    if (fontSize <= 0.0)
        fontSize = defaultFontSize;
    
#if PLATFORM(IOS)
    if (actualFont)
        font = [actualFont fontWithSize:fontSize];
#else
    if (actualFont)
        font = [fontManager convertFont:actualFont toSize:fontSize];
#endif
    if (!font) {
        NSString *fontName = [_stringForNode(element, CSSPropertyFontFamily) capitalizedString];
        NSString *fontStyle = _stringForNode(element, CSSPropertyFontStyle);
        NSString *fontWeight = _stringForNode(element, CSSPropertyFontWeight);
#if !PLATFORM(IOS)
        NSString *fontVariant = _stringForNode(element, CSSPropertyFontVariant);
#endif
        if (fontName)
            font = _fontForNameAndSize(fontName, fontSize, _fontCache);
        if (!font)
            font = [PlatformFontClass fontWithName:@"Times" size:fontSize];
        if ([@"italic" isEqualToString:fontStyle] || [@"oblique" isEqualToString:fontStyle]) {
            PlatformFont *originalFont = font;
#if PLATFORM(IOS)
            font = [PlatformFontClass fontWithFamilyName:[font familyName] traits:UIFontTraitItalic size:[font pointSize]];
#else
            font = [fontManager convertFont:font toHaveTrait:NSItalicFontMask];
#endif
            if (!font)
                font = originalFont;
        }
        if ([fontWeight hasPrefix:@"bold"] || [fontWeight integerValue] >= 700) {
            // ??? handle weight properly using NSFontManager
            PlatformFont *originalFont = font;
#if PLATFORM(IOS)
            font = [PlatformFontClass fontWithFamilyName:[font familyName] traits:UIFontTraitBold size:[font pointSize]];
#else
            font = [fontManager convertFont:font toHaveTrait:NSBoldFontMask];
#endif
            if (!font)
                font = originalFont;
        }
#if !PLATFORM(IOS) // IJB: No small caps support on iOS
        if ([@"small-caps" isEqualToString:fontVariant]) {
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
    if (backgroundColor && !_elementHasOwnBackgroundColor(element))
        [attrs setObject:backgroundColor forKey:NSBackgroundColorAttributeName];

    if (_getFloat(&strokeWidth, element, CSSPropertyWebkitTextStrokeWidth)) {
        float textStrokeWidth = strokeWidth / ([font pointSize] * 0.01);
        [attrs setObject:[NSNumber numberWithDouble:textStrokeWidth] forKey:NSStrokeWidthAttributeName];
    }
    if (strokeColor)
        [attrs setObject:strokeColor forKey:NSStrokeColorAttributeName];
    if (fontKerning || letterSpacing) {
        if ([fontKerning rangeOfString:@"none"].location != NSNotFound)
            [attrs setObject:@0.0 forKey:NSKernAttributeName];
        else {
            double kernVal = letterSpacing ? [letterSpacing doubleValue] : 0.0;
            if (UIFloatIsZero(kernVal))
                [attrs setObject:@0.0 forKey:NSKernAttributeName]; // auto and normal, the other possible values, are both "kerning enabled"
            else
                [attrs setObject:[NSNumber numberWithDouble:kernVal] forKey:NSKernAttributeName];
        }
    }
    if (fontLigatures) {
        if ([fontLigatures rangeOfString:@"normal"].location != NSNotFound)
            ;   // default: whatever the system decides to do
        else if ([fontLigatures rangeOfString:@"common-ligatures"].location != NSNotFound)
            [attrs setObject:@1 forKey:NSLigatureAttributeName];   // explicitly enabled
        else if ([fontLigatures rangeOfString:@"no-common-ligatures"].location != NSNotFound)
            [attrs setObject:@0 forKey:NSLigatureAttributeName];  // explicitly disabled
    }

    if (textDecoration && [textDecoration length] > 4) {
        if ([textDecoration rangeOfString:@"underline"].location != NSNotFound)
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        if ([textDecoration rangeOfString:@"line-through"].location != NSNotFound)
            [attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
    }
    if (verticalAlign) {
        if ([verticalAlign rangeOfString:@"super"].location != NSNotFound)
            [attrs setObject:[NSNumber numberWithInteger:1] forKey:NSSuperscriptAttributeName];
        if ([verticalAlign rangeOfString:@"sub"].location != NSNotFound)
            [attrs setObject:[NSNumber numberWithInteger:-1] forKey:NSSuperscriptAttributeName];
    }
    if (_getFloat(&baselineOffset, element, CSSPropertyVerticalAlign))
        [attrs setObject:[NSNumber numberWithDouble:baselineOffset] forKey:NSBaselineOffsetAttributeName];
    if (textShadow && [textShadow length] > 4) {
        NSShadow *shadow = _shadowForShadowStyle(textShadow);
        if (shadow)
            [attrs setObject:shadow forKey:NSShadowAttributeName];
    }
    if (element != blockElement && [_writingDirectionArray count] > 0)
        [attrs setObject:[NSArray arrayWithArray:_writingDirectionArray] forKey:NSWritingDirectionAttributeName];
    
    if (blockElement) {
        NSMutableParagraphStyle *paragraphStyle = [defaultParagraphStyle() mutableCopy];
        NSString *blockTag = [blockElement tagName];
        BOOL isParagraph = ([@"P" isEqualToString:blockTag] || [@"LI" isEqualToString:blockTag] || ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]));
        NSString *textAlign = _stringForNode(blockElement, CSSPropertyTextAlign);
        NSString *direction = _stringForNode(blockElement, CSSPropertyDirection);
        NSString *hyphenation = _stringForNode(blockElement, CSSPropertyWebkitHyphens);
        CGFloat leftMargin = 0;
        CGFloat rightMargin = 0;
        CGFloat bottomMargin = 0;
        CGFloat textIndent = 0;
        if (textAlign) {
            // WebKit can return -khtml-left, -khtml-right, -khtml-center
            if ([textAlign hasSuffix:@"left"])
                [paragraphStyle setAlignment:NSTextAlignmentLeft];
            else if ([textAlign hasSuffix:@"right"])
                [paragraphStyle setAlignment:NSTextAlignmentRight];
            else if ([textAlign hasSuffix:@"center"])
                [paragraphStyle setAlignment:NSTextAlignmentCenter];
            else if ([textAlign hasSuffix:@"justify"])
                [paragraphStyle setAlignment:NSTextAlignmentJustified];
        }
        if (direction) {
            if ([direction isEqualToString:@"ltr"])
                [paragraphStyle setBaseWritingDirection:NSWritingDirectionLeftToRight];
            else if ([direction isEqualToString:@"rtl"])
                [paragraphStyle setBaseWritingDirection:NSWritingDirectionRightToLeft];
        }
        if(hyphenation) {
            if ([hyphenation isEqualToString:@"auto"])
                [paragraphStyle setHyphenationFactor:1.0];
            else
                [paragraphStyle setHyphenationFactor:0.0];
        }
        if ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]) {
            NSInteger headerLevel = [blockTag characterAtIndex:1] - '0';
            if (1 <= headerLevel && headerLevel <= 6)
                [paragraphStyle setHeaderLevel:headerLevel];
        }
        if (isParagraph) {
            if (_getFloat(&leftMargin, blockElement, CSSPropertyMarginLeft) && leftMargin > 0.0)
                [paragraphStyle setHeadIndent:leftMargin];
            if (_getFloat(&textIndent, blockElement, CSSPropertyTextIndent))
                [paragraphStyle setFirstLineHeadIndent:[paragraphStyle headIndent] + textIndent];
            if (_getFloat(&rightMargin, blockElement, CSSPropertyMarginRight) && rightMargin > 0.0)
                [paragraphStyle setTailIndent:-rightMargin];
            if (_getFloat(&bottomMargin, blockElement, CSSPropertyMarginBottom) && bottomMargin > 0.0)
                [paragraphStyle setParagraphSpacing:bottomMargin];
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

NSDictionary *HTMLConverter::_attributesForElement(DOMElement *element)
{
    NSDictionary *result;
    if (element) {
        result = [_attributesForElements objectForKey:element];
        if (!result) {
            result = _computedAttributesForElement(element);
            [_attributesForElements setObject:result forKey:element];
        }
    } else
        result = [NSDictionary dictionary];
    return result;

}

void HTMLConverter::_newParagraphForElement(DOMElement *element, NSString *tag, BOOL flag, BOOL suppressTrailingSpace)
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
        NSDictionary *attrs = _attributesForElement(element);
        if (rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs range:rangeToReplace];
        _flags.isSoft = YES;
    }
}

void HTMLConverter::_newLineForElement(DOMElement *element)
{
    unichar c = NSLineSeparatorCharacter;
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    NSDictionary *attrs = _attributesForElement(element);
    if (rangeToReplace.length > 0)
        [_attrStr setAttributes:attrs range:rangeToReplace];
    _flags.isSoft = YES;
}

void HTMLConverter::_newTabForElement(DOMElement *element)
{
    NSString *string = @"\t";
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    NSRange rangeToReplace = (_flags.isSoft && lastChar == ' ') ? NSMakeRange(textLength - 1, 1) : NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    NSDictionary *attrs = _attributesForElement(element);
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
        _WebMessageDocumentClass = objc_lookUpClass("MFWebMessageDocument");
        if (_WebMessageDocumentClass && ![_WebMessageDocumentClass respondsToSelector:@selector(document:attachment:forURL:)])
            _WebMessageDocumentClass = Nil;
        lookedUpClass = YES;
    }
    return _WebMessageDocumentClass;
}

BOOL HTMLConverter::_addAttachmentForElement(DOMElement *element, NSURL *url, BOOL needsParagraph, BOOL usePlaceholder)
{
    BOOL retval = NO, notFound = NO;
    NSFileWrapper *fileWrapper = nil;
    Frame* frame = core([element ownerDocument])->frame();
    DocumentLoader *dataSource = frame->loader().frameHasLoaded() ? frame->loader().documentLoader() : 0;
    BOOL ignoreOrientation = YES;

    if ([url isFileURL]) {
        NSString *path = [[url path] stringByStandardizingPath];
        if (path)
            fileWrapper = [[[NSFileWrapper alloc] initWithURL:url options:0 error:NULL] autorelease];
    }
    if (!fileWrapper) {
        RefPtr<ArchiveResource> resource = dataSource->subresource(url);
        if (!resource)
            resource = dataSource->subresource(url);

        const String& mimeType = resource->mimeType();
        if (usePlaceholder && resource && mimeType == "text/html")
            notFound = YES;
        if (resource && !notFound) {
            fileWrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:resource->data()->createNSData().get()] autorelease];
            [fileWrapper setPreferredFilename:suggestedFilenameWithMIMEType(url, mimeType)];
        }
    }
#if !PLATFORM(IOS)
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(dataSource, url);
        if (usePlaceholder && fileWrapper && [[[[fileWrapper preferredFilename] pathExtension] lowercaseString] hasPrefix:@"htm"])
            notFound = YES;
        if (notFound)
            fileWrapper = nil;
    }
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(_dataSource, url);
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
#if PLATFORM(IOS)
        NSString *vAlign = _stringForNode(element, CSSPropertyVerticalAlign);
        attachment.get().bounds = CGRectMake(0, ([vAlign floatValue] / 100.) * element.clientHeight, element.clientWidth, element.clientHeight);
#endif
        RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithFormat:(needsParagraph ? @"%C\n" : @"%C"), static_cast<unichar>(NSAttachmentCharacter)]);
        NSRange rangeToReplace = NSMakeRange(textLength, 0);
        NSDictionary *attrs;
        if (fileWrapper) {
#if !PLATFORM(IOS)
            if (ignoreOrientation)
                [attachment setIgnoresOrientation:YES];
#endif
        } else {
#if PLATFORM(IOS)
            [attachment release];
            NSURL *missingImageURL = [_webKitBundle() URLForResource:@"missing_image" withExtension:@"tiff"];
            ASSERT_WITH_MESSAGE(missingImageURL != nil, "Unable to find missing_image.tiff!");
            NSFileWrapper *missingImageFileWrapper = [[[NSFileWrapper alloc] initWithURL:missingImageURL options:0 error:NULL] autorelease];
            attachment = [[PlatformNSTextAttachment alloc] initWithFileWrapper:missingImageFileWrapper];
#else
            static NSImage *missingImage = nil;
            NSTextAttachmentCell *cell;
            cell = [[NSTextAttachmentCell alloc] initImageCell:missingImage];
            [attachment setAttachmentCell:cell];
            [cell release];
#endif
        }
        [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
        rangeToReplace.length = [string length];
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += rangeToReplace.length;
        attrs = _attributesForElement(element);
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

void HTMLConverter::_addQuoteForElement(DOMElement *element, BOOL opening, NSInteger level)
{
    unichar c = ((level % 2) == 0) ? (opening ? 0x201c : 0x201d) : (opening ? 0x2018 : 0x2019);
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    RetainPtr<NSDictionary> attrs = _attributesForElement(element);
    if (rangeToReplace.length > 0)
        [_attrStr setAttributes:attrs.get() range:rangeToReplace];
    _flags.isSoft = NO;
}

void HTMLConverter::_addValue(NSString *value, DOMElement *element)
{
    NSUInteger textLength = [_attrStr length];
    NSUInteger valueLength = [value length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    if (valueLength) {
        [_attrStr replaceCharactersInRange:rangeToReplace withString:value];
        rangeToReplace.length = valueLength;
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += rangeToReplace.length;
        RetainPtr<NSDictionary> attrs = _attributesForElement(element);
        if (rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs.get() range:rangeToReplace];
        _flags.isSoft = NO;
    }
}

void HTMLConverter::_fillInBlock(NSTextBlock *block, DOMElement *element, PlatformColor *backgroundColor, CGFloat extraMargin, CGFloat extraPadding, BOOL isTable)
{
    CGFloat val = 0;
    PlatformColor *color = nil;
    BOOL isTableCellElement = [element isKindOfClass:[DOMHTMLTableCellElement class]];
    NSString *width = isTableCellElement ? [(DOMHTMLTableCellElement *)element width] : [element getAttribute:@"width"];

    if ((width && [width length]) || !isTable) {
        if (_getFloat(&val, element, CSSPropertyWidth))
            [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockWidth];
    }
    
    if (_getFloat(&val, element, CSSPropertyMinWidth))
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumWidth];
    if (_getFloat(&val, element, CSSPropertyMaxWidth))
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumWidth];
    if (_getFloat(&val, element, CSSPropertyMinHeight))
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumHeight];
    if (_getFloat(&val, element, CSSPropertyMaxHeight))
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumHeight];

    if (_getFloat(&val, element, CSSPropertyPaddingLeft))
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    else
        [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    if (_getFloat(&val, element, CSSPropertyPaddingTop))
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    if (_getFloat(&val, element, CSSPropertyPaddingRight))
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    if (_getFloat(&val, element, CSSPropertyPaddingBottom))
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];
    
    if (_getFloat(&val, element, CSSPropertyBorderLeftWidth))
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinXEdge];
    if (_getFloat(&val, element, CSSPropertyBorderTopWidth))
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinYEdge];
    if (_getFloat(&val, element, CSSPropertyBorderRightWidth))
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxXEdge];
    if (_getFloat(&val, element, CSSPropertyBorderBottomWidth))
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxYEdge];

    if (_getFloat(&val, element, CSSPropertyMarginLeft))
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge];
    if (_getFloat(&val, element, CSSPropertyMarginTop))
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge];
    if (_getFloat(&val, element, CSSPropertyMarginRight))
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge];
    if (_getFloat(&val, element, CSSPropertyMarginBottom))
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge];

    if ((color = _colorForNode(element, CSSPropertyBackgroundColor)))
        [block setBackgroundColor:color];
    if (!color && backgroundColor)
        [block setBackgroundColor:backgroundColor];
    if ((color = _colorForNode(element, CSSPropertyBorderLeftColor)))
        [block setBorderColor:color forEdge:NSMinXEdge];
    if ((color = _colorForNode(element, CSSPropertyBorderTopColor)))
        [block setBorderColor:color forEdge:NSMinYEdge];
    if ((color = _colorForNode(element, CSSPropertyBorderRightColor)))
        [block setBorderColor:color forEdge:NSMaxXEdge];
    if ((color = _colorForNode(element, CSSPropertyBorderBottomColor)))
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
    
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090)
    NSString *calendarIdentifier = NSCalendarIdentifierGregorian;
#else
    NSString *calendarIdentifier = NSGregorianCalendar;
#endif

    return [[[[NSCalendar alloc] initWithCalendarIdentifier:calendarIdentifier] autorelease] dateFromComponents:dateComponents.get()];
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
            [_documentAttrs setObject:[NSNumber numberWithDouble:versionNumber] forKey:NSCocoaVersionDocumentAttribute];
        }
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
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

void HTMLConverter::_processHeadElement(DOMElement *element)
{
    // ??? should gather data from other sources e.g. Word, but for that we would need to be able to get comments from DOM
    NSArray *childNodes = _childrenForNode(element);
    NSUInteger count = [childNodes count];
    for (NSUInteger i = 0; i < count; i++) {
        DOMNode *node = [childNodes objectAtIndex:i];
        unsigned short nodeType = [node nodeType];
        if (DOM_ELEMENT_NODE == nodeType) {
            DOMElement *element = (DOMElement *)node;
            NSString *tag = [element tagName];
            if ([@"META" isEqualToString:tag] && [element respondsToSelector:@selector(name)] && [element respondsToSelector:@selector(content)]) {
                NSString *name = [(DOMHTMLMetaElement *)element name];
                NSString *content = [(DOMHTMLMetaElement *)element content];
                if (name && content)
                    _processMetaElementWithName(name, content);
            }
        }
    }
}

BOOL HTMLConverter::_enterElement(DOMElement *element, NSString *tag, NSString *displayVal, BOOL embedded)
{
    if ([@"HEAD" isEqualToString:tag] && !embedded)
        _processHeadElement(element);
    else if (!displayVal || !([@"none" isEqualToString:displayVal] || [@"table-column" isEqualToString:displayVal] || [@"table-column-group" isEqualToString:displayVal])) {
        if (_elementIsBlockLevel(element) && ![@"BR" isEqualToString:tag] && !([@"table-cell" isEqualToString:displayVal] && [_textTables count] == 0)
            && !([_textLists count] > 0 && [@"block" isEqualToString:displayVal] && ![@"LI" isEqualToString:tag] && ![@"UL" isEqualToString:tag] && ![@"OL" isEqualToString:tag]))
            _newParagraphForElement(element, tag, NO, YES);
        return YES;
    }
    return NO;
}

void HTMLConverter::_addTableForElement(DOMElement *tableElement)
{
    RetainPtr<NSTextTable> table = adoptNS([[PlatformNSTextTable alloc] init]);
    CGFloat cellSpacingVal = 1;
    CGFloat cellPaddingVal = 1;
    [table setNumberOfColumns:1];
    [table setLayoutAlgorithm:NSTextTableAutomaticLayoutAlgorithm];
    [table setCollapsesBorders:NO];
    [table setHidesEmptyCells:NO];
    if (tableElement) {
        NSString *borderCollapse = _stringForNode(tableElement, CSSPropertyBorderCollapse);
        NSString *emptyCells = _stringForNode(tableElement, CSSPropertyEmptyCells);
        NSString *tableLayout = _stringForNode(tableElement, CSSPropertyTableLayout);
        if ([tableElement respondsToSelector:@selector(cellSpacing)]) {
            NSString *cellSpacing = [(DOMHTMLTableElement *)tableElement cellSpacing];
            if (cellSpacing && [cellSpacing length] > 0 && ![cellSpacing hasSuffix:@"%"])
                cellSpacingVal = [cellSpacing floatValue];
        }
        if ([tableElement respondsToSelector:@selector(cellPadding)]) {
            NSString *cellPadding = [(DOMHTMLTableElement *)tableElement cellPadding];
            if (cellPadding && [cellPadding length] > 0 && ![cellPadding hasSuffix:@"%"])
                cellPaddingVal = [cellPadding floatValue];
        }
        _fillInBlock(table.get(), tableElement, nil, 0, 0, YES);
        if ([@"collapse" isEqualToString:borderCollapse]) {
            [table setCollapsesBorders:YES];
            cellSpacingVal = 0;
        }
        if ([@"hide" isEqualToString:emptyCells])
            [table setHidesEmptyCells:YES];
        if ([@"fixed" isEqualToString:tableLayout])
            [table setLayoutAlgorithm:NSTextTableFixedLayoutAlgorithm];
    }
    [_textTables addObject:table.get()];
    [_textTableSpacings addObject:[NSNumber numberWithDouble:cellSpacingVal]];
    [_textTablePaddings addObject:[NSNumber numberWithDouble:cellPaddingVal]];
    [_textTableRows addObject:[NSNumber numberWithInteger:0]];
    [_textTableRowArrays addObject:[NSMutableArray array]];
}

void HTMLConverter::_addTableCellForElement(DOMElement *tableCellElement)
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
    if (tableCellElement) {
        if ([tableCellElement respondsToSelector:@selector(rowSpan)]) {
            rowSpan = [(DOMHTMLTableCellElement *)tableCellElement rowSpan];
            if (rowSpan < 1)
                rowSpan = 1;
        }
        if ([tableCellElement respondsToSelector:@selector(colSpan)]) {
            colSpan = [(DOMHTMLTableCellElement *)tableCellElement colSpan];
            if (colSpan < 1)
                colSpan = 1;
        }
    }
    RetainPtr<NSTextTableBlock> block = adoptNS([[PlatformNSTextTableBlock alloc] initWithTable:table startingRow:rowNumber rowSpan:rowSpan startingColumn:columnNumber columnSpan:colSpan]);
    if (tableCellElement) {
        NSString *verticalAlign = _stringForNode(tableCellElement, CSSPropertyVerticalAlign);
        _fillInBlock(block.get(), tableCellElement, color, cellSpacingVal / 2, 0, NO);
        if ([@"middle" isEqualToString:verticalAlign])
            [block setVerticalAlignment:NSTextBlockMiddleAlignment];
        else if ([@"bottom" isEqualToString:verticalAlign])
            [block setVerticalAlignment:NSTextBlockBottomAlignment];
        else if ([@"baseline" isEqualToString:verticalAlign])
            [block setVerticalAlignment:NSTextBlockBaselineAlignment];
        else if ([@"top" isEqualToString:verticalAlign])
            [block setVerticalAlignment:NSTextBlockTopAlignment];
    }
    [_textBlocks addObject:block.get()];
    [rowArray addObject:block.get()];
    [rowArray sortUsingFunction:_colCompare context:NULL];
}

BOOL HTMLConverter::_processElement(DOMElement *element, NSString *tag, NSString *displayVal, NSInteger depth)
{
    BOOL retval = YES;
    BOOL isBlockLevel = _elementIsBlockLevel(element);
    if (isBlockLevel)
        [_writingDirectionArray removeAllObjects];
    else {
        NSString *bidi = _stringForNode(element, CSSPropertyUnicodeBidi);
        if (bidi && [bidi isEqualToString:@"embed"]) {
            NSUInteger val = NSTextWritingDirectionEmbedding;
            NSString *direction = _stringForNode(element, CSSPropertyDirection);
            if ([direction isEqualToString:@"rtl"])
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        } else if (bidi && [bidi isEqualToString:@"bidi-override"]) {
            NSUInteger val = NSTextWritingDirectionOverride;
            NSString *direction = _stringForNode(element, CSSPropertyDirection);
            if ([direction isEqualToString:@"rtl"])
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        }
    }
    if ([@"table" isEqualToString:displayVal] || ([_textTables count] == 0 && [@"table-row-group" isEqualToString:displayVal])) {
        DOMElement *tableElement = element;
        if ([@"table-row-group" isEqualToString:displayVal]) {
            // If we are starting in medias res, the first thing we see may be the tbody, so go up to the table
            tableElement = _blockLevelElementForNode([element parentNode]);
            if (![@"table" isEqualToString:_stringForNode(tableElement, CSSPropertyDisplay)])
                tableElement = element;
        }
        while ([_textTables count] > [_textBlocks count])
            _addTableCellForElement(nil);
        _addTableForElement(tableElement);
    } else if ([@"table-footer-group" isEqualToString:displayVal] && [_textTables count] > 0) {
        [_textTableFooters setObject:element forKey:[NSValue valueWithNonretainedObject:[_textTables lastObject]]];
        retval = NO;
    } else if ([@"table-row" isEqualToString:displayVal] && [_textTables count] > 0) {
        PlatformColor *color = _colorForNode(element, CSSPropertyBackgroundColor);
        if (!color) color = [PlatformColorClass clearColor];
        [_textTableRowBackgroundColors addObject:color];
    } else if ([@"table-cell" isEqualToString:displayVal]) {
        while ([_textTables count] < [_textBlocks count] + 1)
            _addTableForElement(nil);
        _addTableCellForElement(element);
    } else if ([@"IMG" isEqualToString:tag]) {
        NSString *urlString = [element getAttribute:@"src"];
        if (urlString && [urlString length] > 0) {
            NSURL *url = core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url) url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
#if PLATFORM(IOS)
            BOOL usePlaceholderImage = NO;
#else
            BOOL usePlaceholderImage = YES;
#endif
            if (url)
                _addAttachmentForElement(element, url, isBlockLevel, usePlaceholderImage);
        }
        retval = NO;
    } else if ([@"OBJECT" isEqualToString:tag]) {
        NSString *baseString = [element getAttribute:@"codebase"];
        NSString *urlString = [element getAttribute:@"data"];
        NSString *declareString = [element getAttribute:@"declare"];
        if (urlString && [urlString length] > 0 && ![@"true" isEqualToString:declareString]) {
            NSURL *baseURL = nil;
            NSURL *url = nil;
            if (baseString && [baseString length] > 0) {
                baseURL = core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(baseString));
                if (!baseURL)
                    baseURL = [NSURL _web_URLWithString:[baseString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
            }
            if (baseURL)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:baseURL];
            if (!url)
                url = core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
            if (url)
                retval = !_addAttachmentForElement(element, url, isBlockLevel, NO);
        }
    } else if ([@"FRAME" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(contentDocument)]) {
            DOMDocument *contentDocument = [(DOMHTMLFrameElement *)element contentDocument];
            if (contentDocument)
                _traverseNode(contentDocument, depth + 1, YES);
        }
        retval = NO;
    } else if ([@"IFRAME" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(contentDocument)]) {
            DOMDocument *contentDocument = [(DOMHTMLIFrameElement *)element contentDocument];
            if (contentDocument) {
                _traverseNode(contentDocument, depth + 1, YES);
                retval = NO;
            }
        }
    } else if ([@"BR" isEqualToString:tag]) {
        DOMElement *blockElement = _blockLevelElementForNode([element parentNode]);
        NSString *breakClass = [element getAttribute:@"class"], *blockTag = [blockElement tagName];
        BOOL isExtraBreak = [@"Apple-interchange-newline" isEqualToString:breakClass], blockElementIsParagraph = ([@"P" isEqualToString:blockTag] || [@"LI" isEqualToString:blockTag] || ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]));
        if (isExtraBreak)
            _flags.hasTrailingNewline = YES;
        else {
            if (blockElement && blockElementIsParagraph)
                _newLineForElement(element);
            else
                _newParagraphForElement(element, tag, YES, NO);
        }
    } else if ([@"UL" isEqualToString:tag]) {
        RetainPtr<NSTextList> list;
        NSString *listStyleType = _stringForNode(element, CSSPropertyListStyleType);
        if (!listStyleType || [listStyleType length] == 0)
            listStyleType = @"disc";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:[NSString stringWithFormat:@"{%@}", listStyleType] options:0]);
        [_textLists addObject:list.get()];
    } else if ([@"OL" isEqualToString:tag]) {
        RetainPtr<NSTextList> list;
        NSString *listStyleType = _stringForNode(element, CSSPropertyListStyleType);
        if (!listStyleType || [listStyleType length] == 0)
            listStyleType = @"decimal";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:[NSString stringWithFormat:@"{%@}.", listStyleType] options:0]);
        if ([element respondsToSelector:@selector(start)]) {
            NSInteger startingItemNumber = [(DOMHTMLOListElement *)element start];
            [list setStartingItemNumber:startingItemNumber];
        }
        [_textLists addObject:list.get()];
    } else if ([@"Q" isEqualToString:tag]) {
        _addQuoteForElement(element, YES, _quoteLevel++);
    } else if ([@"INPUT" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(type)] && [element respondsToSelector:@selector(value)] && [@"text" compare:[(DOMHTMLInputElement *)element type] options:NSCaseInsensitiveSearch] == NSOrderedSame) {
            NSString *value = [(DOMHTMLInputElement *)element value];
            if (value && [value length] > 0)
                _addValue(value, element);
        }
    } else if ([@"TEXTAREA" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(value)]) {
            NSString *value = [(DOMHTMLTextAreaElement *)element value];
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
    CGFloat pointSize;
    
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
                pointSize = font ? [font pointSize] : 12;
                if ([[paragraphStyle textLists] count] == listIndex + 1) {
                    stringToInsert = [NSString stringWithFormat:@"\t%@\t", [list markerForItemNumber:itemNum++]];
                    insertLength = [stringToInsert length];
                    attrsToInsert = [PlatformNSTextList _standardMarkerAttributesForAttributes:[_attrStr attributesAtIndex:paragraphRange.location effectiveRange:NULL]];

                    [_attrStr replaceCharactersInRange:NSMakeRange(paragraphRange.location, 0) withString:stringToInsert];
                    [_attrStr setAttributes:attrsToInsert range:NSMakeRange(paragraphRange.location, insertLength)];
                    range.length += insertLength;
                    paragraphRange.length += insertLength;
                    if (paragraphRange.location < _domRangeStartIndex) _domRangeStartIndex += insertLength;
                    
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
#if PLATFORM(IOS)
                    tab = [[PlatformNSTextTab alloc] initWithTextAlignment:NSTextAlignmentNatural location:listLocation options:nil];
#else
                    tab = [[PlatformNSTextTab alloc] initWithTextAlignment:NSNaturalTextAlignment location:listLocation options:nil];
#endif
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

void HTMLConverter::_exitElement(DOMElement *element, NSString *tag, NSString *displayVal, NSInteger depth, NSUInteger startIndex)
{
    NSRange range = NSMakeRange(startIndex, [_attrStr length] - startIndex);
    if (range.length > 0 && [@"A" isEqualToString:tag]) {
        NSString *urlString = [element getAttribute:@"href"];
        NSString *strippedString = [urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (urlString && [urlString length] > 0 && strippedString && [strippedString length] > 0 && ![strippedString hasPrefix:@"#"]) {
            NSURL *url = core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(strippedString));
            if (!url)
                url = [NSURL _web_URLWithString:strippedString relativeToURL:_baseURL];
            [_attrStr addAttribute:NSLinkAttributeName value:url ? (id)url : (id)urlString range:range];
        }
    }
    if (!_flags.reachedEnd && _elementIsBlockLevel(element)) {
        [_writingDirectionArray removeAllObjects];
        if ([@"table-cell" isEqualToString:displayVal] && [_textBlocks count] == 0) {
            _newTabForElement(element);
        } else if ([_textLists count] > 0 && [@"block" isEqualToString:displayVal] && ![@"LI" isEqualToString:tag] && ![@"UL" isEqualToString:tag] && ![@"OL" isEqualToString:tag]) {
            _newLineForElement(element);
        } else {
            _newParagraphForElement(element, tag, (range.length == 0), YES);
        }
    } else if ([_writingDirectionArray count] > 0) {
        NSString *bidi = _stringForNode(element, CSSPropertyUnicodeBidi);
        if (bidi && ([bidi isEqualToString:@"embed"] || [bidi isEqualToString:@"bidi-override"])) {
            [_writingDirectionArray removeLastObject];
        }
    }
    range = NSMakeRange(startIndex, [_attrStr length] - startIndex);
    if ([@"table" isEqualToString:displayVal] && [_textTables count] > 0) {
        NSValue *key = [NSValue valueWithNonretainedObject:[_textTables lastObject]];
        DOMNode *footer = [_textTableFooters objectForKey:key];
        while ([_textTables count] < [_textBlocks count] + 1)
            [_textBlocks removeLastObject];
        if (footer) {
            _traverseFooterNode(footer, depth + 1);
            [_textTableFooters removeObjectForKey:key];
        }
        [_textTables removeLastObject];
        [_textTableSpacings removeLastObject];
        [_textTablePaddings removeLastObject];
        [_textTableRows removeLastObject];
        [_textTableRowArrays removeLastObject];
    } else if ([@"table-row" isEqualToString:displayVal] && [_textTables count] > 0) {
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
    } else if ([@"table-cell" isEqualToString:displayVal] && [_textBlocks count] > 0) {
        while ([_textTables count] > [_textBlocks count]) {
            [_textTables removeLastObject];
            [_textTableSpacings removeLastObject];
            [_textTablePaddings removeLastObject];
            [_textTableRows removeLastObject];
            [_textTableRowArrays removeLastObject];
        }
        [_textBlocks removeLastObject];
    } else if (([@"UL" isEqualToString:tag] || [@"OL" isEqualToString:tag]) && [_textLists count] > 0) {
        NSTextList *list = [_textLists lastObject];
        _addMarkersToList(list, range);
        [_textLists removeLastObject];
    } else if ([@"Q" isEqualToString:tag]) {
        _addQuoteForElement(element, NO, --_quoteLevel);
    } else if ([@"SPAN" isEqualToString:tag]) {
        NSString *className = [element getAttribute:@"class"];
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

void HTMLConverter::_processText(DOMCharacterData *text)
{
    NSString *instr = [text data];
    NSString *outstr = instr;
    NSString *whitespaceVal;
    NSString *transformVal;
    NSUInteger textLength = [_attrStr length];
    NSUInteger startOffset = 0;
    NSUInteger endOffset = [instr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    BOOL wasSpace = NO;
    BOOL wasLeading = YES;
    BOOL suppressLeadingSpace = ((_flags.isSoft && lastChar == ' ') || lastChar == '\n' || lastChar == '\r' || lastChar == '\t' || lastChar == NSParagraphSeparatorCharacter || lastChar == NSLineSeparatorCharacter || lastChar == NSFormFeedCharacter || lastChar == WebNextLineCharacter);
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    CFMutableStringRef mutstr = NULL;
    whitespaceVal = _stringForNode(text, CSSPropertyWhiteSpace);
    transformVal = _stringForNode(text, CSSPropertyTextTransform);
    
    if (_domRange) {
        if (text == [_domRange startContainer]) {
            startOffset = (NSUInteger)[_domRange startOffset];
            _domRangeStartIndex = [_attrStr length];
            _flags.reachedStart = YES;
        }
        if (text == [_domRange endContainer]) {
            endOffset = (NSUInteger)[_domRange endOffset];
            _flags.reachedEnd = YES;
        }
        if ((startOffset > 0 || endOffset < [instr length]) && endOffset >= startOffset) {
            instr = [instr substringWithRange:NSMakeRange(startOffset, endOffset - startOffset)];
            outstr = instr;
        }
    }
    if ([whitespaceVal hasPrefix:@"pre"]) {
        if (textLength > 0 && [instr length] > 0 && _flags.isSoft) {
            unichar c = [instr characterAtIndex:0];
            if (c == '\n' || c == '\r' || c == NSParagraphSeparatorCharacter || c == NSLineSeparatorCharacter || c == NSFormFeedCharacter || c == WebNextLineCharacter)
                rangeToReplace = NSMakeRange(textLength - 1, 1);
        }
    } else {
        CFStringInlineBuffer inlineBuffer;
        const unsigned int TextBufferSize = 255;

        unichar buffer[TextBufferSize + 1];
        NSUInteger i, count = [instr length], idx = 0;

        mutstr = CFStringCreateMutable(NULL, 0);
        CFStringInitInlineBuffer((CFStringRef)instr, &inlineBuffer, CFRangeMake(0, count));
        for (i = 0; i < count; i++) {
            unichar c = CFStringGetCharacterFromInlineBuffer(&inlineBuffer, i);
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == 0xc || c == 0x200b) {
                wasSpace = (!wasLeading || !suppressLeadingSpace);
            } else {
                if (wasSpace)
                    buffer[idx++] = ' ';
                buffer[idx++] = c;
                if (idx >= TextBufferSize) {
                    CFStringAppendCharacters(mutstr, buffer, idx);
                    idx = 0;
                }
                wasSpace = wasLeading = NO;
            }
        }
        if (wasSpace)
            buffer[idx++] = ' ';
        if (idx > 0)
            CFStringAppendCharacters(mutstr, buffer, idx);
        outstr = (NSString *)mutstr;
    }
    if ([outstr length] > 0) {
        if ([@"capitalize" isEqualToString:transformVal])
            outstr = [outstr capitalizedString];
        else if ([@"uppercase" isEqualToString:transformVal])
            outstr = [outstr uppercaseString];
        else if ([@"lowercase" isEqualToString:transformVal])
            outstr = [outstr lowercaseString];
        [_attrStr replaceCharactersInRange:rangeToReplace withString:outstr];
        rangeToReplace.length = [outstr length];
        RetainPtr<NSDictionary> attrs;
        DOMElement *element = (DOMElement *)text;
        while (element) {
            // Fill attrs dictionary with attributes from parent nodes, not overwriting ones deeper in the tree
            if([element nodeType] == DOM_ELEMENT_NODE) {
                RetainPtr<NSMutableDictionary> newAttrs = adoptNS([_attributesForElement(element) mutableCopy]);
                if (attrs) {
                    // Already-set attributes (from lower in the tree) overwrite the higher ones.
                    [newAttrs addEntriesFromDictionary:attrs.get()];
                }
                attrs = newAttrs;
            }
            element = (DOMElement *)[element parentNode];
        }
        if (rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs.get() range:rangeToReplace];
        _flags.isSoft = wasSpace;
    }
    if (mutstr)
        CFRelease(mutstr);
}

void HTMLConverter::_traverseNode(DOMNode *node, NSInteger depth, BOOL embedded)
{
    unsigned short nodeType;
    NSArray *childNodes;
    NSUInteger count;
    NSUInteger startOffset;
    NSUInteger endOffset;
    BOOL isStart = NO;
    BOOL isEnd = NO;

    if (_flags.reachedEnd)
        return;
    if (_domRange && !_flags.reachedStart && _domStartAncestors && ![_domStartAncestors containsObject:node])
        return;
    
    nodeType = [node nodeType];
    childNodes = _childrenForNode(node);
    count = [childNodes count];
    startOffset = 0;
    endOffset = count;

    if (_domRange) {
        if (node == [_domRange startContainer]) {
            startOffset = (NSUInteger)[_domRange startOffset];
            isStart = YES;
            _flags.reachedStart = YES;
        }
        if (node == [_domRange endContainer]) {
            endOffset = (NSUInteger)[_domRange endOffset];
            isEnd = YES;
        }
    }
    
    if (nodeType == DOM_DOCUMENT_NODE || nodeType == DOM_DOCUMENT_FRAGMENT_NODE) {
        for (NSUInteger i = 0; i < count; i++) {
            if (isStart && i == startOffset)
                _domRangeStartIndex = [_attrStr length];
            if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                _traverseNode([childNodes objectAtIndex:i], depth + 1, embedded);
            if (isEnd && i + 1 >= endOffset)
                _flags.reachedEnd = YES;
            if (_flags.reachedEnd) break;
        }
    } else if (nodeType == DOM_ELEMENT_NODE) {
        DOMElement *element = (DOMElement *)node;
        NSString *tag = [element tagName];
        NSString *displayVal = _stringForNode(element, CSSPropertyDisplay);
        if (_enterElement(element, tag, displayVal, embedded)) {
            NSUInteger startIndex = [_attrStr length];
            if (_processElement(element, tag, displayVal, depth)) {
                for (NSUInteger i = 0; i < count; i++) {
                    if (isStart && i == startOffset)
                        _domRangeStartIndex = [_attrStr length];
                    if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                        _traverseNode([childNodes objectAtIndex:i], depth + 1, embedded);
                    if (isEnd && i + 1 >= endOffset)
                        _flags.reachedEnd = YES;
                    if (_flags.reachedEnd)
                        break;
                }
                _exitElement(element, tag, displayVal, depth, startIndex);
            }
        }
    } else if (nodeType == DOM_TEXT_NODE || nodeType == DOM_CDATA_SECTION_NODE) {
        _processText((DOMCharacterData *)node);
    }
    
    if (isEnd)
        _flags.reachedEnd = YES;
}

void HTMLConverter::_traverseFooterNode(DOMNode *node, NSInteger depth)
{
    DOMElement *element = (DOMElement *)node;
    NSArray *childNodes = _childrenForNode(node);
    NSString *tag = @"TBODY", *displayVal = @"table-row-group";
    NSUInteger count = [childNodes count];
    NSUInteger startOffset = 0;
    NSUInteger endOffset = count;
    BOOL isStart = NO;
    BOOL isEnd = NO;

    if (_flags.reachedEnd)
        return;
    if (_domRange && !_flags.reachedStart && _domStartAncestors && ![_domStartAncestors containsObject:node])
        return;
    if (_domRange) {
        if (node == [_domRange startContainer]) {
            startOffset = (NSUInteger)[_domRange startOffset];
            isStart = YES;
            _flags.reachedStart = YES;
        }
        if (node == [_domRange endContainer]) {
            endOffset = (NSUInteger)[_domRange endOffset];
            isEnd = YES;
        }
    }
    if (_enterElement(element, tag, displayVal, YES)) {
        NSUInteger startIndex = [_attrStr length];
        if (_processElement(element, tag, displayVal, depth)) {
            for (NSUInteger i = 0; i < count; i++) {
                if (isStart && i == startOffset)
                    _domRangeStartIndex = [_attrStr length];
                if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                    _traverseNode([childNodes objectAtIndex:i], depth + 1, YES);
                if (isEnd && i + 1 >= endOffset)
                    _flags.reachedEnd = YES;
                if (_flags.reachedEnd)
                    break;
            }
            _exitElement(element, tag, displayVal, depth, startIndex);
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

void HTMLConverter::_loadFromDOMRange()
{
    if (-1 == _errorCode) {
        DOMNode *commonAncestorContainer = [_domRange commonAncestorContainer];
        DOMNode *ancestorContainer = [_domRange startContainer];
        
        _domStartAncestors = [[NSMutableArray alloc] init];
        while (ancestorContainer) {
            [_domStartAncestors addObject:ancestorContainer];
            if (ancestorContainer == commonAncestorContainer)
                break;
            ancestorContainer = [ancestorContainer parentNode];
        }
        
        _document = [commonAncestorContainer ownerDocument];
        _dataSource = (DocumentLoader *)core(_document)->frame()->loader().documentLoader();
        
        if (_document && _dataSource) {
            _domRangeStartIndex = 0;
            _errorCode = 0;
            _traverseNode(commonAncestorContainer, 0, NO);
            if (_domRangeStartIndex > 0 && _domRangeStartIndex <= [_attrStr length])
                [_attrStr deleteCharactersInRange:NSMakeRange(0, _domRangeStartIndex)];
        }
    }
}

#if !PLATFORM(IOS)

static NSFileWrapper *fileWrapperForURL(DocumentLoader *dataSource, NSURL *URL)
{
    if ([URL isFileURL])
        return [[[NSFileWrapper alloc] initWithURL:[URL URLByResolvingSymlinksInPath] options:0 error:nullptr] autorelease];

    RefPtr<ArchiveResource> resource = dataSource->subresource(URL);
    if (resource) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:resource->data()->createNSData().get()] autorelease];
        NSString *filename = resource->response().suggestedFilename();
        if (!filename || ![filename length])
            filename = suggestedFilenameWithMIMEType(resource->url(), resource->mimeType());
        [wrapper setPreferredFilename:filename];
        return wrapper;
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

static NSFileWrapper *fileWrapperForElement(Element* element)
{
    NSFileWrapper *wrapper = nil;
    
    const AtomicString& attr = element->getAttribute(srcAttr);
    if (!attr.isEmpty()) {
        NSURL *URL = element->document().completeURL(attr);
        if (DocumentLoader* loader = element->document().loader())
            wrapper = fileWrapperForURL(loader, URL);
    }
    if (!wrapper) {
        RenderImage* renderer = toRenderImage(element->renderer());
        if (renderer->cachedImage() && !renderer->cachedImage()->errorOccurred()) {
            wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:(NSData *)(renderer->cachedImage()->imageForRenderer(renderer)->getTIFFRepresentation())];
            [wrapper setPreferredFilename:@"image.tiff"];
            [wrapper autorelease];
        }
    }

    return wrapper;
}

#endif

namespace WebCore {
    
// This function supports more HTML features than the editing variant below, such as tables.
NSAttributedString *attributedStringFromRange(Range& range)
{
    HTMLConverter converter(kit(&range));
    return converter.convert();
}
    
#if !PLATFORM(IOS)
// This function uses TextIterator, which makes offsets in its result compatible with HTML editing.
NSAttributedString *editingAttributedStringFromRange(Range& range)
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSMutableAttributedString *string = [[NSMutableAttributedString alloc] init];
    NSUInteger stringLength = 0;
    RetainPtr<NSMutableDictionary> attrs = adoptNS([[NSMutableDictionary alloc] init]);

    for (TextIterator it(&range); !it.atEnd(); it.advance()) {
        RefPtr<Range> currentTextRange = it.range();
        Node* startContainer = currentTextRange->startContainer();
        Node* endContainer = currentTextRange->endContainer();
        int startOffset = currentTextRange->startOffset();
        int endOffset = currentTextRange->endOffset();
        
        if (startContainer == endContainer && (startOffset == endOffset - 1)) {
            Node* node = startContainer->childNode(startOffset);
            if (node && node->hasTagName(imgTag)) {
                NSFileWrapper* fileWrapper = fileWrapperForElement(toElement(node));
                NSTextAttachment* attachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
                [string appendAttributedString:[NSAttributedString attributedStringWithAttachment:attachment]];
                [attachment release];
            }
        }

        int currentTextLength = it.text().length();
        if (!currentTextLength)
            continue;

        RenderObject* renderer = startContainer->renderer();
        ASSERT(renderer);
        if (!renderer)
            continue;
        const RenderStyle& style = renderer->style();
        if (style.textDecorationsInEffect() & TextDecorationUnderline)
            [attrs.get() setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
        if (style.textDecorationsInEffect() & TextDecorationLineThrough)
            [attrs.get() setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
        if (NSFont *font = style.font().primaryFont()->getNSFont())
            [attrs.get() setObject:font forKey:NSFontAttributeName];
        else
            [attrs.get() setObject:[fontManager convertFont:WebDefaultFont() toSize:style.font().primaryFont()->platformData().size()] forKey:NSFontAttributeName];
        if (style.visitedDependentColor(CSSPropertyColor).alpha())
            [attrs.get() setObject:nsColor(style.visitedDependentColor(CSSPropertyColor)) forKey:NSForegroundColorAttributeName];
        else
            [attrs.get() removeObjectForKey:NSForegroundColorAttributeName];
        if (style.visitedDependentColor(CSSPropertyBackgroundColor).alpha())
            [attrs.get() setObject:nsColor(style.visitedDependentColor(CSSPropertyBackgroundColor)) forKey:NSBackgroundColorAttributeName];
        else
            [attrs.get() removeObjectForKey:NSBackgroundColorAttributeName];

        [string replaceCharactersInRange:NSMakeRange(stringLength, 0) withString:it.text().createNSStringWithoutCopying().get()];
        [string setAttributes:attrs.get() range:NSMakeRange(stringLength, currentTextLength)];
        stringLength += currentTextLength;
    }

    return [string autorelease];
}
#endif
    
}
