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
#import "Document.h"
#import "DocumentLoader.h"
#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMHTMLTableCellElement.h"
#import "DOMPrivate.h"
#import "DOMRangeInternal.h"
#import "Element.h"
#import "Font.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "HTMLNames.h"
#import "HTMLParserIdioms.h"
#import "LoaderNSURLExtras.h"
#import "RenderImage.h"
#import "SoftLinking.h"
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
SOFT_LINK_CONSTANT(UIFoundation, NSSuperscriptAttributeName, NSString *)
#define NSSuperscriptAttributeName getNSSuperscriptAttributeName()
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

@interface WebHTMLConverter(WebHTMLConverterInternal)

- (NSString *)_stringForNode:(DOMNode *)node property:(NSString *)key;
- (PlatformColor *)_colorForNode:(DOMNode *)node property:(NSString *)key;
- (BOOL)_getFloat:(CGFloat *)val forNode:(DOMNode *)node property:(NSString *)key;
- (void)_traverseNode:(DOMNode *)node depth:(NSInteger)depth embedded:(BOOL)embedded;
- (void)_traverseFooterNode:(DOMNode *)node depth:(NSInteger)depth;

@end

@implementation WebHTMLConverter

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

+ (NSParagraphStyle *)defaultParagraphStyle
{
    static NSMutableParagraphStyle *defaultParagraphStyle = nil;
    if (!defaultParagraphStyle) {
        defaultParagraphStyle = [[PlatformNSParagraphStyle defaultParagraphStyle] mutableCopy];
        [defaultParagraphStyle setDefaultTabInterval:36];
        [defaultParagraphStyle setTabStops:[NSArray array]];
    }
    return defaultParagraphStyle;
}

- (NSArray *)_childrenForNode:(DOMNode *)node
{
    NSMutableArray *array = [NSMutableArray array];
    DOMNode *child = [node firstChild];
    while (child) {
        [array addObject:child];
        child = [child nextSibling];
    }
    return array;
}

- (DOMCSSStyleDeclaration *)_computedStyleForElement:(DOMElement *)element
{
    DOMDocument *document = [element ownerDocument];
    DOMCSSStyleDeclaration *result = nil;
    result = [_computedStylesForElements objectForKey:element];
    if (result) {
        if ([result isEqual:[NSNull null]])
            result = nil;
    } else {
        result = [document getComputedStyle:element pseudoElement:@""] ;
        [_computedStylesForElements setObject:(result ? (id)result : (id)[NSNull null]) forKey:element];
    }
    return result;
}

- (DOMCSSStyleDeclaration *)_specifiedStyleForElement:(DOMElement *)element
{
    DOMCSSStyleDeclaration *result = [_specifiedStylesForElements objectForKey:element];
    if (result) {
        if ([result isEqual:[NSNull null]])
            result = nil;
    } else {
        result = [element style];
        [_specifiedStylesForElements setObject:(result ? (id)result : (id)[NSNull null]) forKey:element];
    }
    return result;
}

- (NSString *)_computedStringForNode:(DOMNode *)node property:(NSString *)key
{
    NSString *result = nil;
    BOOL inherit = YES;
    DOMElement *element = (DOMElement *)node;    
    if (element && [element nodeType] == DOM_ELEMENT_NODE) {
        DOMCSSStyleDeclaration *computedStyle;
        DOMCSSStyleDeclaration *specifiedStyle;
        inherit = NO;
        if (!result && (computedStyle = [self _computedStyleForElement:element])) {
            DOMCSSPrimitiveValue *computedValue = (DOMCSSPrimitiveValue *)[computedStyle getPropertyCSSValue:key];
            if (computedValue) {
                unsigned short valueType = [computedValue cssValueType];
                if (valueType == DOM_CSS_PRIMITIVE_VALUE) {
                    unsigned short primitiveType = [computedValue primitiveType];
                    if (primitiveType == DOM_CSS_STRING || primitiveType == DOM_CSS_URI || primitiveType == DOM_CSS_IDENT || primitiveType == DOM_CSS_ATTR) {
                        result = [computedValue getStringValue];
                        if (result && ![result length])
                            result = nil;
                    }
                } else if (valueType == DOM_CSS_VALUE_LIST)
                    result = [computedStyle getPropertyValue:key];
            }
        }
        if (!result && (specifiedStyle = [self _specifiedStyleForElement:element])) {
            DOMCSSPrimitiveValue *specifiedValue = (DOMCSSPrimitiveValue *)[specifiedStyle getPropertyCSSValue:key];
            if (specifiedValue) {
                unsigned short valueType = [specifiedValue cssValueType];
                if (valueType == DOM_CSS_PRIMITIVE_VALUE) {
                    unsigned short primitiveType = [specifiedValue primitiveType];
                    if (primitiveType == DOM_CSS_STRING || primitiveType == DOM_CSS_URI || primitiveType == DOM_CSS_IDENT || primitiveType == DOM_CSS_ATTR) {
                        result = [specifiedValue getStringValue];
                        if (result && ![result length])
                            result = nil;
                        if (!result)
                            result = [specifiedStyle getPropertyValue:key];
                    }
                } else if (valueType == DOM_CSS_INHERIT)
                    inherit = YES;
                else if (valueType == DOM_CSS_VALUE_LIST)
                    result = [specifiedStyle getPropertyValue:key];
            }
        }
        if (!result) {
            Element* coreElement = core(element);
            if ([@"display" isEqualToString:key]) {
                if (coreElement->hasTagName(headTag) || coreElement->hasTagName(scriptTag) || coreElement->hasTagName(appletTag) || coreElement->hasTagName(noframesTag))
                    result = @"none";
                else if (coreElement->hasTagName(addressTag) || coreElement->hasTagName(blockquoteTag) || coreElement->hasTagName(bodyTag) || coreElement->hasTagName(centerTag)
                         || coreElement->hasTagName(ddTag) || coreElement->hasTagName(dirTag) || coreElement->hasTagName(divTag) || coreElement->hasTagName(dlTag)
                         || coreElement->hasTagName(dtTag) || coreElement->hasTagName(fieldsetTag) || coreElement->hasTagName(formTag) || coreElement->hasTagName(frameTag)
                         || coreElement->hasTagName(framesetTag) || coreElement->hasTagName(hrTag) || coreElement->hasTagName(htmlTag) || coreElement->hasTagName(h1Tag)
                         || coreElement->hasTagName(h2Tag) || coreElement->hasTagName(h3Tag) || coreElement->hasTagName(h4Tag) || coreElement->hasTagName(h5Tag)
                         || coreElement->hasTagName(h6Tag) || coreElement->hasTagName(iframeTag) || coreElement->hasTagName(menuTag) || coreElement->hasTagName(noscriptTag)
                         || coreElement->hasTagName(olTag) || coreElement->hasTagName(pTag) || coreElement->hasTagName(preTag) || coreElement->hasTagName(ulTag))
                    result = @"block";
                else if (coreElement->hasTagName(liTag))
                    result = @"list-item";
                else if (coreElement->hasTagName(tableTag))
                    result = @"table";
                else if (coreElement->hasTagName(trTag))
                    result = @"table-row";
                else if (coreElement->hasTagName(thTag) || coreElement->hasTagName(tdTag))
                    result = @"table-cell";
                else if (coreElement->hasTagName(theadTag))
                    result = @"table-header-group";
                else if (coreElement->hasTagName(tbodyTag))
                    result = @"table-row-group";
                else if (coreElement->hasTagName(tfootTag))
                    result = @"table-footer-group";
                else if (coreElement->hasTagName(colTag))
                    result = @"table-column";
                else if (coreElement->hasTagName(colgroupTag))
                    result = @"table-column-group";
                else if (coreElement->hasTagName(captionTag))
                    result = @"table-caption";
            } else if ([@"white-space" isEqualToString:key]) {
                if (coreElement->hasTagName(preTag))
                    result = @"pre";
                else
                    inherit = YES;
            } else if ([@"font-style" isEqualToString:key]) {
                if (coreElement->hasTagName(iTag) || coreElement->hasTagName(citeTag) || coreElement->hasTagName(emTag) || coreElement->hasTagName(varTag) || coreElement->hasTagName(addressTag))
                    result = @"italic";
                else
                    inherit = YES;
            } else if ([@"font-weight" isEqualToString:key]) {
                if (coreElement->hasTagName(bTag) || coreElement->hasTagName(strongTag) || coreElement->hasTagName(thTag))
                    result = @"bolder";
                else
                    inherit = YES;
            } else if ([@"text-decoration" isEqualToString:key]) {
                if (coreElement->hasTagName(uTag) || coreElement->hasTagName(insTag))
                    result = @"underline";
                else if (coreElement->hasTagName(sTag) || coreElement->hasTagName(strikeTag) || coreElement->hasTagName(delTag))
                    result = @"line-through";
                else
                    inherit = YES; // ??? this is not strictly correct
            } else if ([@"text-align" isEqualToString:key]) {
                if (coreElement->hasTagName(centerTag) || coreElement->hasTagName(captionTag) || coreElement->hasTagName(thTag))
                    result = @"center";
                else
                    inherit = YES;
            } else if ([@"vertical-align" isEqualToString:key]) {
                if (coreElement->hasTagName(supTag))
                    result = @"super";
                else if (coreElement->hasTagName(subTag))
                    result = @"sub";
                else if (coreElement->hasTagName(theadTag) || coreElement->hasTagName(tbodyTag) || coreElement->hasTagName(tfootTag))
                    result = @"middle";
                else if (coreElement->hasTagName(trTag) || coreElement->hasTagName(thTag) || coreElement->hasTagName(tdTag))
                    inherit = YES;
            } else if ([@"font-family" isEqualToString:key] || [@"font-variant" isEqualToString:key] || [@"font-effect" isEqualToString:key]
                       || [@"text-transform" isEqualToString:key] || [@"text-shadow" isEqualToString:key] || [@"visibility" isEqualToString:key]
                       || [@"border-collapse" isEqualToString:key] || [@"empty-cells" isEqualToString:key] || [@"word-spacing" isEqualToString:key]
                       || [@"list-style-type" isEqualToString:key] || [@"direction" isEqualToString:key]) {
                inherit = YES;
            }
        }
    }
    if (!result && inherit) {
        DOMNode *parentNode = [node parentNode];
        if (parentNode)
            result = [self _stringForNode:parentNode property:key];
    }
    return result ? [result lowercaseString] : nil;
}

- (NSString *)_stringForNode:(DOMNode *)node property:(NSString *)key
{
    NSString *result = nil;
    RetainPtr<NSMutableDictionary> attributeDictionary = [_stringsForNodes objectForKey:node];
    if (!attributeDictionary) {
        attributeDictionary = adoptNS([[NSMutableDictionary alloc] init]);
        [_stringsForNodes setObject:attributeDictionary.get() forKey:node];
    }
    result = [attributeDictionary objectForKey:key];
    if (result) {
        if ([result isEqualToString:@""])
            result = nil;
    } else {
        result = [self _computedStringForNode:node property:key];
        [attributeDictionary setObject:(result ? result : @"") forKey:key];
    }
    return result;
}

static inline BOOL _getFloat(DOMCSSPrimitiveValue *primitiveValue, CGFloat *val)
{
    if (!val)
        return NO;
    switch ([primitiveValue primitiveType]) {
        case DOM_CSS_PX:
            *val = [primitiveValue getFloatValue:DOM_CSS_PX];
            return YES;
        case DOM_CSS_PT:
            *val = 4 * [primitiveValue getFloatValue:DOM_CSS_PT] / 3;
            return YES;
        case DOM_CSS_PC:
            *val = 16 * [primitiveValue getFloatValue:DOM_CSS_PC];
            return YES;
        case DOM_CSS_CM:
            *val = 96 * [primitiveValue getFloatValue:DOM_CSS_CM] / (CGFloat)2.54;
            return YES;
        case DOM_CSS_MM:
            *val = 96 * [primitiveValue getFloatValue:DOM_CSS_MM] / (CGFloat)25.4;
            return YES;
        case DOM_CSS_IN:
            *val = 96 * [primitiveValue getFloatValue:DOM_CSS_IN];
            return YES;
        default:
            return NO;
    }
}

- (BOOL)_getComputedFloat:(CGFloat *)val forNode:(DOMNode *)node property:(NSString *)key
{
    BOOL result = NO;
    BOOL inherit = YES;
    CGFloat floatVal = 0;
    DOMElement *element = (DOMElement *)node;    
    if (element && [element nodeType] == DOM_ELEMENT_NODE) {
        DOMCSSStyleDeclaration *computedStyle, *specifiedStyle;
        inherit = NO;
        if (!result && (computedStyle = [self _computedStyleForElement:element])) {
            DOMCSSPrimitiveValue *computedValue = (DOMCSSPrimitiveValue *)[computedStyle getPropertyCSSValue:key];
            if (computedValue && [computedValue cssValueType] == DOM_CSS_PRIMITIVE_VALUE)
                result = _getFloat(computedValue, &floatVal);
        }
        if (!result && (specifiedStyle = [self _specifiedStyleForElement:element])) {
            DOMCSSPrimitiveValue *specifiedValue = (DOMCSSPrimitiveValue *)[specifiedStyle getPropertyCSSValue:key];
            if (specifiedValue) {
                unsigned short valueType = [specifiedValue cssValueType];
                if (valueType == DOM_CSS_PRIMITIVE_VALUE)
                    result = _getFloat(specifiedValue, &floatVal);
                else if (valueType == DOM_CSS_INHERIT)
                    inherit = YES;
            }
        }
        if (!result) {
            if ([@"text-indent" isEqualToString:key] || [@"letter-spacing" isEqualToString:key] || [@"word-spacing" isEqualToString:key]
                || [@"line-height" isEqualToString:key] || [@"widows" isEqualToString:key] || [@"orphans" isEqualToString:key])
                inherit = YES;
        }
    }
    if (!result && inherit) {
        DOMNode *parentNode = [node parentNode];
        if (parentNode)
            result = [self _getFloat:&floatVal forNode:parentNode property:key];
    }
    if (result && val)
        *val = floatVal;
    return result;
}

- (BOOL)_getFloat:(CGFloat *)val forNode:(DOMNode *)node property:(NSString *)key
{
    BOOL result = NO;
    CGFloat floatVal = 0;
    NSNumber *floatNumber;
    RetainPtr<NSMutableDictionary> attributeDictionary = [_floatsForNodes objectForKey:node];
    if (!attributeDictionary) {
        attributeDictionary = adoptNS([[NSMutableDictionary alloc] init]);
        [_floatsForNodes setObject:attributeDictionary.get() forKey:node];
    }
    floatNumber = [attributeDictionary objectForKey:key];
    if (floatNumber) {
        if (![[NSNull null] isEqual:floatNumber]) {
            result = YES;
            floatVal = [floatNumber floatValue];
        }
    } else {
        result = [self _getComputedFloat:&floatVal forNode:node property:key];
        [attributeDictionary setObject:(result ? (id)[NSNumber numberWithDouble:floatVal] : (id)[NSNull null]) forKey:key];
    }
    if (result && val)
        *val = floatVal;
    return result;
}

static NSString *_NSFirstPathForDirectoriesInDomains(NSSearchPathDirectory directory, NSSearchPathDomainMask domainMask, BOOL expandTilde)
{
    NSArray *array = NSSearchPathForDirectoriesInDomains(directory, domainMask, expandTilde);
    return [array count] >= 1 ? [array objectAtIndex:0] : nil;
}

static NSString *_NSSystemLibraryPath(void)
{
    return _NSFirstPathForDirectoriesInDomains(NSLibraryDirectory, NSSystemDomainMask, YES);
}

- (NSBundle *)_webKitBundle
{
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    if (!bundle)
        bundle = [NSBundle bundleWithPath:[_NSSystemLibraryPath() stringByAppendingPathComponent:@"Frameworks/WebKit.framework"]];
    return bundle;
}

#if PLATFORM(IOS)
static inline UIColor *_colorForRGBColor(DOMRGBColor *domRGBColor, BOOL)
{
    return [getUIColorClass() _disambiguated_due_to_CIImage_colorWithCGColor:[domRGBColor color]];
}

#else
static inline NSColor *_colorForRGBColor(DOMRGBColor *domRGBColor, BOOL ignoreBlack)
{
    NSColor *color = [domRGBColor _color];
    NSColorSpace *colorSpace = [color colorSpace];
    const CGFloat ColorEpsilon = 1 / (2 * (CGFloat)255.0);
    
    if (color) {
        if ([colorSpace isEqual:[NSColorSpace genericGrayColorSpace]] || [colorSpace isEqual:[NSColorSpace deviceGrayColorSpace]]) {
            CGFloat white, alpha;
            [color getWhite:&white alpha:&alpha];
            if (white < ColorEpsilon && (ignoreBlack || alpha < ColorEpsilon))
                color = nil;
        } else {
            NSColor *rgbColor = nil;
            if ([colorSpace isEqual:[NSColorSpace genericRGBColorSpace]] || [colorSpace isEqual:[NSColorSpace deviceRGBColorSpace]])
                rgbColor = color;
            if (!rgbColor)
                rgbColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
            if (rgbColor) {
                CGFloat red, green, blue, alpha;
                [rgbColor getRed:&red green:&green blue:&blue alpha:&alpha];
                if (red < ColorEpsilon && green < ColorEpsilon && blue < ColorEpsilon && (ignoreBlack || alpha < ColorEpsilon))
                    color = nil;
            }
        }
    }
    return color;
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

- (BOOL)_elementIsBlockLevel:(DOMElement *)element
{
    BOOL isBlockLevel = NO;
    NSNumber *val = nil;
    val = [_elementIsBlockLevel objectForKey:element];
    if (val)
        isBlockLevel = [val boolValue];
    else {
        NSString *displayVal = [self _stringForNode:element property:@"display"];
        NSString *floatVal = [self _stringForNode:element property:@"float"];
        if (floatVal && ([@"left" isEqualToString:floatVal] || [@"right" isEqualToString:floatVal]))
            isBlockLevel = YES;
        else if (displayVal)
            isBlockLevel = ([@"block" isEqualToString:displayVal] || [@"list-item" isEqualToString:displayVal] || [displayVal hasPrefix:@"table"]);

        [_elementIsBlockLevel setObject:[NSNumber numberWithBool:isBlockLevel] forKey:element];
    }
    return isBlockLevel;
}

- (BOOL)_elementHasOwnBackgroundColor:(DOMElement *)element
{
    // In the text system, text blocks (table elements) and documents (body elements)
    // have their own background colors, which should not be inherited.
    if ([self _elementIsBlockLevel:element]) {
        Element* coreElement = core(element);
        NSString *displayVal = [self _stringForNode:element property:@"display"];
        if (coreElement->hasTagName(htmlTag) || coreElement->hasTagName(bodyTag) || [displayVal hasPrefix:@"table"])
            return YES;
    }
    return NO;
}
    
- (DOMElement *)_blockLevelElementForNode:(DOMNode *)node
{
    DOMElement *element = (DOMElement *)node;
    while (element && [element nodeType] != DOM_ELEMENT_NODE)
        element = (DOMElement *)[element parentNode];
    if (element && ![self _elementIsBlockLevel:element])
        element = [self _blockLevelElementForNode:[element parentNode]];
    return element;
}

- (PlatformColor *)_computedColorForNode:(DOMNode *)node property:(NSString *)key
{
    PlatformColor *result = nil;
    BOOL inherit = YES;
    BOOL haveResult = NO;
    BOOL isColor = [@"color" isEqualToString:key];
    BOOL isBackgroundColor = [@"background-color" isEqualToString:key];
    DOMElement *element = (DOMElement *)node;    
    if (element && [element nodeType] == DOM_ELEMENT_NODE) {
        DOMCSSStyleDeclaration *computedStyle, *specifiedStyle;
        inherit = NO;
        if (!haveResult && (computedStyle = [self _computedStyleForElement:element])) {
            DOMCSSPrimitiveValue *computedValue = (DOMCSSPrimitiveValue *)[computedStyle getPropertyCSSValue:key];
            if (computedValue && [computedValue cssValueType] == DOM_CSS_PRIMITIVE_VALUE && [computedValue primitiveType] == DOM_CSS_RGBCOLOR) {
                result = _colorForRGBColor([computedValue getRGBColorValue], isColor);
                haveResult = YES;
            }
        }
        if (!haveResult && (specifiedStyle = [self _specifiedStyleForElement:element])) {
            DOMCSSPrimitiveValue *specifiedValue = (DOMCSSPrimitiveValue *)[specifiedStyle getPropertyCSSValue:key];
            if (specifiedValue) {
                unsigned short valueType = [specifiedValue cssValueType];
                if (valueType == DOM_CSS_PRIMITIVE_VALUE && [specifiedValue primitiveType] == DOM_CSS_RGBCOLOR) {
                    result = _colorForRGBColor([specifiedValue getRGBColorValue], isColor);
                    haveResult = YES;
                } else if (valueType == DOM_CSS_INHERIT)
                    inherit = YES;
            }
        }
        if (!result) {
            if ((isColor && !haveResult) || (isBackgroundColor && ![self _elementHasOwnBackgroundColor:element]))
                inherit = YES;
        }
    }
    if (!result && inherit) {
        DOMNode *parentNode = [node parentNode];
        if (parentNode && !(isBackgroundColor && [parentNode nodeType] == DOM_ELEMENT_NODE && [self _elementHasOwnBackgroundColor:(DOMElement *)parentNode]))
            result = [self _colorForNode:parentNode property:key];
    }
    return result;
}

- (PlatformColor *)_colorForNode:(DOMNode *)node property:(NSString *)key {
    RetainPtr<NSMutableDictionary> attributeDictionary = [_colorsForNodes objectForKey:node];
    if (!attributeDictionary) {
        attributeDictionary = adoptNS([[NSMutableDictionary alloc] init]);
        [_colorsForNodes setObject:attributeDictionary.get() forKey:node];
    }
    PlatformColor *result = [attributeDictionary objectForKey:key];
    if (!result) {
        result = [self _computedColorForNode:node property:key];
        [attributeDictionary setObject:(result ? result : [PlatformColorClass clearColor]) forKey:key];
    }
    if ([[PlatformColorClass clearColor] isEqual:result] || ([result alphaComponent] == 0.0) )
        result = nil;
    return result;
}

#define UIFloatIsZero(number) (fabs(number - 0) < FLT_EPSILON)

- (NSDictionary *)_computedAttributesForElement:(DOMElement *)element
{
    DOMElement *blockElement = [self _blockLevelElementForNode:element];
    NSMutableDictionary *attrs = [NSMutableDictionary dictionary];
#if !PLATFORM(IOS)
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
#endif
    NSString *fontEffect = [self _stringForNode:element property:@"font-effect"];
    NSString *textDecoration = [self _stringForNode:element property:@"text-decoration"];
    NSString *verticalAlign = [self _stringForNode:element property:@"vertical-align"];
    NSString *textShadow = [self _stringForNode:element property:@"text-shadow"];
    NSString *fontLigatures = [self _stringForNode:element property:@"font-variant-ligatures"];
    NSString *fontKerning = [self _stringForNode:element property:@"font-kerning"];
    NSString *letterSpacing = [self _stringForNode:element property:@"letter-spacing"];
    CGFloat fontSize = 0;
    CGFloat baselineOffset = 0;
    CGFloat strokeWidth = 0.0;
    PlatformFont *font = nil;
    PlatformFont *actualFont = (PlatformFont *)[element _font];
    PlatformColor *foregroundColor = [self _colorForNode:element property:@"color"];
    PlatformColor *backgroundColor = [self _colorForNode:element property:@"background-color"];
    PlatformColor *strokeColor = [self _colorForNode:element property:@"-webkit-text-stroke-color"];

    if (![self _getFloat:&fontSize forNode:element property:@"font-size"] || fontSize <= 0.0)
        fontSize = _defaultFontSize;
    fontSize *= _textSizeMultiplier;
    if (fontSize < _minimumFontSize) fontSize = _minimumFontSize;
    if (fabs(floor(2.0 * fontSize + 0.5) / 2.0 - fontSize) < 0.05)
        fontSize = (CGFloat)floor(2.0 * fontSize + 0.5) / 2;
    else if (fabs(floor(10.0 * fontSize + 0.5) / 10.0 - fontSize) < 0.005)
        fontSize = (CGFloat)floor(10.0 * fontSize + 0.5) / 10;

    if (fontSize <= 0.0)
        fontSize = 12;
    
#if PLATFORM(IOS)
    if (actualFont)
        font = [actualFont fontWithSize:fontSize];
#else
    if (actualFont)
        font = [fontManager convertFont:actualFont toSize:fontSize];
#endif
    if (!font) {
        NSString *fontName = [[self _stringForNode:element property:@"font-family"] capitalizedString];
        NSString *fontStyle = [self _stringForNode:element property:@"font-style"];
        NSString *fontWeight = [self _stringForNode:element property:@"font-weight"];
#if !PLATFORM(IOS)
        NSString *fontVariant = [self _stringForNode:element property:@"font-variant"];
#endif
        if (!fontName)
            fontName = _standardFontFamily;
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
    if (backgroundColor && ![self _elementHasOwnBackgroundColor:element])
        [attrs setObject:backgroundColor forKey:NSBackgroundColorAttributeName];

    if ([self _getFloat:&strokeWidth forNode:element property:@"-webkit-text-stroke-width"]) {
        float textStrokeWidth = strokeWidth / ([font pointSize] * 0.01);
        [attrs setObject:[NSNumber numberWithDouble:textStrokeWidth] forKey:NSStrokeWidthAttributeName];
    }
    if(strokeColor)
        [attrs setObject:strokeColor forKey:NSStrokeColorAttributeName];
    if (fontEffect) {
        if ([fontEffect rangeOfString:@"outline"].location != NSNotFound)
            [attrs setObject:[NSNumber numberWithDouble:3.0] forKey:NSStrokeWidthAttributeName];
        if ([fontEffect rangeOfString:@"emboss"].location != NSNotFound)
            [attrs setObject:[[[PlatformNSShadow alloc] init] autorelease] forKey:NSShadowAttributeName];
    }
    if (fontKerning || letterSpacing) {
        if ([fontEffect rangeOfString:@"none"].location != NSNotFound)
            [attrs setObject:@0.0 forKey:NSKernAttributeName];
        else {
            double kernVal = letterSpacing ? [letterSpacing doubleValue] : 0.0;
            if (UIFloatIsZero(kernVal))
                [attrs setObject:[NSNull null] forKey:NSKernAttributeName]; // auto and normal, the other possible values, are both "kerning enabled"
            else
                [attrs setObject:[NSNumber numberWithDouble:kernVal] forKey:NSKernAttributeName];
        }
    }
    if (fontLigatures) {
        if ([fontEffect rangeOfString:@"normal"].location != NSNotFound)
            ;   // default: whatever the system decides to do
        else if ([fontEffect rangeOfString:@"common-ligatures"].location != NSNotFound)
            [attrs setObject:@1 forKey:NSLigatureAttributeName];   // explicitly enabled
        else if ([fontEffect rangeOfString:@"no-common-ligatures"].location != NSNotFound)
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
    if ([self _getFloat:&baselineOffset forNode:element property:@"vertical-align"])
        [attrs setObject:[NSNumber numberWithDouble:baselineOffset] forKey:NSBaselineOffsetAttributeName];
    if (textShadow && [textShadow length] > 4) {
        NSShadow *shadow = _shadowForShadowStyle(textShadow);
        if (shadow)
            [attrs setObject:shadow forKey:NSShadowAttributeName];
    }
    if (element != blockElement && [_writingDirectionArray count] > 0)
        [attrs setObject:[NSArray arrayWithArray:_writingDirectionArray] forKey:NSWritingDirectionAttributeName];
    
    if (blockElement) {
        NSMutableParagraphStyle *paragraphStyle = [[[self class] defaultParagraphStyle] mutableCopy];
        NSString *blockTag = [blockElement tagName];
        BOOL isParagraph = ([@"P" isEqualToString:blockTag] || [@"LI" isEqualToString:blockTag] || ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]));
        NSString *textAlign = [self _stringForNode:blockElement property:@"text-align"];
        NSString *direction = [self _stringForNode:blockElement property:@"direction"];
        NSString *hyphenation = [self _stringForNode:blockElement property:@"-webkit-hyphens"];
        CGFloat leftMargin = 0;
        CGFloat rightMargin = 0;
        CGFloat bottomMargin = 0;
        CGFloat textIndent = 0;
        CGFloat lineHeight = 0;
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
            if ([self _getFloat:&leftMargin forNode:blockElement property:@"margin-left"] && leftMargin > 0.0)
                [paragraphStyle setHeadIndent:leftMargin];
            if ([self _getFloat:&textIndent forNode:blockElement property:@"text-indent"])
                [paragraphStyle setFirstLineHeadIndent:[paragraphStyle headIndent] + textIndent];
            if ([self _getFloat:&rightMargin forNode:blockElement property:@"margin-right"] && rightMargin > 0.0)
                [paragraphStyle setTailIndent:-rightMargin];
            if ([self _getFloat:&bottomMargin forNode:blockElement property:@"margin-bottom"] && bottomMargin > 0.0)
                [paragraphStyle setParagraphSpacing:bottomMargin];
        }
        if (_webViewTextSizeMultiplier > 0.0 && [self _getFloat:&lineHeight forNode:element property:@"line-height"] && lineHeight > 0.0)
            [paragraphStyle setMinimumLineHeight:lineHeight / _webViewTextSizeMultiplier];
        if ([_textLists count] > 0)
            [paragraphStyle setTextLists:_textLists];
        if ([_textBlocks count] > 0)
            [paragraphStyle setTextBlocks:_textBlocks];
        [attrs setObject:paragraphStyle forKey:NSParagraphStyleAttributeName];
        [paragraphStyle release];
    }
    return attrs;
}

- (NSDictionary *)_attributesForElement:(DOMElement *)element
{
    NSDictionary *result;
    if (element) {
        result = [_attributesForElements objectForKey:element];
        if (!result) {
            result = [self _computedAttributesForElement:element];
            [_attributesForElements setObject:result forKey:element];
        }
    } else
        result = [NSDictionary dictionary];
    return result;

}

- (void)_newParagraphForElement:(DOMElement *)element tag:(NSString *)tag allowEmpty:(BOOL)flag suppressTrailingSpace:(BOOL)suppress
{
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    NSRange rangeToReplace = (suppress && _flags.isSoft && (lastChar == ' ' || lastChar == NSLineSeparatorCharacter)) ? NSMakeRange(textLength - 1, 1) : NSMakeRange(textLength, 0);
    BOOL needBreak = (flag || lastChar != '\n');
    if (needBreak) {
        NSString *string = (([@"BODY" isEqualToString:tag] || [@"HTML" isEqualToString:tag]) ? @"" : @"\n");
        [_writingDirectionArray removeAllObjects];
        [_attrStr replaceCharactersInRange:rangeToReplace withString:string];
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += [string length] - rangeToReplace.length;
        rangeToReplace.length = [string length];
        if (!_flags.isIndexing) {
            NSDictionary *attrs = [self _attributesForElement:element];
            if (!_flags.isTesting && rangeToReplace.length > 0)
                [_attrStr setAttributes:attrs range:rangeToReplace];
        }
        _flags.isSoft = YES;
    }
}

- (void)_newLineForElement:(DOMElement *)element
{
    unichar c = NSLineSeparatorCharacter;
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex) _domRangeStartIndex += rangeToReplace.length;
    if (!_flags.isIndexing) {
        NSDictionary *attrs = [self _attributesForElement:element];
        if (!_flags.isTesting && rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs range:rangeToReplace];
    }
    _flags.isSoft = YES;
}

- (void)_newTabForElement:(DOMElement *)element
{
    NSString *string = @"\t";
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : '\n';
    NSRange rangeToReplace = (_flags.isSoft && lastChar == ' ') ? NSMakeRange(textLength - 1, 1) : NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    if (!_flags.isIndexing) {
        NSDictionary *attrs = [self _attributesForElement:element];
        if (!_flags.isTesting && rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs range:rangeToReplace];
    }
    _flags.isSoft = YES;
}

- (Class)_WebMessageDocumentClass
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

- (BOOL)_addAttachmentForElement:(DOMElement *)element URL:(NSURL *)url needsParagraph:(BOOL)needsParagraph usePlaceholder:(BOOL)flag
{
    BOOL retval = NO, notFound = NO;
    NSFileWrapper *fileWrapper = nil;
    Frame* frame = core([element ownerDocument])->frame();
    DocumentLoader *dataSource = frame->loader().frameHasLoaded() ? frame->loader().documentLoader() : 0;
    BOOL ignoreOrientation = YES;

    if (_flags.isIndexing)
        return NO;
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
        if (flag && resource && mimeType == "text/html")
            notFound = YES;
        if (resource && !notFound) {
            fileWrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:resource->data()->createNSData().get()] autorelease];
            [fileWrapper setPreferredFilename:suggestedFilenameWithMIMEType(url, mimeType)];
        }
    }
#if !PLATFORM(IOS)
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(dataSource, url);
        if (flag && fileWrapper && [[[[fileWrapper preferredFilename] pathExtension] lowercaseString] hasPrefix:@"htm"]) notFound = YES;
        if (notFound) fileWrapper = nil;
    }
    if (!fileWrapper && !notFound) {
        fileWrapper = fileWrapperForURL(_dataSource, url);
        if (flag && fileWrapper && [[[[fileWrapper preferredFilename] pathExtension] lowercaseString] hasPrefix:@"htm"]) notFound = YES;
        if (notFound) fileWrapper = nil;
    }
#endif
    if (!fileWrapper && !notFound && url) {
        // Special handling for Mail attachments, until WebKit provides a standard way to get the data.
        Class WebMessageDocumentClass = [self _WebMessageDocumentClass];
        if (WebMessageDocumentClass) {
            NSTextAttachment *mimeTextAttachment = nil;
            [WebMessageDocumentClass document:NULL attachment:&mimeTextAttachment forURL:url];
            if (mimeTextAttachment && [mimeTextAttachment respondsToSelector:@selector(fileWrapper)]) {
                fileWrapper = [mimeTextAttachment performSelector:@selector(fileWrapper)];
                ignoreOrientation = NO;
            }
        }
    }
    if (fileWrapper || flag) {
        NSUInteger textLength = [_attrStr length];
        RetainPtr<NSTextAttachment> attachment = adoptNS([[PlatformNSTextAttachment alloc] initWithFileWrapper:fileWrapper]);
#if PLATFORM(IOS)
        NSString *vAlign = [self _stringForNode:element property:@"vertical-align"];
        NSString *hAlign = [self _stringForNode:element property:@"horizontal-align"];
        attachment.get().bounds = CGRectMake(([hAlign floatValue] / 100.) * element.clientWidth, ([vAlign floatValue] / 100.) * element.clientHeight, element.clientWidth, element.clientHeight);
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
            NSURL *missingImageURL = [[self _webKitBundle] URLForResource:@"missing_image" withExtension:@"tiff"];
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
        attrs = [self _attributesForElement:element];
        if (!_flags.isTesting && rangeToReplace.length > 0) {
            [_attrStr setAttributes:attrs range:rangeToReplace];
            rangeToReplace.length = 1;
            [_attrStr addAttribute:NSAttachmentAttributeName value:attachment.get() range:rangeToReplace];
        }
        _flags.isSoft = NO;
        retval = YES;
    }
    return retval;
}

- (void)_addQuoteForElement:(DOMElement *)element opening:(BOOL)opening level:(NSInteger)level
{
    unichar c = ((level % 2) == 0) ? (opening ? 0x201c : 0x201d) : (opening ? 0x2018 : 0x2019);
    RetainPtr<NSString> string = adoptNS([[NSString alloc] initWithCharacters:&c length:1]);
    NSUInteger textLength = [_attrStr length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    [_attrStr replaceCharactersInRange:rangeToReplace withString:string.get()];
    rangeToReplace.length = [string length];
    if (rangeToReplace.location < _domRangeStartIndex)
        _domRangeStartIndex += rangeToReplace.length;
    if (!_flags.isIndexing) {
        RetainPtr<NSDictionary> attrs = [self _attributesForElement:element];
        if (!_flags.isTesting && rangeToReplace.length > 0)
            [_attrStr setAttributes:attrs.get() range:rangeToReplace];
    }
    _flags.isSoft = NO;
}

- (void)_addValue:(NSString *)value forElement:(DOMElement *)element
{
    NSUInteger textLength = [_attrStr length];
    NSUInteger valueLength = [value length];
    NSRange rangeToReplace = NSMakeRange(textLength, 0);
    if (valueLength) {
        [_attrStr replaceCharactersInRange:rangeToReplace withString:value];
        rangeToReplace.length = valueLength;
        if (rangeToReplace.location < _domRangeStartIndex)
            _domRangeStartIndex += rangeToReplace.length;
        if (!_flags.isIndexing) {
            RetainPtr<NSDictionary> attrs = [self _attributesForElement:element];
            if (!_flags.isTesting && rangeToReplace.length > 0)
                [_attrStr setAttributes:attrs.get() range:rangeToReplace];
        }
        _flags.isSoft = NO;
    }
}

- (void)_fillInBlock:(NSTextBlock *)block forElement:(DOMElement *)element backgroundColor:(PlatformColor *)backgroundColor extraMargin:(CGFloat)extraMargin extraPadding:(CGFloat)extraPadding isTable:(BOOL)isTable
{
    CGFloat val = 0;
    PlatformColor *color = nil;
    BOOL isTableCellElement = [element isKindOfClass:[DOMHTMLTableCellElement class]];
    NSString *width = isTableCellElement ? [(DOMHTMLTableCellElement *)element width] : [element getAttribute:@"width"];

    if ((width && [width length]) || !isTable) {
        if ([self _getFloat:&val forNode:element property:@"width"])
            [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockWidth];
    }
    
    if ([self _getFloat:&val forNode:element property:@"min-width"])
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumWidth];
    if ([self _getFloat:&val forNode:element property:@"max-width"])
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumWidth];
    if ([self _getFloat:&val forNode:element property:@"min-height"])
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMinimumHeight];
    if ([self _getFloat:&val forNode:element property:@"max-height"])
        [block setValue:val type:NSTextBlockAbsoluteValueType forDimension:NSTextBlockMaximumHeight];

    if ([self _getFloat:&val forNode:element property:@"padding-left"])
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinXEdge];
    if ([self _getFloat:&val forNode:element property:@"padding-top"])
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMinYEdge];
    if ([self _getFloat:&val forNode:element property:@"padding-right"])
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxXEdge];
    if ([self _getFloat:&val forNode:element property:@"padding-bottom"])
        [block setWidth:val + extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge]; else [block setWidth:extraPadding type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockPadding edge:NSMaxYEdge];
    
    if ([self _getFloat:&val forNode:element property:@"border-left-width"])
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinXEdge];
    if ([self _getFloat:&val forNode:element property:@"border-top-width"])
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMinYEdge];
    if ([self _getFloat:&val forNode:element property:@"border-right-width"])
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxXEdge];
    if ([self _getFloat:&val forNode:element property:@"border-bottom-width"])
        [block setWidth:val type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockBorder edge:NSMaxYEdge];

    if ([self _getFloat:&val forNode:element property:@"margin-left"])
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinXEdge];
    if ([self _getFloat:&val forNode:element property:@"margin-top"])
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMinYEdge];
    if ([self _getFloat:&val forNode:element property:@"margin-right"])
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxXEdge];
    if ([self _getFloat:&val forNode:element property:@"margin-bottom"])
        [block setWidth:val + extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge]; else [block setWidth:extraMargin type:NSTextBlockAbsoluteValueType forLayer:NSTextBlockMargin edge:NSMaxYEdge];

    if ((color = [self _colorForNode:element property:@"background-color"]))
        [block setBackgroundColor:color];
    if (!color && backgroundColor) [block setBackgroundColor:backgroundColor];
    if ((color = [self _colorForNode:element property:@"border-left-color"]))
        [block setBorderColor:color forEdge:NSMinXEdge];
    if ((color = [self _colorForNode:element property:@"border-top-color"]))
        [block setBorderColor:color forEdge:NSMinYEdge];
    if ((color = [self _colorForNode:element property:@"border-right-color"]))
        [block setBorderColor:color forEdge:NSMaxXEdge];
    if ((color = [self _colorForNode:element property:@"border-bottom-color"]))
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
    
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
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

- (void)_processMetaElementWithName:(NSString *)name content:(NSString *)content {
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

- (void)_processHeadElement:(DOMElement *)element {
    // ??? should gather data from other sources e.g. Word, but for that we would need to be able to get comments from DOM
    NSArray *childNodes = [self _childrenForNode:element];
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
                    [self _processMetaElementWithName:name content:content];
            }
        }
    }
}

- (BOOL)_enterElement:(DOMElement *)element tag:(NSString *)tag display:(NSString *)displayVal embedded:(BOOL)embedded
{
    if ([@"HEAD" isEqualToString:tag] && !embedded)
        [self _processHeadElement:element];
    else if (!displayVal || !([@"none" isEqualToString:displayVal] || [@"table-column" isEqualToString:displayVal] || [@"table-column-group" isEqualToString:displayVal])) {
        if ([self _elementIsBlockLevel:element] && ![@"BR" isEqualToString:tag] && !([@"table-cell" isEqualToString:displayVal] && [_textTables count] == 0) 
            && !([_textLists count] > 0 && [@"block" isEqualToString:displayVal] && ![@"LI" isEqualToString:tag] && ![@"UL" isEqualToString:tag] && ![@"OL" isEqualToString:tag]))
            [self _newParagraphForElement:element tag:tag allowEmpty:NO suppressTrailingSpace:YES];
        return YES;
    }
    return NO;
}

- (void)_addTableForElement:(DOMElement *)tableElement
{
    RetainPtr<NSTextTable> table = adoptNS([[PlatformNSTextTable alloc] init]);
    CGFloat cellSpacingVal = 1;
    CGFloat cellPaddingVal = 1;
    [table setNumberOfColumns:1];
    [table setLayoutAlgorithm:NSTextTableAutomaticLayoutAlgorithm];
    [table setCollapsesBorders:NO];
    [table setHidesEmptyCells:NO];
    if (tableElement) {
        NSString *borderCollapse = [self _stringForNode:tableElement property:@"border-collapse"];
        NSString *emptyCells = [self _stringForNode:tableElement property:@"empty-cells"];
        NSString *tableLayout = [self _stringForNode:tableElement property:@"table-layout"];
        if ([tableElement respondsToSelector:@selector(cellSpacing)]) {
            NSString *cellSpacing = [(DOMHTMLTableElement *)tableElement cellSpacing];
            if (cellSpacing && [cellSpacing length] > 0 && ![cellSpacing hasSuffix:@"%"]) cellSpacingVal = [cellSpacing floatValue];
        }
        if ([tableElement respondsToSelector:@selector(cellPadding)]) {
            NSString *cellPadding = [(DOMHTMLTableElement *)tableElement cellPadding];
            if (cellPadding && [cellPadding length] > 0 && ![cellPadding hasSuffix:@"%"]) cellPaddingVal = [cellPadding floatValue];
        }
        [self _fillInBlock:table.get() forElement:tableElement backgroundColor:nil extraMargin:0 extraPadding:0 isTable:YES];
        if ([@"collapse" isEqualToString:borderCollapse]) {
            [table setCollapsesBorders:YES];
            cellSpacingVal = 0;
        }
        if ([@"hide" isEqualToString:emptyCells]) [table setHidesEmptyCells:YES];
        if ([@"fixed" isEqualToString:tableLayout]) [table setLayoutAlgorithm:NSTextTableFixedLayoutAlgorithm];
    }
    [_textTables addObject:table.get()];
    [_textTableSpacings addObject:[NSNumber numberWithDouble:cellSpacingVal]];
    [_textTablePaddings addObject:[NSNumber numberWithDouble:cellPaddingVal]];
    [_textTableRows addObject:[NSNumber numberWithInteger:0]];
    [_textTableRowArrays addObject:[NSMutableArray array]];
}

- (void)_addTableCellForElement:(DOMElement *)tableCellElement
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
        NSString *verticalAlign = [self _stringForNode:tableCellElement property:@"vertical-align"];
        [self _fillInBlock:block.get() forElement:tableCellElement backgroundColor:color extraMargin:cellSpacingVal / 2 extraPadding:0 isTable:NO];
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

- (BOOL)_processElement:(DOMElement *)element tag:(NSString *)tag display:(NSString *)displayVal depth:(NSInteger)depth
{
    BOOL retval = YES;
    BOOL isBlockLevel = [self _elementIsBlockLevel:element];
    if (isBlockLevel)
        [_writingDirectionArray removeAllObjects];
    else {
        NSString *bidi = [self _stringForNode:element property:@"unicode-bidi"];
        if (bidi && [bidi isEqualToString:@"embed"]) {
            NSUInteger val = NSTextWritingDirectionEmbedding;
            NSString *direction = [self _stringForNode:element property:@"direction"];
            if ([direction isEqualToString:@"rtl"])
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        } else if (bidi && [bidi isEqualToString:@"bidi-override"]) {
            NSUInteger val = NSTextWritingDirectionOverride;
            NSString *direction = [self _stringForNode:element property:@"direction"];
            if ([direction isEqualToString:@"rtl"])
                val |= NSWritingDirectionRightToLeft;
            [_writingDirectionArray addObject:[NSNumber numberWithUnsignedInteger:val]];
        }
    }
    if ([@"table" isEqualToString:displayVal] || ([_textTables count] == 0 && [@"table-row-group" isEqualToString:displayVal])) {
        DOMElement *tableElement = element;
        if ([@"table-row-group" isEqualToString:displayVal]) {
            // If we are starting in medias res, the first thing we see may be the tbody, so go up to the table
            tableElement = [self _blockLevelElementForNode:[element parentNode]];
            if (![@"table" isEqualToString:[self _stringForNode:tableElement property:@"display"]])
                tableElement = element;
        }
        while ([_textTables count] > [_textBlocks count])
            [self _addTableCellForElement:nil];
        [self _addTableForElement:tableElement];
    } else if ([@"table-footer-group" isEqualToString:displayVal] && [_textTables count] > 0) {
        [_textTableFooters setObject:element forKey:[NSValue valueWithNonretainedObject:[_textTables lastObject]]];
        retval = NO;
    } else if ([@"table-row" isEqualToString:displayVal] && [_textTables count] > 0) {
        PlatformColor *color = [self _colorForNode:element property:@"background-color"];
        if (!color) color = [PlatformColorClass clearColor];
        [_textTableRowBackgroundColors addObject:color];
    } else if ([@"table-cell" isEqualToString:displayVal]) {
        while ([_textTables count] < [_textBlocks count] + 1) {
            [self _addTableForElement:nil];
        }
        [self _addTableCellForElement:element];
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
                [self _addAttachmentForElement:element URL:url needsParagraph:isBlockLevel usePlaceholder:usePlaceholderImage];
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
                url =core([element ownerDocument])->completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
            if (!url)
                url = [NSURL _web_URLWithString:[urlString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:_baseURL];
            if (url)
                retval = ![self _addAttachmentForElement:element URL:url needsParagraph:isBlockLevel usePlaceholder:NO];
        }
    } else if ([@"FRAME" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(contentDocument)]) {
            DOMDocument *contentDocument = [(DOMHTMLFrameElement *)element contentDocument];
            if (contentDocument)
                [self _traverseNode:contentDocument depth:depth + 1 embedded:YES];
        }
        retval = NO;
    } else if ([@"IFRAME" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(contentDocument)]) {
            DOMDocument *contentDocument = [(DOMHTMLIFrameElement *)element contentDocument];
            if (contentDocument) {
                [self _traverseNode:contentDocument depth:depth + 1 embedded:YES];
                retval = NO;
            }
        }
    } else if ([@"BR" isEqualToString:tag]) {
        DOMElement *blockElement = [self _blockLevelElementForNode:[element parentNode]];
        NSString *breakClass = [element getAttribute:@"class"], *blockTag = [blockElement tagName];
        BOOL isExtraBreak = [@"Apple-interchange-newline" isEqualToString:breakClass], blockElementIsParagraph = ([@"P" isEqualToString:blockTag] || [@"LI" isEqualToString:blockTag] || ([blockTag hasPrefix:@"H"] && 2 == [blockTag length]));
        if (isExtraBreak)
            _flags.hasTrailingNewline = YES;
        else {
            if (blockElement && blockElementIsParagraph)
                [self _newLineForElement:element];
            else
                [self _newParagraphForElement:element tag:tag allowEmpty:YES suppressTrailingSpace:NO];
        }
    } else if ([@"UL" isEqualToString:tag]) {
        RetainPtr<NSTextList> list;
        NSString *listStyleType = [self _stringForNode:element property:@"list-style-type"];
        if (!listStyleType || [listStyleType length] == 0)
            listStyleType = @"disc";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:[NSString stringWithFormat:@"{%@}", listStyleType] options:0]);
        [_textLists addObject:list.get()];
    } else if ([@"OL" isEqualToString:tag]) {
        RetainPtr<NSTextList> list;
        NSString *listStyleType = [self _stringForNode:element property:@"list-style-type"];
        if (!listStyleType || [listStyleType length] == 0) listStyleType = @"decimal";
        list = adoptNS([[PlatformNSTextList alloc] initWithMarkerFormat:[NSString stringWithFormat:@"{%@}.", listStyleType] options:0]);
        if ([element respondsToSelector:@selector(start)]) {
            NSInteger startingItemNumber = [(DOMHTMLOListElement *)element start];
            [list setStartingItemNumber:startingItemNumber];
        }
        [_textLists addObject:list.get()];
    } else if ([@"Q" isEqualToString:tag]) {
        [self _addQuoteForElement:element opening:YES level:_quoteLevel++];
    } else if ([@"INPUT" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(type)] && [element respondsToSelector:@selector(value)] && [@"text" compare:[(DOMHTMLInputElement *)element type] options:NSCaseInsensitiveSearch] == NSOrderedSame) {
            NSString *value = [(DOMHTMLInputElement *)element value];
            if (value && [value length] > 0) [self _addValue:value forElement:element];
        }
    } else if ([@"TEXTAREA" isEqualToString:tag]) {
        if ([element respondsToSelector:@selector(value)]) {
            NSString *value = [(DOMHTMLTextAreaElement *)element value];
            if (value && [value length] > 0) [self _addValue:value forElement:element];
        }
        retval = NO;
    }
    return retval;
}

- (void)_addMarkersToList:(NSTextList *)list range:(NSRange)range
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
                    if (!_flags.isIndexing && !_flags.isTesting)
                        attrsToInsert = [PlatformNSTextList _standardMarkerAttributesForAttributes:[_attrStr attributesAtIndex:paragraphRange.location effectiveRange:NULL]];

                    [_attrStr replaceCharactersInRange:NSMakeRange(paragraphRange.location, 0) withString:stringToInsert];
                    if (!_flags.isIndexing && !_flags.isTesting) [_attrStr setAttributes:attrsToInsert range:NSMakeRange(paragraphRange.location, insertLength)];
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
                            if ([tab location] <= listLocation) tabToRemove = tab;
                        }
                        if (tabToRemove) [newStyle removeTabStop:tab]; else break;
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
                    if (!_flags.isIndexing && !_flags.isTesting) [_attrStr addAttribute:NSParagraphStyleAttributeName value:newStyle range:paragraphRange];
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

- (void)_exitElement:(DOMElement *)element tag:(NSString *)tag display:(NSString *)displayVal depth:(NSInteger)depth startIndex:(NSUInteger)startIndex
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
            if (!_flags.isIndexing && !_flags.isTesting)
                [_attrStr addAttribute:NSLinkAttributeName value:url ? (id)url : (id)urlString range:range];
        }
    }
    if (!_flags.reachedEnd && [self _elementIsBlockLevel:element]) {
        [_writingDirectionArray removeAllObjects];
        if ([@"table-cell" isEqualToString:displayVal] && [_textBlocks count] == 0) {
            [self _newTabForElement:element];
        } else if ([_textLists count] > 0 && [@"block" isEqualToString:displayVal] && ![@"LI" isEqualToString:tag] && ![@"UL" isEqualToString:tag] && ![@"OL" isEqualToString:tag]) {
            [self _newLineForElement:element];
        } else {
            [self _newParagraphForElement:element tag:tag allowEmpty:(range.length == 0) suppressTrailingSpace:YES];
        }
    } else if ([_writingDirectionArray count] > 0) {
        NSString *bidi = [self _stringForNode:element property:@"unicode-bidi"];
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
            [self _traverseFooterNode:footer depth:depth + 1];
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
        [self _addMarkersToList:list range:range];
        [_textLists removeLastObject];
    } else if ([@"Q" isEqualToString:tag]) {
        [self _addQuoteForElement:element opening:NO level:--_quoteLevel];
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

- (void)_processText:(DOMCharacterData *)text
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
    whitespaceVal = [self _stringForNode:text property:@"white-space"];
    transformVal = [self _stringForNode:text property:@"text-transform"];
    
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
        if (!_flags.isIndexing) {
            RetainPtr<NSDictionary> attrs;
            DOMElement *element = (DOMElement *)text;
            while (element) {
                // Fill attrs dictionary with attributes from parent nodes, not overwriting ones deeper in the tree
                if([element nodeType] == DOM_ELEMENT_NODE) {
                    RetainPtr<NSMutableDictionary> newAttrs = adoptNS([[self _attributesForElement:element] mutableCopy]);
                    if (attrs) {
                        // Already-set attributes (from lower in the tree) overwrite the higher ones.
                        [newAttrs addEntriesFromDictionary:attrs.get()];
                    }
                    attrs = newAttrs;
                }
                element = (DOMElement *)[element parentNode];
            }
            if (!_flags.isTesting && rangeToReplace.length > 0)
                [_attrStr setAttributes:attrs.get() range:rangeToReplace];
        }
        _flags.isSoft = wasSpace;
    }
    if (mutstr)
        CFRelease(mutstr);
}

- (void)_traverseNode:(DOMNode *)node depth:(NSInteger)depth embedded:(BOOL)embedded
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
    childNodes = [self _childrenForNode:node];
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
                [self _traverseNode:[childNodes objectAtIndex:i] depth:depth + 1 embedded:embedded];
            if (isEnd && i + 1 >= endOffset)
                _flags.reachedEnd = YES;
            if (_thumbnailLimit > 0 && [_attrStr length] >= _thumbnailLimit)
                _flags.reachedEnd = YES;
            if (_flags.reachedEnd) break;
        }
    } else if (nodeType == DOM_ELEMENT_NODE) {
        DOMElement *element = (DOMElement *)node;
        NSString *tag = [element tagName], *displayVal = [self _stringForNode:element property:@"display"], *floatVal = [self _stringForNode:element property:@"float"];
        BOOL isBlockLevel = NO;
        if (floatVal && ([@"left" isEqualToString:floatVal] || [@"right" isEqualToString:floatVal])) {
            isBlockLevel = YES;
        } else if (displayVal) {
            isBlockLevel = ([@"block" isEqualToString:displayVal] || [@"list-item" isEqualToString:displayVal] || [displayVal hasPrefix:@"table"]);
        }
        [_elementIsBlockLevel setObject:[NSNumber numberWithBool:isBlockLevel] forKey:element];
        if ([self _enterElement:element tag:tag display:displayVal embedded:embedded]) {
            NSUInteger startIndex = [_attrStr length];
            if ([self _processElement:element tag:tag display:displayVal depth:depth]) {
                for (NSUInteger i = 0; i < count; i++) {
                    if (isStart && i == startOffset)
                        _domRangeStartIndex = [_attrStr length];
                    if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                        [self _traverseNode:[childNodes objectAtIndex:i] depth:depth + 1 embedded:embedded];
                    if (isEnd && i + 1 >= endOffset)
                        _flags.reachedEnd = YES;
                    if (_flags.reachedEnd)
                        break;
                }
                [self _exitElement:element tag:tag display:displayVal depth:depth startIndex:startIndex];
            }
        }
    } else if (nodeType == DOM_TEXT_NODE || nodeType == DOM_CDATA_SECTION_NODE) {
        [self _processText:(DOMCharacterData *)node];
    }
    
    if (isEnd) _flags.reachedEnd = YES;
}

- (void)_traverseFooterNode:(DOMNode *)node depth:(NSInteger)depth
{
    DOMElement *element = (DOMElement *)node;
    NSArray *childNodes = [self _childrenForNode:node];
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
    if ([self _enterElement:element tag:tag display:displayVal embedded:YES]) {
        NSUInteger startIndex = [_attrStr length];
        if ([self _processElement:element tag:tag display:displayVal depth:depth]) {
            for (NSUInteger i = 0; i < count; i++) {
                if (isStart && i == startOffset)
                    _domRangeStartIndex = [_attrStr length];
                if ((!isStart || startOffset <= i) && (!isEnd || endOffset > i))
                    [self _traverseNode:[childNodes objectAtIndex:i] depth:depth + 1 embedded:YES];
                if (isEnd && i + 1 >= endOffset)
                    _flags.reachedEnd = YES;
                if (_flags.reachedEnd)
                    break;
            }
            [self _exitElement:element tag:tag display:displayVal depth:depth startIndex:startIndex];
        }
    }
    if (isEnd)
        _flags.reachedEnd = YES;
}

- (void)_adjustTrailingNewline
{
    NSUInteger textLength = [_attrStr length];
    unichar lastChar = (textLength > 0) ? [[_attrStr string] characterAtIndex:textLength - 1] : 0;
    BOOL alreadyHasTrailingNewline = (lastChar == '\n' || lastChar == '\r' || lastChar == NSParagraphSeparatorCharacter || lastChar == NSLineSeparatorCharacter || lastChar == WebNextLineCharacter);
    if (_flags.hasTrailingNewline && !alreadyHasTrailingNewline)
        [_attrStr replaceCharactersInRange:NSMakeRange(textLength, 0) withString:@"\n"];
}

- (void)_loadFromDOMRange
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
        if (_textSizeMultiplier <= 0.0)
            _textSizeMultiplier = 1;
        if (_defaultFontSize <= 0.0)
            _defaultFontSize = 12;
        if (_minimumFontSize < 1.0)
            _minimumFontSize = 1;
        if (_document && _dataSource) {
            _domRangeStartIndex = 0;
            _errorCode = 0;
            [self _traverseNode:commonAncestorContainer depth:0 embedded:NO];
            if (_domRangeStartIndex > 0 && _domRangeStartIndex <= [_attrStr length])
                [_attrStr deleteCharactersInRange:NSMakeRange(0, _domRangeStartIndex)];
        }
    }
}

- (void)dealloc
{
    [_attrStr release];
    [_documentAttrs release];
    [_domRange release];
    [_domStartAncestors release];
    [_standardFontFamily release];
    [_textLists release];
    [_textBlocks release];
    [_textTables release];
    [_textTableFooters release];
    [_textTableSpacings release];
    [_textTablePaddings release];
    [_textTableRows release];
    [_textTableRowArrays release];
    [_textTableRowBackgroundColors release];
    [_computedStylesForElements release];
    [_specifiedStylesForElements release];
    [_stringsForNodes release];
    [_floatsForNodes release];
    [_colorsForNodes release];
    [_attributesForElements release];
    [_elementIsBlockLevel release];
    [_fontCache release];
    [_writingDirectionArray release];
    [super dealloc];
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _attrStr = [[NSMutableAttributedString alloc] init];
    _documentAttrs = [[NSMutableDictionary alloc] init];

    _textLists = [[NSMutableArray alloc] init];
    _textBlocks = [[NSMutableArray alloc] init];
    _textTables = [[NSMutableArray alloc] init];
    _textTableFooters = [[NSMutableDictionary alloc] init];
    _textTableSpacings = [[NSMutableArray alloc] init];
    _textTablePaddings = [[NSMutableArray alloc] init];
    _textTableRows = [[NSMutableArray alloc] init];
    _textTableRowArrays = [[NSMutableArray alloc] init];
    _textTableRowBackgroundColors = [[NSMutableArray alloc] init];
    _computedStylesForElements = [[NSMutableDictionary alloc] init];
    _specifiedStylesForElements = [[NSMutableDictionary alloc] init];
    _stringsForNodes = [[NSMutableDictionary alloc] init];
    _floatsForNodes = [[NSMutableDictionary alloc] init];
    _colorsForNodes = [[NSMutableDictionary alloc] init];
    _attributesForElements = [[NSMutableDictionary alloc] init];
    _elementIsBlockLevel = [[NSMutableDictionary alloc] init];
    _fontCache = [[NSMutableDictionary alloc] init];
    _writingDirectionArray = [[NSMutableArray alloc] init];

    _textSizeMultiplier = 1;
    _webViewTextSizeMultiplier = 0;
    _defaultTabInterval = 36;
    _defaultFontSize = 12;
    _minimumFontSize = 1;
    _errorCode = -1;
    _indexingLimit = 0;
    _thumbnailLimit = 0;

    _flags.isIndexing = (_indexingLimit > 0);
    _flags.isTesting = 0;
    
    return self;
}

- (id)initWithDOMRange:(DOMRange *)domRange
{
    self = [self init];
    if (!self)
        return nil;
    _domRange = [domRange retain];
    return self;
}

// This function supports more HTML features than the editing variant below, such as tables.
- (NSAttributedString *)attributedString
{
    [self _loadFromDOMRange];
    return (!_errorCode) ? [[_attrStr retain] autorelease] : nil;
}

#if !PLATFORM(IOS)

// This function uses TextIterator, which makes offsets in its result compatible with HTML editing.
+ (NSAttributedString *)editingAttributedStringFromRange:(Range*)range
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSMutableAttributedString *string = [[NSMutableAttributedString alloc] init];
    NSUInteger stringLength = 0;
    RetainPtr<NSMutableDictionary> attrs = adoptNS([[NSMutableDictionary alloc] init]);

    for (TextIterator it(range); !it.atEnd(); it.advance()) {
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

@end

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
