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
        [self setEditable:NO];
    }
    return self;
}

- (void)provisionalDataSourceChanged:(IFWebDataSource *)dataSource
{
    if([[dataSource contentType] isEqualToString:@"text/rtf"])
        isRTF = YES;
    else
        isRTF = NO;
}

- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource
{

}

- (void)dataSourceUpdated:(IFWebDataSource *)dataSource
{
    NSString *theString;
    
    //FIXME: This needs to be more efficient
    
    if(isRTF){
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0,0) withRTF:[dataSource data]];
    }else{
        [self setRichText:NO];
        
        // set correct encoding
        theString = [[NSString alloc] initWithData:[dataSource data] encoding:NSASCIIStringEncoding];
        [self setString:theString];
        [theString release];
    }
}

- (void)layout
{
    NSRect superFrame = [[self superview] frame];
    NSRect frame = [self frame];
    
    [self setFrame:NSMakeRect(frame.origin.x, frame.origin.y, superFrame.size.width, frame.size.height)];
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
