/*	
    WebPDFView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guaranateed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3

#import <WebKit/WebDataSource.h>
#import <WebKit/WebPDFView.h>

@implementation WebPDFView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable];
    }
    return self;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
}

- (void)layout
{
    [self setFrame:[[self superview] frame]];
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

@end

#endif //MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
