/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)

#import <UIKit/NSAttributedString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(UIFoundation)

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
SOFT_LINK_CONSTANT(UIFoundation, NSBackgroundColorDocumentAttribute, NSString *)
#define NSBackgroundColorDocumentAttribute getNSBackgroundColorDocumentAttribute()
SOFT_LINK_CONSTANT(UIFoundation, NSMarkedClauseSegmentAttributeName, NSString *)
#define NSMarkedClauseSegmentAttributeName getNSMarkedClauseSegmentAttributeName()
SOFT_LINK_CONSTANT(UIFoundation, NSTextAlternativesAttributeName, NSString *)
#define NSTextAlternativesAttributeName getNSTextAlternativesAttributeName()

#if HAVE(NSTEXTLIST_MARKER_FORMATS)
#if USE(APPLE_INTERNAL_SDK)
#import <UIKit/NSTextList.h>
#else
typedef NSString *NSTextListMarkerFormat;
#endif // USE(APPLE_INTERNAL_SDK)
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerCircle, NSTextListMarkerFormat)
#define NSTextListMarkerCircle getNSTextListMarkerCircle()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerDisc, NSTextListMarkerFormat)
#define NSTextListMarkerDisc getNSTextListMarkerDisc()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerSquare, NSTextListMarkerFormat)
#define NSTextListMarkerSquare getNSTextListMarkerSquare()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseHexadecimal getNSTextListMarkerLowercaseHexadecimal()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseHexadecimal getNSTextListMarkerUppercaseHexadecimal()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerOctal, NSTextListMarkerFormat)
#define NSTextListMarkerOctal getNSTextListMarkerOctal()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseAlpha getNSTextListMarkerLowercaseAlpha()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseAlpha getNSTextListMarkerUppercaseAlpha()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseLatin getNSTextListMarkerLowercaseLatin()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseLatin getNSTextListMarkerUppercaseLatin()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseRoman getNSTextListMarkerLowercaseRoman()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseRoman getNSTextListMarkerUppercaseRoman()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerDecimal, NSTextListMarkerFormat)
#define NSTextListMarkerDecimal getNSTextListMarkerDecimal()
#else
typedef NSString *NSTextListMarkerFormat;
static NSTextListMarkerFormat const NSTextListMarkerCircle = @"{circle}";
static NSTextListMarkerFormat const NSTextListMarkerDisc = @"{disc}";
static NSTextListMarkerFormat const NSTextListMarkerSquare = @"{square}";
static NSTextListMarkerFormat const NSTextListMarkerLowercaseHexadecimal = @"{lower-hexadecimal}";
static NSTextListMarkerFormat const NSTextListMarkerUppercaseHexadecimal = @"{upper-hexadecimal}";
static NSTextListMarkerFormat const NSTextListMarkerOctal = @"{octal}";
static NSTextListMarkerFormat const NSTextListMarkerLowercaseAlpha = @"{lower-alpha}";
static NSTextListMarkerFormat const NSTextListMarkerUppercaseAlpha = @"{upper-alpha}";
static NSTextListMarkerFormat const NSTextListMarkerLowercaseLatin = @"{lower-latin}";
static NSTextListMarkerFormat const NSTextListMarkerUppercaseLatin = @"{upper-latin}";
static NSTextListMarkerFormat const NSTextListMarkerLowercaseRoman = @"{lower-roman}";
static NSTextListMarkerFormat const NSTextListMarkerUppercaseRoman = @"{upper-roman}";
static NSTextListMarkerFormat const NSTextListMarkerDecimal = @"{decimal}";
#endif // HAVE(NSTEXTLIST_MARKER_FORMATS)

// We don't softlink NSSuperscriptAttributeName because UIFoundation stopped exporting it.
// This attribute is being deprecated at the API level, but internally UIFoundation
// will continue to support it.
static NSString *const NSSuperscriptAttributeName = @"NSSuperscript";

static NSString *const NSExcludedElementsDocumentAttribute = @"ExcludedElements";

@interface NSAttributedString ()
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

#endif // PLATFORM(IOS_FAMILY)
