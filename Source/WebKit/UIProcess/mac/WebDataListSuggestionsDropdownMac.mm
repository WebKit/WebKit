/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebDataListSuggestionsDropdownMac.h"

#if ENABLE(DATALIST_ELEMENT) && USE(APPKIT)

#import "WebPageProxy.h"
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/NSColorSPI.h>

static const CGFloat dropdownTopMargin = 2;
static const CGFloat dropdownRowHeight = 20;
static const CGFloat dropdownShadowHeight = 5;
static const CGFloat dropdownShadowBlurRadius = 3;
static const size_t dropdownMaxSuggestions = 6;
static NSString * const suggestionCellReuseIdentifier = @"WKDataListSuggestionCell";

@interface WKDataListSuggestionWindow : NSWindow
@end

@interface WKDataListSuggestionCell : NSView {
    RetainPtr<NSTextField> _textField;
    BOOL _mouseIsOver;
    BOOL _active;
}

@property (nonatomic, assign) BOOL active;

- (void)setText:(NSString *)text;
@end

@interface WKDataListSuggestionTable : NSTableView {
    RetainPtr<NSScrollView> _enclosingScrollView;
    Optional<size_t> _activeRow;
}

- (id)initWithElementRect:(const WebCore::IntRect&)rect;
- (void)setVisibleRect:(NSRect)rect;
- (void)setActiveRow:(size_t)row;
- (Optional<size_t>)currentActiveRow;
- (void)reload;
@end

@interface WKDataListSuggestionsView : NSObject<NSTableViewDataSource, NSTableViewDelegate> {
@private
    RetainPtr<WKDataListSuggestionTable> _table;
    WebKit::WebDataListSuggestionsDropdownMac* _dropdown;
    Vector<String> _suggestions;
    NSView *_view;
    RetainPtr<NSWindow> _enclosingWindow;
}

- (id)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(NSView *)view;
- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownMac*)dropdown;
- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information;
- (void)moveSelectionByDirection:(const String&)direction;
- (void)invalidate;

- (String)currentSelectedString;
@end

namespace WebKit {

Ref<WebDataListSuggestionsDropdownMac> WebDataListSuggestionsDropdownMac::create(WebPageProxy& page, NSView *view)
{
    return adoptRef(*new WebDataListSuggestionsDropdownMac(page, view));
}

WebDataListSuggestionsDropdownMac::~WebDataListSuggestionsDropdownMac() { }

WebDataListSuggestionsDropdownMac::WebDataListSuggestionsDropdownMac(WebPageProxy& page, NSView *view)
    : WebDataListSuggestionsDropdown(page)
    , m_view(view)
{
}

void WebDataListSuggestionsDropdownMac::show(WebCore::DataListSuggestionInformation&& information)
{
    if (m_dropdownUI) {
        [m_dropdownUI updateWithInformation:WTFMove(information)];
        return;
    }

    m_dropdownUI = adoptNS([[WKDataListSuggestionsView alloc] initWithInformation:WTFMove(information) inView:m_view]);
    [m_dropdownUI showSuggestionsDropdown:this];
}

void WebDataListSuggestionsDropdownMac::didSelectOption(const String& selectedOption)
{
    if (!m_page)
        return;

    m_page->didSelectOption(selectedOption);
    close();
}

void WebDataListSuggestionsDropdownMac::selectOption()
{
    if (!m_page)
        return;

    String selectedOption = [m_dropdownUI currentSelectedString];
    if (!selectedOption.isNull())
        m_page->didSelectOption(selectedOption);

    close();
}

void WebDataListSuggestionsDropdownMac::handleKeydownWithIdentifier(const String& key)
{
    if (key == "Enter")
        selectOption();
    else if (key == "Up" || key == "Down")
        [m_dropdownUI moveSelectionByDirection:key];
}

void WebDataListSuggestionsDropdownMac::close()
{
    [m_dropdownUI invalidate];
    m_dropdownUI = nil;
    WebDataListSuggestionsDropdown::close();
}

} // namespace WebKit

@implementation WKDataListSuggestionWindow
@end

@implementation WKDataListSuggestionCell

@synthesize active=_active;

- (id)initWithFrame:(NSRect)frameRect
{
    if (!(self = [super initWithFrame:frameRect]))
        return self;

    _textField = adoptNS([[NSTextField alloc] init]);
    [_textField setEditable:NO];
    [_textField setBezeled:NO];
    [_textField setBackgroundColor:[NSColor clearColor]];
    [self addSubview:_textField.get()];

    [self setIdentifier:@"WKDataListSuggestionCell"];

    [self addTrackingRect:self.bounds owner:self userData:nil assumeInside:NO];

    return self;
}

- (void)setText:(NSString *)text
{
    [_textField setStringValue:text];
    [_textField sizeToFit];

    NSRect textFieldFrame = [_textField frame];
    [_textField setFrame:NSMakeRect(0, (NSHeight(self.frame) - NSHeight(textFieldFrame)) / 2, NSWidth(textFieldFrame), NSHeight(textFieldFrame))];

    _mouseIsOver = NO;
    self.active = NO;
}

- (void)setActive:(BOOL)shouldActivate
{
    _active = shouldActivate;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (self.active) {
        [[NSColor alternateSelectedControlColor] setFill];
        [_textField setTextColor:[NSColor alternateSelectedControlTextColor]];
    } else if (_mouseIsOver) {
        [[NSColor quaternaryLabelColor] setFill];
        [_textField setTextColor:[NSColor textColor]];
    } else {
        [[NSColor controlBackgroundColor] setFill];
        [_textField setTextColor:[NSColor textColor]];
    }

    NSRectFill(dirtyRect);
    [super drawRect:dirtyRect];
}

- (void)mouseEntered:(NSEvent *)event
{
    [super mouseEntered:event];
    _mouseIsOver = YES;
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)event
{
    [super mouseExited:event];
    _mouseIsOver = NO;
    [self setNeedsDisplay:YES];
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

@end

@implementation WKDataListSuggestionTable

- (id)initWithElementRect:(const WebCore::IntRect&)rect
{
    if (!(self = [super initWithFrame:NSMakeRect(0, 0, rect.width(), 0)]))
        return self;

    [self setIntercellSpacing:NSZeroSize];
    [self setHeaderView:nil];
    [self setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleNone];

    auto column = adoptNS([[NSTableColumn alloc] init]);
    [column setWidth:rect.width()];
    [self addTableColumn:column.get()];

    _enclosingScrollView = adoptNS([[NSScrollView alloc] init]);
    [_enclosingScrollView setHasVerticalScroller:YES];
    [_enclosingScrollView setVerticalScrollElasticity:NSScrollElasticityNone];
    [_enclosingScrollView setHorizontalScrollElasticity:NSScrollElasticityNone];
    [_enclosingScrollView setDocumentView:self];

    auto dropShadow = adoptNS([[NSShadow alloc] init]);
    [dropShadow setShadowColor:[NSColor systemGrayColor]];
    [dropShadow setShadowOffset:NSMakeSize(0, dropdownShadowHeight)];
    [dropShadow setShadowBlurRadius:dropdownShadowBlurRadius];
    [_enclosingScrollView setWantsLayer:YES];
    [_enclosingScrollView setShadow:dropShadow.get()];

    return self;
}

- (void)setVisibleRect:(NSRect)rect
{
    [self setFrame:NSMakeRect(0, 0, NSWidth(rect) - dropdownShadowHeight * 2, 0)];
    [_enclosingScrollView setFrame:NSMakeRect(dropdownShadowHeight, dropdownShadowHeight, NSWidth(rect) - dropdownShadowHeight * 2, NSHeight(rect) - dropdownShadowHeight)];
}

- (Optional<size_t>)currentActiveRow
{
    return _activeRow;
}

- (void)setActiveRow:(size_t)row
{
    if (_activeRow) {
        WKDataListSuggestionCell *oldCell = (WKDataListSuggestionCell *)[self viewAtColumn:0 row:_activeRow.value() makeIfNecessary:NO];
        oldCell.active = NO;
    }

    [self scrollRowToVisible:row];
    WKDataListSuggestionCell *newCell = (WKDataListSuggestionCell *)[self viewAtColumn:0 row:row makeIfNecessary:NO];
    newCell.active = YES;

    _activeRow = row;
}

- (void)reload
{
    _activeRow = WTF::nullopt;
    [self reloadData];
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

- (NSScrollView *)enclosingScrollView
{
    return _enclosingScrollView.get();
}

- (void)removeFromSuperviewWithoutNeedingDisplay
{
    [super removeFromSuperviewWithoutNeedingDisplay];
    [_enclosingScrollView removeFromSuperviewWithoutNeedingDisplay];
}

@end

@implementation WKDataListSuggestionsView

- (id)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(NSView *)view
{
    if (!(self = [super init]))
        return self;

    _view = view;
    _suggestions = WTFMove(information.suggestions);
    _table = adoptNS([[WKDataListSuggestionTable alloc] initWithElementRect:information.elementRect]);

    _enclosingWindow = adoptNS([[WKDataListSuggestionWindow alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [_enclosingWindow setReleasedWhenClosed:NO];
    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
    [_enclosingWindow setBackgroundColor:[NSColor clearColor]];
    [_enclosingWindow setOpaque:NO];

    [_table setVisibleRect:[_enclosingWindow frame]];
    [_table setDelegate:self];
    [_table setDataSource:self];
    [_table setAction:@selector(selectedRow:)];
    [_table setTarget:self];

    return self;
}

- (String)currentSelectedString
{
    Optional<size_t> selectedRow = [_table currentActiveRow];
    if (selectedRow && selectedRow.value() < _suggestions.size())
        return _suggestions.at(selectedRow.value());

    return String();
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    _suggestions = WTFMove(information.suggestions);
    [_table reload];

    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
    [_table setVisibleRect:[_enclosingWindow frame]];
}

- (void)notifyAccessibilityClients:(NSString *)info
{
    NSDictionary<NSAccessibilityNotificationUserInfoKey, id> *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        NSAccessibilityPriorityKey, @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey, info, nil];
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, userInfo);
}

- (void)moveSelectionByDirection:(const String&)direction
{
    size_t size = _suggestions.size();
    Optional<size_t> oldSelection = [_table currentActiveRow];

    size_t newSelection;
    if (oldSelection) {
        size_t oldValue = oldSelection.value();
        if (direction == "Up")
            newSelection = oldValue ? (oldValue - 1) : (size - 1);
        else
            newSelection = (oldValue + 1) % size;
    } else
        newSelection = (direction == "Up") ? (size - 1) : 0;

    [_table setActiveRow:newSelection];

    // Notify accessibility clients of new selection.
    NSString *currentSelectedString = [self currentSelectedString];
    [self notifyAccessibilityClients:currentSelectedString];
}

- (void)invalidate
{
    [_table removeFromSuperviewWithoutNeedingDisplay];

    [_table setDelegate:nil];
    [_table setDataSource:nil];
    [_table setTarget:nil];

    _table = nil;

    [[_view window] removeChildWindow:_enclosingWindow.get()];
    [_enclosingWindow close];
    _enclosingWindow = nil;

    // Notify accessibility clients that datalist went away.
    NSString *info = WEB_UI_STRING("Suggestions list hidden.", "Accessibility announcement for the data list suggestions dropdown going away.");
    [self notifyAccessibilityClients:info];
}

- (NSRect)dropdownRectForElementRect:(const WebCore::IntRect&)rect
{
    NSRect windowRect = [[_view window] convertRectToScreen:[_view convertRect:rect toView:nil]];
    CGFloat height = std::min(_suggestions.size(), dropdownMaxSuggestions) * dropdownRowHeight;
    return NSMakeRect(NSMinX(windowRect) - dropdownShadowHeight, NSMinY(windowRect) - height - dropdownShadowHeight - dropdownTopMargin, rect.width() + dropdownShadowHeight * 2, height + dropdownShadowHeight);
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownMac*)dropdown
{
    _dropdown = dropdown;
    [[_enclosingWindow contentView] addSubview:[_table enclosingScrollView]];
    [_table reload];
    [[_view window] addChildWindow:_enclosingWindow.get() ordered:NSWindowAbove];
    [[_table enclosingScrollView] flashScrollers];

    // Notify accessibility clients of datalist becoming visible.
    NSString *currentSelectedString = [self currentSelectedString];
    NSString *info = [NSString stringWithFormat:WEB_UI_STRING("Suggestions list visible, %@", "Accessibility announcement that the suggestions list became visible. The format argument is for the first option in the list."), currentSelectedString];
    [self notifyAccessibilityClients:info];
}

- (void)selectedRow:(NSTableView *)sender
{
    _dropdown->didSelectOption(_suggestions.at([sender selectedRow]));
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _suggestions.size();
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    return dropdownRowHeight;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    WKDataListSuggestionCell *result = [tableView makeViewWithIdentifier:suggestionCellReuseIdentifier owner:self];

    if (!result) {
        result = [[[WKDataListSuggestionCell alloc] initWithFrame:NSMakeRect(0, 0, tableView.frame.size.width, dropdownRowHeight)] autorelease];
        [result setIdentifier:suggestionCellReuseIdentifier];
    }

    [result setText:_suggestions.at(row)];

    Optional<size_t> currentActiveRow = [_table currentActiveRow];
    if (currentActiveRow && static_cast<size_t>(row) == currentActiveRow.value())
        result.active = YES;

    return result;
}

@end

#endif // ENABLE(DATALIST_ELEMENT) && USE(APPKIT)
