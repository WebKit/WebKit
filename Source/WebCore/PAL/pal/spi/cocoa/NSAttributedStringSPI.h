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

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#ifdef __cplusplus

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#import <wtf/SoftLinking.h>

#if PLATFORM(IOS_FAMILY)

#import <UIKit/NSAttributedString.h>

SOFT_LINK_PRIVATE_FRAMEWORK_REQUIRED(UIFoundation)

SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSFontAttributeName, NSString *)
#define NSFontAttributeName getNSFontAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSForegroundColorAttributeName, NSString *)
#define NSForegroundColorAttributeName getNSForegroundColorAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSBackgroundColorAttributeName, NSString *)
#define NSBackgroundColorAttributeName getNSBackgroundColorAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSStrokeColorAttributeName, NSString *)
#define NSStrokeColorAttributeName getNSStrokeColorAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSStrokeWidthAttributeName, NSString *)
#define NSStrokeWidthAttributeName getNSStrokeWidthAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSShadowAttributeName, NSString *)
#define NSShadowAttributeName getNSShadowAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSKernAttributeName, NSString *)
#define NSKernAttributeName getNSKernAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSLigatureAttributeName, NSString *)
#define NSLigatureAttributeName getNSLigatureAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSUnderlineStyleAttributeName, NSString *)
#define NSUnderlineStyleAttributeName getNSUnderlineStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSStrikethroughStyleAttributeName, NSString *)
#define NSStrikethroughStyleAttributeName getNSStrikethroughStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSBaselineOffsetAttributeName, NSString *)
#define NSBaselineOffsetAttributeName getNSBaselineOffsetAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSWritingDirectionAttributeName, NSString *)
#define NSWritingDirectionAttributeName getNSWritingDirectionAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSParagraphStyleAttributeName, NSString *)
#define NSParagraphStyleAttributeName getNSParagraphStyleAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSPresentationIntentAttributeName, NSString *)
#define NSPresentationIntentAttributeName getNSPresentationIntentAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSAttachmentAttributeName, NSString *)
#define NSAttachmentAttributeName getNSAttachmentAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSLinkAttributeName, NSString *)
#define NSLinkAttributeName getNSLinkAttributeNameSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSAuthorDocumentAttribute, NSString *)
#define NSAuthorDocumentAttribute getNSAuthorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSEditorDocumentAttribute, NSString *)
#define NSEditorDocumentAttribute getNSEditorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSGeneratorDocumentAttribute, NSString *)
#define NSGeneratorDocumentAttribute getNSGeneratorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSCompanyDocumentAttribute, NSString *)
#define NSCompanyDocumentAttribute getNSCompanyDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSDisplayNameDocumentAttribute, NSString *)
#define NSDisplayNameDocumentAttribute getNSDisplayNameDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSCopyrightDocumentAttribute, NSString *)
#define NSCopyrightDocumentAttribute getNSCopyrightDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSSubjectDocumentAttribute, NSString *)
#define NSSubjectDocumentAttribute getNSSubjectDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSCommentDocumentAttribute, NSString *)
#define NSCommentDocumentAttribute getNSCommentDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSNoIndexDocumentAttribute, NSString *)
#define NSNoIndexDocumentAttribute getNSNoIndexDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSKeywordsDocumentAttribute, NSString *)
#define NSKeywordsDocumentAttribute getNSKeywordsDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSCreationTimeDocumentAttribute, NSString *)
#define NSCreationTimeDocumentAttribute getNSCreationTimeDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSModificationTimeDocumentAttribute, NSString *)
#define NSModificationTimeDocumentAttribute getNSModificationTimeDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSConvertedDocumentAttribute, NSString *)
#define NSConvertedDocumentAttribute getNSConvertedDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSCocoaVersionDocumentAttribute, NSString *)
#define NSCocoaVersionDocumentAttribute getNSCocoaVersionDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSBackgroundColorDocumentAttribute, NSString *)
#define NSBackgroundColorDocumentAttribute getNSBackgroundColorDocumentAttributeSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSMarkedClauseSegmentAttributeName, NSString *)
#define NSMarkedClauseSegmentAttributeName getNSMarkedClauseSegmentAttributeNameSingleton()

#import <UIKit/NSTextList.h>
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerCircle, NSTextListMarkerFormat)
#define NSTextListMarkerCircle getNSTextListMarkerCircleSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerDisc, NSTextListMarkerFormat)
#define NSTextListMarkerDisc getNSTextListMarkerDiscSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerSquare, NSTextListMarkerFormat)
#define NSTextListMarkerSquare getNSTextListMarkerSquareSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerLowercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseHexadecimal getNSTextListMarkerLowercaseHexadecimalSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerUppercaseHexadecimal, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseHexadecimal getNSTextListMarkerUppercaseHexadecimalSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerOctal, NSTextListMarkerFormat)
#define NSTextListMarkerOctal getNSTextListMarkerOctalSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerLowercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseAlpha getNSTextListMarkerLowercaseAlphaSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerUppercaseAlpha, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseAlpha getNSTextListMarkerUppercaseAlphaSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerLowercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseLatin getNSTextListMarkerLowercaseLatinSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerUppercaseLatin, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseLatin getNSTextListMarkerUppercaseLatinSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerLowercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerLowercaseRoman getNSTextListMarkerLowercaseRomanSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerUppercaseRoman, NSTextListMarkerFormat)
#define NSTextListMarkerUppercaseRoman getNSTextListMarkerUppercaseRomanSingleton()
SOFT_LINK_CONSTANT_REQUIRED(UIFoundation, NSTextListMarkerDecimal, NSTextListMarkerFormat)
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

#endif // __cplusplus

#endif // !__has_feature(modules)
