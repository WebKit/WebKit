/*
* Copyright (C) 2021 Apple Inc. All rights reserved.
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

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#import <wtf/Platform.h>

#if HAVE(PDFKIT)

#import <PDFKit/PDFKit.h>

#import <wtf/SoftLinking.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, PDFKit)

SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFActionResetForm)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFActionNamed)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFAnnotation)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFDocument)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFPage)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFSelection)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFView)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, kPDFDestinationUnspecifiedValue, CGFloat)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFViewCopyPermissionNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFDocumentCreationDateAttribute, PDFDocumentAttribute)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationKeySubtype, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationKeyWidgetFieldType, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationSubtypeLink, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationSubtypePopup, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationSubtypeText, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationSubtypeWidget, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationWidgetSubtypeButton, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationWidgetSubtypeChoice, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationWidgetSubtypeSignature, NSString *)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, PDFKit, PDFAnnotationWidgetSubtypeText, NSString *)

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
SOFT_LINK_CLASS_FOR_HEADER(PAL, PDFLayerController)
#endif

SPECIALIZE_OBJC_TYPE_TRAITS(PDFActionResetForm, PAL::getPDFActionResetFormClassSingleton())
SPECIALIZE_OBJC_TYPE_TRAITS(PDFActionNamed, PAL::getPDFActionNamedClassSingleton())
SPECIALIZE_OBJC_TYPE_TRAITS(PDFAnnotation, PAL::getPDFAnnotationClassSingleton())
SPECIALIZE_OBJC_TYPE_TRAITS(PDFDocument, PAL::getPDFDocumentClassSingleton())
SPECIALIZE_OBJC_TYPE_TRAITS(PDFPage, PAL::getPDFPageClassSingleton())
SPECIALIZE_OBJC_TYPE_TRAITS(PDFSelection, PAL::getPDFSelectionClassSingleton())

#endif // HAVE(PDFKIT)

#endif // !__has_feature(modules)
