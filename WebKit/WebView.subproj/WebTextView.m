/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSURLResponse.h>

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>

@interface WebTextView (ForwardDeclarations)
- (void)_updateTextSizeMultiplier;
@end

@interface WebTextView (TextSizing) <_web_WebDocumentTextSizing>
@end

@implementation WebTextView

+ (NSArray *)unsupportedTextMIMETypes
{
    return [NSArray arrayWithObjects:
        @"text/calendar",	// iCal
        @"text/x-calendar",
        @"text/vcard",		// vCard
        @"text/x-vcard",
        @"text/directory",
        @"text/qif",		// Quicken
        @"text/x-qif",
        nil];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _textSizeMultiplier = 1.0;
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

- (float)_textSizeMultiplierFromWebView
{
    // Note that we are not guaranteed to be the subview of a webView at any given time.
    WebView *webView = [[self _web_parentWebFrameView] _webView];
    return webView ? [webView textSizeMultiplier] : 1.0;
}

- (void)setFixedWidthFont
{
    WebPreferences *preferences = [WebPreferences standardPreferences];
    NSFont *font = [NSFont fontWithName:[preferences fixedFontFamily]
                                   size:[preferences defaultFixedFontSize]*_textSizeMultiplier];
    [self setFont:font];
}

// This method was borrowed from Mail and changed to use ratios rather than deltas.
// Also, I removed the isEditable clause since RTF displayed in WebKit is never editable.
- (void)_adjustRichTextFontSizeByRatio:(float)ratio 
{
    NSTextStorage *storage = [self textStorage];
    NSRange remainingRange = NSMakeRange(0, [storage length]);
    
    while (remainingRange.length > 0) {
        NSRange effectiveRange;
        NSFont *font = [storage attribute:NSFontAttributeName atIndex:remainingRange.location longestEffectiveRange:&effectiveRange inRange:remainingRange];
        
        if (font) {
            font = [[NSFontManager sharedFontManager] convertFont:font toSize:[font pointSize]*ratio];
            [storage addAttribute:NSFontAttributeName value:font range:effectiveRange];
        }
        if (NSMaxRange(effectiveRange) < NSMaxRange(remainingRange)) {
            remainingRange.length = NSMaxRange(remainingRange) - NSMaxRange(effectiveRange);
            remainingRange.location = NSMaxRange(effectiveRange);
        } else {
            break;
        }
    }
}

- (void)_updateTextSizeMultiplier
{
    float newMultiplier = [self _textSizeMultiplierFromWebView];
    if (newMultiplier == _textSizeMultiplier) {
        return;
    }
    
    float oldMultiplier = _textSizeMultiplier;
    _textSizeMultiplier = newMultiplier;
    
    if ([self isRichText]) {
        [self _adjustRichTextFontSizeByRatio:newMultiplier/oldMultiplier];
    } else {
        [self setFixedWidthFont];
    }
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    [self setRichText:[[[dataSource response] MIMEType] isEqualToString:@"text/rtf"]];
    
    float oldMultiplier = _textSizeMultiplier;
    [self _updateTextSizeMultiplier];
    // If the multiplier didn't change, we still need to update the fixed-width font.
    // If the multiplier did change, this was already handled.
    if (_textSizeMultiplier == oldMultiplier && ![self isRichText]) {
        [self setFixedWidthFont];
    }

}

// We handle incoming data here rather than in dataSourceUpdated because we
// need to distinguish the last hunk of received data from the whole glob
// of data received so far. This is a bad design in that it requires
// WebTextRepresentation to know that it's view is a WebTextView, but this
// bad design already existed.
- (void)appendReceivedData:(NSData *)data fromDataSource:(WebDataSource *)dataSource;
{
    if ([self isRichText]) {
        // FIXME: We should try to progressively load RTF.
        [self replaceCharactersInRange:NSMakeRange(0, [[self string] length])
                               withRTF:[dataSource data]];
        if (_textSizeMultiplier != 1.0) {
            [self _adjustRichTextFontSizeByRatio:_textSizeMultiplier];
        }
    } else {
        [self replaceCharactersInRange:NSMakeRange([[self string] length], 0)
                            withString:[dataSource _stringWithData:data]];
    }
}


- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
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

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{

}

- (void)viewDidMoveToHostWindow
{

}

- (void)defaultsChanged:(NSNotification *)notification
{
    // We use the default fixed-width font, but rich text
    // pages specify their own fonts
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
    WebView *webView = [webFrameView _webView];
    WebFrame *frame = [webFrameView webFrame];

    ASSERT(frame);
    ASSERT(webView);

    BOOL hasSelection = ([self selectedRange].location != NSNotFound && [self selectedRange].length > 0);
    NSDictionary *element = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:hasSelection], WebElementIsSelectedKey,
        frame, WebElementFrameKey, nil];

    return [webView _menuForElement:element];
}

// This approach could be relaxed when dealing with 3228554
- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [self setSelectedRange:NSMakeRange(0,0)];
    }
    return resign;
}

@end

@implementation WebTextView (TextSizing)

- (void)_web_textSizeMultiplierChanged
{
    [self _updateTextSizeMultiplier];
}

@end
