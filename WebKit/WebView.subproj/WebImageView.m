/*	
    IFImageView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFImageView.h"
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFImageRepresentation.h>
#import <WebKit/IFNSViewExtras.h>
#import <WebKit/IFWebDataSource.h>

@implementation IFImageView

- (id)initWithFrame:(NSRect)frame {
    
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
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)drawRect:(NSRect)rect {
    IFImageRenderer *image;
    
    image = [representation image];
    if(image){
        [image beginAnimationInView:self inRect:[self frame] fromRect:[self frame]];
    }
}

- (void)removeFromSuperview
{
    NSView *scrollView = [self _IF_superviewWithName:@"IFDynamicScrollBarsView"];
    [(NSScrollView *)scrollView setDrawsBackground:NO];
    [super removeFromSuperview];
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

- (void)setFrame:(NSRect)frameRect
{
    [super setFrame:frameRect];
}

- (void)layout
{
    IFImageRenderer *image = [representation image];
    
    if(image){
        NSView *scrollView = [self _IF_superviewWithName:@"IFDynamicScrollBarsView"];
        NSSize imageSize = [image size];

        [self setFrameSize:imageSize];
        [image setFlipped:YES];
        [(NSScrollView *)scrollView setDrawsBackground:YES];
        [(NSScrollView *)scrollView setBackgroundColor:[NSColor whiteColor]];
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
