/*	
    IFImageRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFImageRepresentation.h"
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFImageRendererFactory.h>

@implementation IFImageRepresentation

- init
{
    self = [super init];
    if (self) {
        //image = [[IFImageRenderer alloc] init];
    }
    return self;
}


- (void)dealloc
{
    [image release];
}

- (IFImageRenderer *)image
{
    return image;
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource
{
    //[image incrementalLoadWithBytes:[data bytes] length:[data length] complete:isComplete];
}

- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(IFWebDataSource *)dataSource
{
    NSData *resourceData = [dataSource data];
    image = [[[IFImageRendererFactory sharedFactory] imageRendererWithBytes:[resourceData bytes] 
                length:[resourceData length]] retain];
}

@end
