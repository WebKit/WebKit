/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebTextView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebTextRepresentation.h>

#import <WebCore/WebCoreTextDecoder.h>

#import <Foundation/NSURLResponse.h>

@interface NSTextView (AppKitSecret)
+ (NSURL *)_URLForString:(NSString *)string;
@end

@interface WebTextView (ForwardDeclarations)
- (void)_updateTextSizeMultiplier;
@end

@interface WebTextView (TextSizing) <_WebDocumentTextSizing>
@end

@implementation WebTextView

+ (NSArray *)supportedMIMETypes
{
    return [WebTextRepresentation supportedMIMETypes];
}

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
    [_decoder release];
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
    NSFont *font = [[WebTextRendererFactory sharedFactory] cachedFontFromFamily:[preferences fixedFontFamily]
        traits:0 size:[preferences defaultFixedFontSize] * _textSizeMultiplier];
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
- (void)appendReceivedData:(NSData *)data fromDataSource:(WebDataSource *)dataSource
{
    if ([self isRichText]) {
        // FIXME: We should try to progressively load RTF.
        [self replaceCharactersInRange:NSMakeRange(0, [[self string] length])
                               withRTF:[dataSource data]];
        if (_textSizeMultiplier != 1.0) {
            [self _adjustRichTextFontSizeByRatio:_textSizeMultiplier];
        }
    } else {
        if (!_decoder) {
            NSString *textEncodingName = [dataSource textEncodingName];
            if (textEncodingName == nil)
                textEncodingName = [[self _preferences] defaultTextEncodingName];

            _decoder = [[WebCoreTextDecoder alloc] initWithEncodingName:textEncodingName];
        }
        
        [self replaceCharactersInRange:NSMakeRange([[self string] length], 0)
                            withString:[_decoder decodeData:data]];
    }
}

- (void)flushReceivedData
{
    if ([self isRichText]) {
        // FIXME: We should try to progressively load RTF.
    } else {
        if (_decoder)
            [self replaceCharactersInRange:NSMakeRange([[self string] length], 0)
                            withString:[_decoder flush]];
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

- (void)keyDown:(NSEvent *)event
{
    [[self nextResponder] keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    [[self nextResponder] keyUp:event];
}

- (WebFrame *)_webFrame
{
    return [[self _web_parentWebFrameView] webFrame];
}

- (NSDictionary *)_elementAtWindowPoint:(NSPoint)windowPoint
{
    WebFrame *frame = [self _webFrame];
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

- (BOOL)dragSelectionWithEvent:(NSEvent *)event offset:(NSSize)mouseOffset slideBack:(BOOL)slideBack
{
    // Mark webview as initiating the drag so dropping the text back on this webview never tries to navigate.
    WebView *webView = [self _web_parentWebView];
    [webView _setInitiatedDrag:YES];
    
    // The last reference can be lost during the drag if it causes a navigation (e.g. dropping in Safari location
    // field). This can mess up the dragging machinery (radar 4118126), so retain here and release at end of drag.
    [self retain];

    BOOL result = [super dragSelectionWithEvent:event offset:mouseOffset slideBack:slideBack];
    
    // When the call to super finishes, the drag is either aborted or completed. We must clean up in either case.
    [webView _setInitiatedDrag:NO];
    [self release];
    
    return result;
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{    
    WebView *webView = [self _web_parentWebView];
    ASSERT(webView);
    return [webView _menuForElement:[self _elementAtWindowPoint:[event locationInWindow]] defaultItems:nil];
}

- (BOOL)becomeFirstResponder
{
    BOOL result = [super becomeFirstResponder];
    if (result)
        [[self _webFrame] _clearSelectionInOtherFrames];
    return result;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && ![[self _web_parentWebView] maintainsInactiveSelection])
        [self deselectAll];
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
    if (URL != nil)
        [[self _webFrame] _safeLoadURL:URL];
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

- (IBAction)_makeTextSmaller:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextLarger:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextStandardSize:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (BOOL)_tracksCommonSizeFactor
{
    return YES;
}

// never sent because we track the common size factor
- (BOOL)_canMakeTextSmaller          {   ASSERT_NOT_REACHED(); return NO;    }
- (BOOL)_canMakeTextLarger           {   ASSERT_NOT_REACHED(); return NO;    }
- (BOOL)_canMakeTextStandardSize     {   ASSERT_NOT_REACHED(); return NO;    }

@end
