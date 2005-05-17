/*	
    WebTextView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebTextView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSURLResponse.h>

@interface NSTextView (AppKitSecret)
+ (NSURL *)_URLForString:(NSString *)string;
@end

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
        @"text/x-vcalendar",
        @"text/vcalendar",
        @"text/vcard",		// vCard
        @"text/x-vcard",
        @"text/directory",
        @"text/ldif",           // Netscape Address Book
        @"text/qif",		// Quicken
        @"text/x-qif",
        @"text/x-csv",          // CSV (for Address Book and Microsoft Outlook)
        @"text/x-vcf",          // vCard type used in Sun affinity app
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
                                                     name:WebPreferencesChangedNotification
                                                   object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)finalize
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super finalize];
}

- (float)_textSizeMultiplierFromWebView
{
    // Note that we are not guaranteed to be the subview of a WebView at any given time.
    WebView *webView = [self _web_parentWebView];
    return webView ? [webView textSizeMultiplier] : 1.0;
}

- (WebPreferences *)_preferences
{
    // Handle nil result because we might not be in a WebView at any given time.
    WebPreferences *preferences = [[self _web_parentWebView] preferences];
    if (preferences == nil) {
        preferences = [WebPreferences standardPreferences];
    }
    return preferences;
}


- (void)setFixedWidthFont
{
    WebPreferences *preferences = [self _preferences];
    NSString *families[2];
    families[0] = [preferences fixedFontFamily];
    families[1] = nil;
    NSFont *font = [[WebTextRendererFactory sharedFactory] fontWithFamilies:families 
                                                                     traits:0 
                                                                       size:[preferences defaultFixedFontSize]*_textSizeMultiplier];
    if (font) {
        [self setFont:font];
    }
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
    [self sizeToFit];
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

- (NSDictionary *)_elementAtWindowPoint:(NSPoint)windowPoint
{
    WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
    ASSERT(frame);
    
    NSPoint screenPoint = [[self window] convertBaseToScreen:windowPoint];
    BOOL isPointSelected = NSLocationInRange([self characterIndexForPoint:screenPoint], [self selectedRange]);
    return [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:isPointSelected], WebElementIsSelectedKey,
        frame, WebElementFrameKey, nil];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self _elementAtWindowPoint:[self convertPoint:point toView:nil]];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{    
    WebView *webView = [self _web_parentWebView];
    ASSERT(webView);
    return [webView _menuForElement:[self _elementAtWindowPoint:[event locationInWindow]]];
}

- (NSArray *)pasteboardTypesForSelection
{
    return [self writablePasteboardTypes];
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self writeSelectionToPasteboard:pasteboard types:types];
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

- (void)clickedOnLink:(id)link atIndex:(unsigned)charIndex
{
    NSURL *URL = nil;
    if ([link isKindOfClass:[NSURL class]]) {
        URL = (NSURL *)link;
    } else if ([link isKindOfClass:[NSString class]]) {
        URL = [[self class] _URLForString:(NSString *)link];
    }
    if (URL != nil) {    
        // Call the bridge because this is where our security checks are made.
        WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
        [[frame _bridge] loadURL:URL 
                        referrer:[[[[frame dataSource] request] URL] _web_originalDataAsString]
                          reload:NO
                     userGesture:YES       
                          target:nil
                 triggeringEvent:[[self window] currentEvent]
                            form:nil 
                      formValues:nil];
    }
}

#pragma mark PRINTING

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));
    [[self _web_parentWebView] _drawHeaderAndFooter];
}

- (BOOL)knowsPageRange:(NSRangePointer)range {
    // Waiting for beginDocument to adjust the printing margins is too late.
    [[self _web_parentWebView] _adjustPrintingMarginsForHeaderAndFooter];
    return [super knowsPageRange:range];
}

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

@end

@implementation WebTextView (TextSizing)

- (void)_web_textSizeMultiplierChanged
{
    [self _updateTextSizeMultiplier];
}

@end
