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

- (IFImageRenderer *)image
{
    return image;
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource isComplete:(BOOL)isComplete
{
    if(isComplete){
        NSData *resourceData = [dataSource data];
        image = [[IFImageRendererFactory alloc] imageRendererWithBytes:[resourceData bytes] 
                    length:[resourceData length]];
    }
    //[image incrementalLoadWithBytes:[data bytes] length:[data length] complete:isComplete];
}

- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource
{

}

@end
