/*	
    WebImageRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebImageRepresentation.h"

#import <WebKit/WebDataSource.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebLocalizableStrings.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <Foundation/NSURLRequest.h>

@implementation WebImageRepresentation

- (void)dealloc
{
    [filename release];
    [image release];
    [data release];
    [URL release];
    [super dealloc];
}

- (WebImageRenderer *)image
{
    return image;
}

- (NSURL *)URL
{
    return URL;
}

- (BOOL)doneLoading
{
    return data != nil;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    URL = [[[dataSource request] URL] retain];
    filename = [[[dataSource response] suggestedFilename] retain];
    image = [[[WebImageRendererFactory sharedFactory] imageRendererWithMIMEType:[[dataSource response] MIMEType]] retain];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:NO];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    data = [[dataSource data] retain];
    if ([data length] > 0) {
        [image incrementalLoadWithBytes:[data bytes] length:[data length] complete:YES];
    }
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    data = [[dataSource data] retain];
    [image incrementalLoadWithBytes:[data bytes] length:[data length] complete:YES];
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
    NSSize size = [image size];
    if (!NSEqualSizes(size, NSZeroSize)) {
        return [NSString stringWithFormat:UI_STRING("%@ %.0fx%.0f pixels", "window title for a standalone image"), filename, size.width, size.height];
    }
    return filename;
}

- (NSData *)data
{
    return data;
}

- (NSString *)filename
{
    return filename;
}

- (NSFileWrapper *)fileWrapper
{
    NSFileWrapper *wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:data];
    [wrapper setPreferredFilename:filename];
    return [wrapper autorelease]; 
}

@end
