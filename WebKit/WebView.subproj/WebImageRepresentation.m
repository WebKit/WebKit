/*	
    WebImageRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebImageRepresentation.h"

#import <WebKit/WebArchive.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebResource.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <Foundation/NSURLRequest.h>

@implementation WebImageRepresentation

- (void)dealloc
{
    [image release];
    [super dealloc];
}

- (WebImageRenderer *)image
{
    return image;
}

- (NSURL *)URL
{
    return [[dataSource request] URL];
}

- (BOOL)doneLoading
{
    return doneLoading;
}

- (void)setDataSource:(WebDataSource *)theDataSource
{
    dataSource = theDataSource;
    image = [[[WebImageRendererFactory sharedFactory] imageRendererWithMIMEType:[[dataSource response] MIMEType]] retain];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:NO callback:0];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    if ([allData length] > 0) {
        [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES callback:0];
    }
    doneLoading = YES;
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)theDataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES callback:0];
    doneLoading = YES;
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
    NSString *filename = [self filename];
    NSSize size = [image size];
    if (!NSEqualSizes(size, NSZeroSize)) {
        return [NSString stringWithFormat:UI_STRING("%@ %.0f×%.0f pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filename, size.width, size.height];
    }
    return filename;
}

- (NSData *)data
{
    return [dataSource data];
}

- (NSString *)filename
{
    return [[dataSource response] suggestedFilename];
}

- (WebArchive *)archive
{
    return [dataSource webArchive];
}

@end
