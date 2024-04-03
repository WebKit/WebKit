/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_PDF) && PLATFORM(MAC)

#include "PDFDocumentLayout.h"
#include "PDFPluginBase.h"
#include "UnifiedPDFPlugin.h"
#include <PDFKit/PDFKit.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakObjCPtr.h>

namespace WebCore {
class HTMLPlugInElement;
class WeakPtrImplWithEventTargetData;
}

@interface WKAccessibilityPDFDocumentObject: NSObject {
    RetainPtr<PDFDocument> _pdfDocument;
    WeakObjCPtr<NSObject> _parent;
    ThreadSafeWeakPtr<WebKit::UnifiedPDFPlugin> _pdfPlugin;
}

@property (assign) WeakPtr<WebCore::HTMLPlugInElement, WebCore::WeakPtrImplWithEventTargetData> pluginElement;

- (id)initWithPDFDocument:(RetainPtr<PDFDocument>)document andElement:(WebCore::HTMLPlugInElement*)element;
- (void)setParent:(NSObject *)parent;
- (void)setPDFDocument:(RetainPtr<PDFDocument>)document;
- (void)setPDFPlugin:(WebKit::UnifiedPDFPlugin*)plugin;
- (PDFDocument *)document;
- (NSObject *)accessibilityParent;
- (id)accessibilityHitTest:(NSPoint)point;
- (void)gotoDestination:(PDFDestination *)destination;
- (NSRect)convertFromPDFPageToScreenForAccessibility:(NSRect)rectInPageCoordinate pageIndex:(WebKit::PDFDocumentLayout::PageIndex)pageIndex;

@end

#endif
