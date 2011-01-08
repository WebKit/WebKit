/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "PDFViewController.h"

#import "DataReference.h"
#import <PDFKit/PDFKit.h>
#import <wtf/text/WTFString.h>

@class PDFDocument;
@class PDFView;

@interface PDFDocument (PDFDocumentDetails)
- (NSPrintOperation *)getPrintOperationForPrintInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate;
@end

extern "C" NSString *_NSPathForSystemFramework(NSString *framework);
    
@interface WKPDFView : NSView
{
    WebKit::PDFViewController* _pdfViewController;

    RetainPtr<NSView> _pdfPreviewView;
    PDFView *_pdfView;
}

- (id)initWithFrame:(NSRect)frame PDFViewController:(WebKit::PDFViewController*)pdfViewController;
- (void)invalidate;
- (PDFView *)pdfView;

@end

@implementation WKPDFView

- (id)initWithFrame:(NSRect)frame PDFViewController:(WebKit::PDFViewController*)pdfViewController
{
    if ((self = [super initWithFrame:frame])) {
        [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
        Class previewViewClass = WebKit::PDFViewController::pdfPreviewViewClass();
        ASSERT(previewViewClass);
        
        _pdfPreviewView.adoptNS([[previewViewClass alloc] initWithFrame:frame]);
        [_pdfPreviewView.get() setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:_pdfPreviewView.get()];

        _pdfView = [_pdfPreviewView.get() performSelector:@selector(pdfView)];
    }

    return self;
}

- (void)invalidate
{
    _pdfViewController = nil;
}

- (PDFView *)pdfView
{
    return _pdfView;
}

@end

namespace WebKit {

PassOwnPtr<PDFViewController> PDFViewController::create(WKView *wkView)
{
    return adoptPtr(new PDFViewController(wkView));
}

PDFViewController::PDFViewController(WKView *wkView)
    : m_wkView(wkView)
    , m_wkPDFView(AdoptNS, [[WKPDFView alloc] initWithFrame:[m_wkView bounds] PDFViewController:this])
    , m_pdfView([m_wkPDFView.get() pdfView])
{
    [m_wkView addSubview:m_wkPDFView.get()];
}

PDFViewController::~PDFViewController()
{
    [m_wkPDFView.get() removeFromSuperview];
    [m_wkPDFView.get() invalidate];
    m_wkPDFView = nullptr;
}

static RetainPtr<CFDataRef> convertPostScriptDataSourceToPDF(const CoreIPC::DataReference& dataReference)
{
    // Convert PostScript to PDF using Quartz 2D API
    // http://developer.apple.com/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_ps_convert/chapter_16_section_1.html
    
    CGPSConverterCallbacks callbacks = { 0, 0, 0, 0, 0, 0, 0, 0 };    
    RetainPtr<CGPSConverterRef> converter(AdoptCF, CGPSConverterCreate(0, &callbacks, 0));
    ASSERT(converter);

    RetainPtr<NSData> nsData(AdoptNS, [[NSData alloc] initWithBytesNoCopy:const_cast<uint8_t*>(dataReference.data()) length:dataReference.size() freeWhenDone:NO]);   

    RetainPtr<CGDataProviderRef> provider(AdoptCF, CGDataProviderCreateWithCFData((CFDataRef)nsData.get()));
    ASSERT(provider);

    RetainPtr<CFMutableDataRef> result(AdoptCF, CFDataCreateMutable(kCFAllocatorDefault, 0));
    ASSERT(result);
    
    RetainPtr<CGDataConsumerRef> consumer(AdoptCF, CGDataConsumerCreateWithCFData(result.get()));
    ASSERT(consumer);
    
    CGPSConverterConvert(converter.get(), provider.get(), consumer.get(), 0);

    if (!result)
        return 0;

    return result;
}

void PDFViewController::setPDFDocumentData(const String& mimeType, const CoreIPC::DataReference& dataReference)
{
    RetainPtr<CFDataRef> data;
    
    if (equalIgnoringCase(mimeType, "application/postscript")) {
        data = convertPostScriptDataSourceToPDF(dataReference);
        if (!data)
            return;
    } else {
        // Make sure to copy the data.
        data.adoptCF(CFDataCreate(0, dataReference.data(), dataReference.size()));
    }

    RetainPtr<PDFDocument> pdfDocument(AdoptNS, [[pdfDocumentClass() alloc] initWithData:(NSData *)data.get()]);
    [m_pdfView setDocument:pdfDocument.get()];
}

double PDFViewController::zoomFactor() const
{
    return [m_pdfView scaleFactor];
}

void PDFViewController::setZoomFactor(double zoomFactor)
{
    [m_pdfView setScaleFactor:zoomFactor];
}

Class PDFViewController::pdfDocumentClass()
{
    static Class pdfDocumentClass = [pdfKitBundle() classNamed:@"PDFDocument"];
    
    return pdfDocumentClass;
}

Class PDFViewController::pdfPreviewViewClass()
{
    static Class pdfPreviewViewClass = [pdfKitBundle() classNamed:@"PDFPreviewView"];
    
    return pdfPreviewViewClass;
}
    
NSBundle* PDFViewController::pdfKitBundle()
{
    static NSBundle *pdfKitBundle;
    if (pdfKitBundle)
        return pdfKitBundle;

    NSString *pdfKitPath = [_NSPathForSystemFramework(@"Quartz.framework") stringByAppendingString:@"/Frameworks/PDFKit.framework"];
    if (!pdfKitPath) {
        LOG_ERROR("Couldn't find PDFKit.framework");
        return nil;
    }

    pdfKitBundle = [NSBundle bundleWithPath:pdfKitPath];
    if (![pdfKitBundle load])
        LOG_ERROR("Couldn't load PDFKit.framework");
    return pdfKitBundle;
}

NSPrintOperation *PDFViewController::makePrintOperation(NSPrintInfo *printInfo)
{
    return [[m_pdfView document] getPrintOperationForPrintInfo:printInfo autoRotate:YES];
}

} // namespace WebKit
