/*	
    IFImageView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFImageView.h>

#import <WebKit/IFDocument.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFImageRepresentation.h>
#import <WebKit/IFWebDataSource.h>

@implementation IFImageView

- (id)initWithFrame:(NSRect)frame
{    
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
    }
    return self;
}

- (void)dealloc
{
    [[representation image] stopAnimation];
    [representation release];
    
    [super dealloc];
}


- (BOOL)isFlipped 
{
    return YES;
}


- (void)drawRect:(NSRect)rect
{
    [[representation image] beginAnimationInRect:[self frame] fromRect:[self frame]];
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
    IFImageRenderer *image = [representation image];
    
    if(image){
        [self setFrameSize:[image size]];
        [image setFlipped:YES];
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
