/*	
    WebPDFRepresentation.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#ifndef OMIT_TIGER_FEATURES

#import <Quartz/Quartz.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPDFRepresentation.h>
#import <WebKit/WebPDFView.h>

@implementation WebPDFRepresentation

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
    PDFDocument *doc = [[PDFDocument alloc] initWithData:[dataSource data]];
    [view setDocument:doc];
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
