/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebPreferences.h>

#import <WebFoundation/WebResourceResponse.h>

@implementation WebTextView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
        [[self textContainer] setWidthTracksTextView:YES];
        [self setAutoresizingMask:NSViewWidthSizable];
        [self setEditable:NO];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(defaultsChanged:)
                                                     name:NSUserDefaultsDidChangeNotification
                                                   object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)setFixedWidthFont
{
    WebPreferences *preferences = [WebPreferences standardPreferences];
    NSFont *font = [NSFont fontWithName:[preferences fixedFontFamily]
        size:[preferences defaultFixedFontSize]];
    [self setFont:font];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
    NSString *string;
    
    // FIXME: This needs to be more efficient for progressively loading documents.
    
    if ([[[dataSource response] contentType] isEqualToString:@"text/rtf"]) {
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0,0) withRTF:[dataSource data]];
    } else {
        [self setRichText:NO];
        [self setFixedWidthFont];
        // FIXME: This needs to use the correct encoding, but the list of names of encodings
        // is currently inside WebCore where we can't share it.
        string = [[NSString alloc] initWithData:[dataSource data] encoding:NSASCIIStringEncoding];
        [self setString:string];
        [string release];
    }
}

- (void)setNeedsLayout:(BOOL)flag
{
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

- (void)defaultsChanged:(NSNotification *)notification
{
    if(![self isRichText]){
        [self setFixedWidthFont];
    }
}

- (BOOL)canTakeFindStringFromSelection
{
    return [self isSelectable] && [self selectedRange].length && ![self hasMarkedText];
}


- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self canTakeFindStringFromSelection]) {
        NSBeep();
        return;
    }
    
    // Note: can't use writeSelectionToPasteboard:type: here, though it seems equivalent, because
    // it doesn't declare the types to the pasteboard and thus doesn't bump the change count
    [self writeSelectionToPasteboard:[NSPasteboard pasteboardWithName:NSFindPboard]
                               types:[NSArray arrayWithObject:NSStringPboardType]];
}


@end
