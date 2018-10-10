/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !PLATFORM(IOS)

#import "WebPDFRepresentation.h"

#import "WebDataSourcePrivate.h"
#import "WebFrame.h"
#import "WebJSPDFDoc.h"
#import "WebPDFDocumentExtras.h"
#import "WebPDFView.h"
#import "WebTypesInternal.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <wtf/Assertions.h>
#import <wtf/ObjcRuntimeExtras.h>
#import <wtf/RetainPtr.h>

@implementation WebPDFRepresentation

+ (NSArray *)postScriptMIMETypes
{
    return [NSArray arrayWithObjects:
        @"application/postscript",
        nil];
}

+ (NSArray *)supportedMIMETypes
{
    return [[[self class] postScriptMIMETypes] arrayByAddingObjectsFromArray:
        [NSArray arrayWithObjects:
            @"text/pdf",
            @"application/pdf",
            nil]];
}

+ (Class)PDFDocumentClass
{
    static Class PDFDocumentClass = nil;
    if (PDFDocumentClass == nil) {
        PDFDocumentClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFDocument"];
        if (PDFDocumentClass == nil) {
            LOG_ERROR("Couldn't find PDFDocument class in PDFKit.framework");
        }
    }
    return PDFDocumentClass;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
}

- (NSData *)convertPostScriptDataSourceToPDF:(NSData *)data
{
    // Convert PostScript to PDF using Quartz 2D API
    // http://developer.apple.com/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_ps_convert/chapter_16_section_1.html

    CGPSConverterCallbacks callbacks = { 0, 0, 0, 0, 0, 0, 0, 0 };    
    RetainPtr<CGPSConverterRef> converter = adoptCF(CGPSConverterCreate(0, &callbacks, 0));
    ASSERT(converter.get());

    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithCFData((CFDataRef)data));
    ASSERT(provider.get());

    RetainPtr<CFMutableDataRef> result = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    ASSERT(result.get());

    RetainPtr<CGDataConsumerRef> consumer = adoptCF(CGDataConsumerCreateWithCFData(result.get()));
    ASSERT(consumer.get());

    // Error handled by detecting zero-length 'result' in caller
    CGPSConverterConvert(converter.get(), provider.get(), consumer.get(), 0);

    return result.bridgingAutorelease();
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    NSData *data = [dataSource data];

    NSArray *postScriptMIMETypes = [[self class] postScriptMIMETypes];
    NSString *mimeType = [dataSource _responseMIMEType];
    if ([postScriptMIMETypes containsObject:mimeType]) {
        data = [self convertPostScriptDataSourceToPDF:data];
        if ([data length] == 0)
            return;
    }

    WebPDFView *view = (WebPDFView *)[[[dataSource webFrame] frameView] documentView];
    PDFDocument *doc = [[[[self class] PDFDocumentClass] alloc] initWithData:data];
    [view setPDFDocument:doc];

    NSArray *scripts = allScriptsInPDFDocument(doc);
    [doc release];
    doc = nil;

    if (![scripts count])
        return;

    JSGlobalContextRef ctx = JSGlobalContextCreate(0);
    JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx, dataSource);
    for (NSString *script in scripts)
        JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script).get(), jsPDFDoc, nullptr, 0, nullptr);
    JSGlobalContextRelease(ctx);
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}


- (NSString *)documentSource
{
    return nil;
}


- (NSString *)title
{
    return nil;
}

@end

#endif // !PLATFORM(IOS)
