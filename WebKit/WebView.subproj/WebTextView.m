/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebFoundation/WebResourceResponse.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebPreferences.h>

@implementation WebTextView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        canDragFrom = YES;
        canDragTo = YES;
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
    // FIXME: This needs to be more efficient for progressively loading documents.
    
    if ([[[dataSource response] contentType] isEqualToString:@"text/rtf"]) {
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0, [[self string] length]) withRTF:[dataSource data]];
    } else {
        [self setRichText:NO];
        [self setFixedWidthFont];
        [self setString:[dataSource stringWithData:[dataSource data]]];
    }
    
    
}

- (void)viewDidMoveToSuperview
{
    NSView *superview = [self superview];
    if (superview) {
        [self setFrameSize:NSMakeSize([superview frame].size.width, [self frame].size.height)];
    }
}

- (void)setNeedsLayout:(BOOL)flag
{
}

- (void)layout
{
}

- (void)setAcceptsDrags:(BOOL)flag
{
    canDragFrom = flag;
}

- (BOOL)acceptsDrags
{
    return canDragFrom;
}

- (void)setAcceptsDrops:(BOOL)flag
{
    canDragTo = flag;
}

- (BOOL)acceptsDrops
{
    return canDragTo;
}

- (void)defaultsChanged:(NSNotification *)notification
{
    if (![self isRichText]) {
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

- (BOOL)supportsTextEncoding
{
    return YES;
}

// Pass key events to next responder so command-arrows work.
- (void)keyDown:(NSEvent *)event
{
    [[self nextResponder] keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    [[self nextResponder] keyUp:event];
}

@end
