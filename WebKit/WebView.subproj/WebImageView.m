/*	
    WebImageView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageView.h>

#import <WebKit/WebDocument.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebDataSource.h>

@implementation WebImageView

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

- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource
{
}

- (void)provisionalDataSourceCommitted:(WebDataSource *)dataSource
{
    representation = [[dataSource representation] retain];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)layout
{
    WebImageRenderer *image = [representation image];
    if (image) {
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

- (void)viewDidMoveToWindow
{
    if (![self window])
        [[representation image] stopAnimation];
    [super viewDidMoveToWindow];
}

@end
