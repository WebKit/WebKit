/*	
    IFTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFTextView.h>

#import <WebKit/IFWebDataSource.h>

@implementation IFTextView

- (id)initWithFrame:(NSRect)frame
{
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
}

- (void)provisionalDataSourceCommitted:(IFWebDataSource *)dataSource
{
}

- (void)dataSourceUpdated:(IFWebDataSource *)dataSource
{
    NSString *string;
    
    // FIXME: This needs to be more efficient for progressively loading documents.
    
    if ([[dataSource contentType] isEqualToString:@"text/rtf"]) {
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0,0) withRTF:[dataSource data]];
    } else {
        [self setRichText:NO];
        
        // FIXME: This needs to use the correct encoding, but the list of names of encodings
        // is currently inside WebCore where we can't share it.
        string = [[NSString alloc] initWithData:[dataSource data] encoding:NSASCIIStringEncoding];
        [self setString:string];
        [string release];
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
