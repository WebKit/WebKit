/*	
    WebPDFRepresentation.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#ifndef OMIT_TIGER_FEATURES

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPDFRepresentation.h>
#import <WebKit/WebPDFView.h>

#import <Quartz/Quartz.h>

@implementation WebPDFRepresentation

+ (Class)PDFDocumentClass
{
    static Class PDFDocumentClass = nil;
    if (PDFDocumentClass == nil) {
        PDFDocumentClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFDocument"];
        if (PDFDocumentClass == nil) {
            ERROR("Couldn't find PDFDocument class in PDFKit.framework");
        }
    }
    return PDFDocumentClass;
}

- (void)setDataSource:(WebDataSource *)dataSource;
{
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource;
{
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource;
{
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    WebPDFView *view = (WebPDFView *)[[[dataSource webFrame] frameView] documentView];
    PDFDocument *doc = [[[[self class] PDFDocumentClass] alloc] initWithData:[dataSource data]];
    [[view PDFSubview] setDocument:doc];
    [doc release];
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

#endif // OMIT_TIGER_FEATURES
