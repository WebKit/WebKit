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

#import "config.h"

#if HAVE(PDFKIT)

#import "PDFKitSPI.h"
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebKit, PDFKit)

#if PLATFORM(APPLETV)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, PDFKit, PDFHostViewController)
#endif

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, PDFKit, PDFActionResetForm)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, PDFKit, PDFDocument)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, PDFKit, PDFLayerController)
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, PDFKit, PDFSelection)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, kPDFDestinationUnspecifiedValue, CGFloat)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFViewCopyPermissionNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFDocumentCreationDateAttribute, PDFDocumentAttribute)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationKeySubtype, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationKeyWidgetFieldType, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationSubtypeLink, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationSubtypePopup, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationSubtypeText, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationSubtypeWidget, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationWidgetSubtypeButton, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationWidgetSubtypeChoice, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationWidgetSubtypeSignature, NSString *)
SOFT_LINK_CONSTANT_FOR_SOURCE(WebKit, PDFKit, PDFAnnotationWidgetSubtypeText, NSString *)

#endif // HAVE(PDFKIT)
