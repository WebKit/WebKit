/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)

#import <UIKit/NSAttributedString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(UIFoundation)

SOFT_LINK_CONSTANT(UIFoundation, NSFontAttributeName, NSString *)
#define NSFontAttributeName getNSFontAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSForegroundColorAttributeName, NSString *)
#define NSForegroundColorAttributeName getNSForegroundColorAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSBackgroundColorAttributeName, NSString *)
#define NSBackgroundColorAttributeName getNSBackgroundColorAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSStrokeColorAttributeName, NSString *)
#define NSStrokeColorAttributeName getNSStrokeColorAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSStrokeWidthAttributeName, NSString *)
#define NSStrokeWidthAttributeName getNSStrokeWidthAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSShadowAttributeName, NSString *)
#define NSShadowAttributeName getNSShadowAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSKernAttributeName, NSString *)
#define NSKernAttributeName getNSKernAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSLigatureAttributeName, NSString *)
#define NSLigatureAttributeName getNSLigatureAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSUnderlineStyleAttributeName, NSString *)
#define NSUnderlineStyleAttributeName getNSUnderlineStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSStrikethroughStyleAttributeName, NSString *)
#define NSStrikethroughStyleAttributeName getNSStrikethroughStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSBaselineOffsetAttributeName, NSString *)
#define NSBaselineOffsetAttributeName getNSBaselineOffsetAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSWritingDirectionAttributeName, NSString *)
#define NSWritingDirectionAttributeName getNSWritingDirectionAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSParagraphStyleAttributeName, NSString *)
#define NSParagraphStyleAttributeName getNSParagraphStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSPresentationIntentAttributeName, NSString *)
#define NSPresentationIntentAttributeName getNSPresentationIntentAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSAttachmentAttributeName, NSString *)
#define NSAttachmentAttributeName getNSAttachmentAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSLinkAttributeName, NSString *)
#define NSLinkAttributeName getNSLinkAttributeNameSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSAuthorDocumentAttribute, NSString *)
#define NSAuthorDocumentAttribute getNSAuthorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSEditorDocumentAttribute, NSString *)
#define NSEditorDocumentAttribute getNSEditorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSGeneratorDocumentAttribute, NSString *)
#define NSGeneratorDocumentAttribute getNSGeneratorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSCompanyDocumentAttribute, NSString *)
#define NSCompanyDocumentAttribute getNSCompanyDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSDisplayNameDocumentAttribute, NSString *)
#define NSDisplayNameDocumentAttribute getNSDisplayNameDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSCopyrightDocumentAttribute, NSString *)
#define NSCopyrightDocumentAttribute getNSCopyrightDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSSubjectDocumentAttribute, NSString *)
#define NSSubjectDocumentAttribute getNSSubjectDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSCommentDocumentAttribute, NSString *)
#define NSCommentDocumentAttribute getNSCommentDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSNoIndexDocumentAttribute, NSString *)
#define NSNoIndexDocumentAttribute getNSNoIndexDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSKeywordsDocumentAttribute, NSString *)
#define NSKeywordsDocumentAttribute getNSKeywordsDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSCreationTimeDocumentAttribute, NSString *)
#define NSCreationTimeDocumentAttribute getNSCreationTimeDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSModificationTimeDocumentAttribute, NSString *)
#define NSModificationTimeDocumentAttribute getNSModificationTimeDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSConvertedDocumentAttribute, NSString *)
#define NSConvertedDocumentAttribute getNSConvertedDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSCocoaVersionDocumentAttribute, NSString *)
#define NSCocoaVersionDocumentAttribute getNSCocoaVersionDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSBackgroundColorDocumentAttribute, NSString *)
#define NSBackgroundColorDocumentAttribute getNSBackgroundColorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSMarkedClauseSegmentAttributeName, NSString *)
#define NSMarkedClauseSegmentAttributeName getNSMarkedClauseSegmentAttributeNameSingleton()

#import <UIKit/NSTextList.h>
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerCircle, NSTextListMarkerFormat)
#define NSTextListMarkerCircle getNSTextListMarkerCircleSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerDisc, NSTextListMarkerFormat)
#define NSTextListMarkerDisc getNSTextListMarkerDiscSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerSquare, NSTextListMarkerFormat)
#define NSTextListMarkerSquare getNSTextListMarkerSquareSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseHexadecimal getNSTextListMarkerLowercaseHexadecimalSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseHexadecimal getNSTextListMarkerUppercaseHexadecimalSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerOctal, NSTextListMarkerFormat)
#define NSTextListMarkerOctal getNSTextListMarkerOctalSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseAlpha getNSTextListMarkerLowercaseAlphaSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseAlpha getNSTextListMarkerUppercaseAlphaSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseLatin getNSTextListMarkerLowercaseLatinSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseLatin getNSTextListMarkerUppercaseLatinSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerLowercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseRoman getNSTextListMarkerLowercaseRomanSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerUppercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseRoman getNSTextListMarkerUppercaseRomanSingleton()
SOFT_LINK_CONSTANT(UIFoundation, NSTextListMarkerDecimal, NSTextListMarkerFormat)
#define NSTextListMarkerDecimal getNSTextListMarkerDecimalSingleton()

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
