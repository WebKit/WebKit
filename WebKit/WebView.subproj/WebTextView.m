/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/NSURLResponse.h>

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>

@implementation WebTextView

+ (NSArray *)unsupportedTextMIMETypes
{
    return [NSArray arrayWithObjects:
        @"text/calendar",	// iCal
        @"text/x-calendar",
        @"text/vcard",		// vCard
        @"text/x-vcard",
        @"text/qif",		// Quicken
        @"text/x-qif",
        nil];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
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
    
    if ([[[dataSource response] MIMEType] isEqualToString:@"text/rtf"]) {
        [self setRichText:YES];
        [self replaceCharactersInRange:NSMakeRange(0, [[self string] length]) withRTF:[dataSource data]];
    } else {
        [self setRichText:NO];
        [self setFixedWidthFont];
        [self setString:[dataSource _stringWithData:[dataSource data]]];
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

- (NSString *)string
{
    return [super string];
}

- (NSAttributedString *)attributedString
{
    return [self attributedSubstringFromRange:NSMakeRange(0, [[self string] length])];
}

- (NSString *)selectedString
{
    return [[self string] substringWithRange:[self selectedRange]];
}

- (NSAttributedString *)selectedAttributedString
{
    return [self attributedSubstringFromRange:[self selectedRange]];
}

- (void)selectAll
{
    [self setSelectedRange:NSMakeRange(0, [[self string] length])];
}

- (void)deselectAll
{
    [self setSelectedRange:NSMakeRange(0,0)];
}

- (void)keyDown:(NSEvent *)event
{
    [[self nextResponder] keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    [[self nextResponder] keyUp:event];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    // Calling super causes unselected clicked text to be selected.
    [super menuForEvent:theEvent];
    
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    WebView *controller = [webFrameView _controller];
    WebFrame *frame = [webFrameView webFrame];

    ASSERT(frame);
    ASSERT(controller);

    BOOL hasSelection = ([self selectedRange].location != NSNotFound && [self selectedRange].length > 0);
    NSDictionary *element = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:hasSelection], WebElementIsSelectedTextKey,
        frame, WebElementFrameKey, nil];

    return [controller _menuForElement:element];
}

@end
