/*	
    IFImageView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFImageView.h"
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFImageRepresentation.h>

@implementation IFImageView

- (id)initWithFrame:(NSRect)frame {
    
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
        didSetFrame = NO;
    }
    return self;
}

- (void)dealloc
{
    [[representation image] stopAnimation];
    [representation release];
}


- (void)drawRect:(NSRect)rect {
    IFImageRenderer *image;
    NSSize imageSize;
    
    image = [representation image];
    if([representation image]){
        
        imageSize = [image size];
        [image beginAnimationInView:self inRect:rect fromRect:NSMakeRect(0, 0, imageSize.width, imageSize.height)];
    }
}

- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource
{

}

- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource
{
    representation = [[dataSource representation] retain];
}

- (void)dataSourceUpdated:(IFWebDataSource *)dataSource
{

}

- (void)layout
{
    if([representation image] && !didSetFrame){
        IFImageRenderer *image = [representation image];
        NSSize imageSize = [image size];
        NSRect frame = [self frame];
        
        [self setFrame:NSMakeRect(frame.origin.x, frame.origin.x, imageSize.width, imageSize.height)];
        didSetFrame = YES;
    }
}

- (void)setCanDragFrom: (BOOL)flag
{
    canDragFrom = flag;
}

- (BOOL)canDragFrom
{
    return canDragFrom;
}

- (void)setCanDragTo: (BOOL)flag
{
    canDragTo = flag;
}

- (BOOL)canDragTo
{
    return canDragTo;
}


@end
