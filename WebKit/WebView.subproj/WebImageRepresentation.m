/*	
    WebImageRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebImageRepresentation.h"

#import <WebCore/WebCoreImageRenderer.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>

@implementation WebImageRepresentation

- init
{
    self = [super init];
    if (self) {
        //image = [[WebImageRenderer alloc] init];
    }
    return self;
}


- (void)dealloc
{
    [image release];
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

- (void)setDataSource:(WebDataSource *)dataSource
{
    URL = [[dataSource URL] retain];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    //[image incrementalLoadWithBytes:[data bytes] length:[data length] complete:isComplete];
}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    NSData *resourceData = [dataSource data];
    image = [[[WebImageRendererFactory sharedFactory] imageRendererWithBytes:[resourceData bytes] 
                length:[resourceData length]] retain];
}

@end
