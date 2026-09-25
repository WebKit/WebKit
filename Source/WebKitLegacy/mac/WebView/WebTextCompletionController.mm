/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebTextCompletionController.h"

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebFrameInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebView.h"
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/SimpleRange.h>

@interface NSWindow (WebNSWindowDetails)
- (void)_setForceActiveControls:(BOOL)flag;
@end


// This class handles the complete: operation.
// It counts on its host view to call endRevertingChange: whenever the current completion needs to be aborted.

// The class is in one of two modes: Popup window showing, or not.
// It is shown when a completion yields more than one match.
// If a completion yields one or zero matches, it is not shown, and there is no state carried across to the next completion.

@implementation WebTextCompletionController

- (id)initWithWebView:(WebView *)view HTMLView:(WebHTMLView *)htmlView
{
    self = [super init];
    if (!self)
        return nil;
    _view = view;
    _htmlView = htmlView;
    return self;
}

- (void)dealloc
{
    // Retaining the member just to release it would be pointless.
    SUPPRESS_UNRETAINED_ARG [_popupWindow release];
    SUPPRESS_UNRETAINED_ARG [_completions release];
    SUPPRESS_UNRETAINED_ARG [_originalString release];
    
    [super dealloc];
}

- (void)_insertMatch:(NSString *)match
{
    // FIXME: 3769654 - We should preserve case of string being inserted, even in prefix (but then also be
    // able to revert that).  Mimic NSText.
    RetainPtr frame = [protect(_htmlView) _frame];
    NSString *newText = [match substringFromIndex:prefixLength];
    [frame _replaceSelectionWithText:newText selectReplacement:YES smartReplace:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_buildUI
{
    NSRect scrollFrame = NSMakeRect(0, 0, 100, 100);
    NSRect tableFrame = NSZeroRect;
    tableFrame.size = [NSScrollView contentSizeForFrameSize:scrollFrame.size horizontalScrollerClass:nil verticalScrollerClass:[NSScroller class] borderType:NSNoBorder
        controlSize:NSControlSizeRegular scrollerStyle:[NSScroller preferredScrollerStyle]];
    auto column = adoptNS([[NSTableColumn alloc] init]);
    [column setWidth:tableFrame.size.width];
    [column setEditable:NO];
    
    _tableView = [[NSTableView alloc] initWithFrame:tableFrame];
    RetainPtr tableView = _tableView;
    [tableView setAutoresizingMask:NSViewWidthSizable];
    [tableView addTableColumn:column.get()];
    [tableView setGridStyleMask:NSTableViewGridNone];
    [tableView setCornerView:nil];
    [tableView setHeaderView:nil];
    [tableView setColumnAutoresizingStyle:NSTableViewUniformColumnAutoresizingStyle];
    [tableView setDelegate:self];
    [tableView setDataSource:self];
    [tableView setTarget:self];
    [tableView setDoubleAction:@selector(tableAction:)];
    
    auto scrollView = adoptNS([[NSScrollView alloc] initWithFrame:scrollFrame]);
    [scrollView setBorderType:NSNoBorder];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollView setDocumentView:tableView];
    [tableView release];
    
    _popupWindow = [[NSWindow alloc] initWithContentRect:scrollFrame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    RetainPtr popupWindow = _popupWindow;
    [popupWindow setAlphaValue:0.88f];
    [popupWindow setContentView:scrollView.get()];
    [popupWindow setHasShadow:YES];
    [popupWindow _setForceActiveControls:YES];
    [popupWindow setReleasedWhenClosed:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_placePopupWindow:(NSPoint)topLeft
{
    RetainPtr completions = _completions;
    NSUInteger numberToShow = [completions count];
    if (numberToShow > 20)
        numberToShow = 20;

    NSRect windowFrame;
    NSPoint wordStart = topLeft;
    RetainPtr view = _view;
    windowFrame.origin = [[view window] convertPointToScreen:[protect(_htmlView) convertPoint:wordStart toView:nil]];
    RetainPtr tableView = _tableView;
    windowFrame.size.height = numberToShow * [tableView rowHeight] + (numberToShow + 1) * [tableView intercellSpacing].height;
    windowFrame.origin.y -= windowFrame.size.height;
    NSDictionary *attributes = @{ NSFontAttributeName: [NSFont systemFontOfSize:12.0f] };
    CGFloat maxWidth = 0;
    int maxIndex = -1;
    for (NSUInteger i = 0; i < numberToShow; i++) {
        float width = ceilf([[completions objectAtIndex:i] sizeWithAttributes:attributes].width);
        if (width > maxWidth) {
            maxWidth = width;
            maxIndex = i;
        }
    }
    windowFrame.size.width = 100;
    if (maxIndex >= 0) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        maxWidth = ceilf([NSScrollView frameSizeForContentSize:NSMakeSize(maxWidth, 100.0f) hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder].width);
ALLOW_DEPRECATED_DECLARATIONS_END
        maxWidth = ceilf([NSWindow frameRectForContentRect:NSMakeRect(0.0f, 0.0f, maxWidth, 100.0f) styleMask:NSWindowStyleMaskBorderless].size.width);
        maxWidth += 5.0f;
        windowFrame.size.width = std::max(maxWidth, windowFrame.size.width);
    }
    RetainPtr popupWindow = _popupWindow;
    [popupWindow setFrame:windowFrame display:NO];
    
    [tableView reloadData];
    [tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
    [tableView scrollRowToVisible:0];
    [self _reflectSelection];
    [popupWindow setLevel:NSPopUpMenuWindowLevel];
    [popupWindow orderFront:nil];
    [[view window] addChildWindow:popupWindow ordered:NSWindowAbove];
}

- (void)doCompletion
{
    if (!_popupWindow) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker) {
            LOG_ERROR("No NSSpellChecker");
            return;
        }

        // Get preceeding word stem
        RetainPtr frame = [protect(_htmlView) _frame];
        RetainPtr selection = kit(core(frame.get())->selection().selection().toNormalizedRange());
        DOMRange *wholeWord = [frame _rangeByAlteringCurrentSelection:WebCore::FrameSelection::Alteration::Extend
            direction:WebCore::SelectionDirection::Backward granularity:WebCore::TextGranularity::WordGranularity];
        DOMRange *prefix = [wholeWord cloneRange];
        [prefix setEnd:[selection.get() startContainer] offset:[selection.get() startOffset]];

        // Reject some NOP cases
        if ([prefix collapsed]) {
            NSBeep();
            return;
        }
        NSString *prefixStr = [frame _stringForRange:prefix];
        NSString *trimmedPrefix = [prefixStr stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (![trimmedPrefix length]) {
            NSBeep();
            return;
        }
        prefixLength = [prefixStr length];

        // Lookup matches
        SUPPRESS_UNRETAINED_ARG [_completions release];
        _completions = [checker completionsForPartialWordRange:NSMakeRange(0, [prefixStr length]) inString:prefixStr language:nil inSpellDocumentWithTag:[protect(_view) spellCheckerDocumentTag]];
        RetainPtr completions = _completions;
        [completions retain];
    
        if (!completions || ![completions count]) {
            NSBeep();
        } else if ([completions count] == 1) {
            [self _insertMatch:[completions objectAtIndex:0]];
        } else {
            ASSERT(!_originalString);       // this should only be set IFF we have a popup window
            _originalString = [[frame _stringForRange:selection.get()] retain];
            [self _buildUI];
            NSRect wordRect = [frame _caretRectAtPosition:WebCore::Position(core([wholeWord startContainer]), [wholeWord startOffset], WebCore::Position::PositionIsOffsetInAnchor) affinity:NSSelectionAffinityDownstream];
            // +1 to be under the word, not the caret
            // FIXME - 3769652 - Wrong positioning for right to left languages.  We should line up the upper
            // right corner with the caret instead of upper left, and the +1 would be a -1.
            NSPoint wordLowerLeft = { NSMinX(wordRect)+1, NSMaxY(wordRect) };
            [self _placePopupWindow:wordLowerLeft];
        }
    } else {
        [self endRevertingChange:YES moveLeft:NO];
    }
}

- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft
{
    RetainPtr originalString = _originalString;
    RetainPtr htmlView = _htmlView;
    if (_popupWindow) {
        // tear down UI
        RetainPtr popupWindow = _popupWindow;
        [[protect(_view) window] removeChildWindow:popupWindow];
        [popupWindow orderOut:self];
        // Must autorelease because event tracking code may be on the stack touching UI
        [popupWindow autorelease];
        _popupWindow = nil;

        if (revertChange) {
            WebFrame *frame = [htmlView _frame];
            [frame _replaceSelectionWithText:originalString selectReplacement:YES smartReplace:NO];
        } else if ([htmlView _hasSelection]) {
            if (goLeft)
                [htmlView moveBackward:nil];
            else
                [htmlView moveForward:nil];
        }
        [originalString release];
        _originalString = nil;
    }
    // else there is no state to abort if the window was not up
}

- (BOOL)popupWindowIsOpen
{
    return _popupWindow != nil;
}

// WebHTMLView gives us a crack at key events it sees. Return whether we consumed the event.
// The features for the various keys mimic NSTextView.
- (BOOL)filterKeyDown:(NSEvent *)event
{
    RetainPtr tableView = _tableView;
    if (!_popupWindow)
        return NO;
    NSString *string = [event charactersIgnoringModifiers];
    if (![string length])
        return NO;
    unichar c = [string characterAtIndex:0];
    if (c == NSUpArrowFunctionKey) {
        int selectedRow = [tableView selectedRow];
        if (0 < selectedRow) {
            [tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow - 1] byExtendingSelection:NO];
            [tableView scrollRowToVisible:selectedRow - 1];
        }
        return YES;
    }
    if (c == NSDownArrowFunctionKey) {
        int selectedRow = [tableView selectedRow];
        if (selectedRow < (int)[protect(_completions) count] - 1) {
            [tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedRow + 1] byExtendingSelection:NO];
            [tableView scrollRowToVisible:selectedRow + 1];
        }
        return YES;
    }
    if (c == NSRightArrowFunctionKey || c == '\n' || c == '\r' || c == '\t') {
        // FIXME: What about backtab?
        [self endRevertingChange:NO moveLeft:NO];
        return YES;
    }
    if (c == NSLeftArrowFunctionKey) {
        [self endRevertingChange:NO moveLeft:YES];
        return YES;
    }
    if (c == 0x1B || c == NSF5FunctionKey) {
        // FIXME: F5?
        [self endRevertingChange:YES moveLeft:NO];
        return YES;
    }
    if (c == ' ' || (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7D)) {
        // FIXME: Is the above list of keys really definitive?
        // Originally this code called ispunct; aren't there other punctuation keys on international keyboards?
        [self endRevertingChange:NO moveLeft:NO];
        return NO; // let the char get inserted
    }
    return NO;
}

- (void)_reflectSelection
{
    int selectedRow = [protect(_tableView) selectedRow];
    ASSERT(selectedRow >= 0);
    ASSERT(selectedRow < (int)[_completions count]);
    [self _insertMatch:[protect(_completions) objectAtIndex:selectedRow]];
}

- (void)tableAction:(id)sender
{
    [self _reflectSelection];
    [self endRevertingChange:NO moveLeft:NO];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [protect(_completions) count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    return [protect(_completions) objectAtIndex:row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [self _reflectSelection];
}

@end
