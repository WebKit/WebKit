/*	
    IFTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFTextView.h"
#import <WebKit/IFWebDataSource.h>

@implementation IFTextView


- (id)initWithFrame:(NSRect)frame {
    
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
        [[self textContainer] setWidthTracksTextView:YES];
        [self setAutoresizingMask:NSViewWidthSizable];
        [self setRichText:YES];
    }
    return self;
}

- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource
{

}

- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource
{

}

- (void)dataSourceUpdated:(IFWebDataSource *)dataSource
{
    NSString *theString = [[NSString alloc] initWithData:[dataSource data] encoding:NSASCIIStringEncoding];
    [self setString:theString];
    [theString release];
}

- (void)layout
{
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

- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{

}

@end
