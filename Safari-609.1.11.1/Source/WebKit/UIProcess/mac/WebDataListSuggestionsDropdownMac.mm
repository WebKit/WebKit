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

#import "AppKitSPI.h"
#import "WebPageProxy.h"
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/NSColorSPI.h>

static const CGFloat dropdownTopMargin = 3;
static const CGFloat dropdownVerticalPadding = 4;
static const CGFloat dropdownRowHeight = 20;
static const size_t dropdownMaxSuggestions = 6;
static NSString * const suggestionCellReuseIdentifier = @"WKDataListSuggestionView";

@interface WKDataListSuggestionWindow : NSWindow
@end

@interface WKDataListSuggestionView : NSTableCellView
@end

@interface WKDataListSuggestionTableRowView : NSTableRowView
@end

@interface WKDataListSuggestionTableView : NSTableView
@end

@interface WKDataListSuggestionsController : NSObject<NSTableViewDataSource, NSTableViewDelegate>

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

    m_dropdownUI = adoptNS([[WKDataListSuggestionsController alloc] initWithInformation:WTFMove(information) inView:m_view]);
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

@implementation WKDataListSuggestionWindow {
    RetainPtr<NSVisualEffectView> _backdropView;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingStoreType defer:(BOOL)defer
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingStoreType defer:defer];
    if (!self)
        return nil;

    self.hasShadow = YES;

    _backdropView = [[NSVisualEffectView alloc] initWithFrame:contentRect];
    [_backdropView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_backdropView setMaterial:NSVisualEffectMaterialMenu];
    [_backdropView setState:NSVisualEffectStateActive];
    [_backdropView setBlendingMode:NSVisualEffectBlendingModeBehindWindow];

    [self setContentView:_backdropView.get()];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)hasKeyAppearance
{
    return YES;
}

- (NSWindowShadowOptions)shadowOptions
{
    return NSWindowShadowSecondaryWindow;
}

@end

@implementation WKDataListSuggestionView {
    RetainPtr<NSTextField> _textField;
}

- (id)initWithFrame:(NSRect)frameRect
{
    if (!(self = [super initWithFrame:frameRect]))
        return self;

    _textField = adoptNS([[NSTextField alloc] initWithFrame:frameRect]);
    self.textField = _textField.get();

    [self addSubview:self.textField];

    self.identifier = suggestionCellReuseIdentifier;
    self.textField.editable = NO;
    self.textField.bezeled = NO;
    self.textField.font = [NSFont menuFontOfSize:0];
    self.textField.drawsBackground = NO;

    return self;
}

- (void)setText:(NSString *)text
{
    self.textField.stringValue = text;
}

- (void)setBackgroundStyle:(NSBackgroundStyle)backgroundStyle
{
    [super setBackgroundStyle:backgroundStyle];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    self.textField.textColor = backgroundStyle == NSBackgroundStyleLight ? [NSColor textColor] : [NSColor alternateSelectedControlTextColor];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (BOOL)acceptsFirstResponder
{
    return NO;
}

@end

@implementation WKDataListSuggestionTableRowView

- (void)drawSelectionInRect:(NSRect)dirtyRect
{
    [self setEmphasized:YES];
    [super drawSelectionInRect:dirtyRect];
}

@end

@implementation WKDataListSuggestionTableView {
    RetainPtr<NSScrollView> _enclosingScrollView;
}

- (id)initWithElementRect:(const WebCore::IntRect&)rect
{
    if (!(self = [super initWithFrame:NSMakeRect(0, 0, rect.width(), 0)]))
        return self;

    [self setHeaderView:nil];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIntercellSpacing:NSMakeSize(0, self.intercellSpacing.height)];

    auto column = adoptNS([[NSTableColumn alloc] init]);
    [column setWidth:rect.width()];
    [self addTableColumn:column.get()];

    _enclosingScrollView = adoptNS([[NSScrollView alloc] init]);
    [_enclosingScrollView setHasVerticalScroller:YES];
    [_enclosingScrollView setVerticalScrollElasticity:NSScrollElasticityAllowed];
    [_enclosingScrollView setHorizontalScrollElasticity:NSScrollElasticityNone];
    [_enclosingScrollView setDocumentView:self];
    [_enclosingScrollView setDrawsBackground:NO];
    [[_enclosingScrollView contentView] setAutomaticallyAdjustsContentInsets:NO];
    [[_enclosingScrollView contentView] setContentInsets:NSEdgeInsetsMake(dropdownVerticalPadding, 0, dropdownVerticalPadding, 0)];
    [[_enclosingScrollView contentView] scrollToPoint:NSMakePoint(0, -dropdownVerticalPadding)];

    return self;
}

- (void)layout
{
    [_enclosingScrollView setFrame:[_enclosingScrollView superview].bounds];
}

- (void)reload
{
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

@implementation WKDataListSuggestionsController {
    WebKit::WebDataListSuggestionsDropdownMac* _dropdown;
    Vector<String> _suggestions;
    NSView *_presentingView;

    RetainPtr<WKDataListSuggestionWindow> _enclosingWindow;
    RetainPtr<WKDataListSuggestionTableView> _table;
}

- (id)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(NSView *)presentingView
{
    if (!(self = [super init]))
        return self;

    _presentingView = presentingView;
    _suggestions = WTFMove(information.suggestions);
    _table = adoptNS([[WKDataListSuggestionTableView alloc] initWithElementRect:information.elementRect]);

    _enclosingWindow = adoptNS([[WKDataListSuggestionWindow alloc] initWithContentRect:NSZeroRect styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView) backing:NSBackingStoreBuffered defer:NO]);
    [_enclosingWindow setReleasedWhenClosed:NO];
    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
    [_enclosingWindow setTitleVisibility:NSWindowTitleHidden];
    [_enclosingWindow setTitlebarAppearsTransparent:YES];
    [_enclosingWindow setMovable:NO];
    [_enclosingWindow setBackgroundColor:[NSColor clearColor]];
    [_enclosingWindow setOpaque:NO];

    [_table setDelegate:self];
    [_table setDataSource:self];
    [_table setAction:@selector(selectedRow:)];
    [_table setTarget:self];

    return self;
}

- (String)currentSelectedString
{
    NSInteger selectedRow = [_table selectedRow];

    if (selectedRow >= 0 && static_cast<size_t>(selectedRow) < _suggestions.size())
        return _suggestions.at(selectedRow);

    return String();
}

- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information
{
    _suggestions = WTFMove(information.suggestions);
    [_table reload];

    [_enclosingWindow setFrame:[self dropdownRectForElementRect:information.elementRect] display:YES];
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
    NSInteger oldSelection = [_table selectedRow];

    size_t newSelection;
    if (oldSelection != -1) {
        if (direction == "Up")
            newSelection = oldSelection ? (oldSelection - 1) : (size - 1);
        else
            newSelection = (oldSelection + 1) % size;
    } else
        newSelection = (direction == "Up") ? (size - 1) : 0;

    [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:newSelection] byExtendingSelection:NO];

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

    [[_presentingView window] removeChildWindow:_enclosingWindow.get()];
    [_enclosingWindow close];
    _enclosingWindow = nil;

    // Notify accessibility clients that datalist went away.
    NSString *info = WEB_UI_STRING("Suggestions list hidden.", "Accessibility announcement for the data list suggestions dropdown going away.");
    [self notifyAccessibilityClients:info];
}

- (NSRect)dropdownRectForElementRect:(const WebCore::IntRect&)rect
{
    NSRect windowRect = [[_presentingView window] convertRectToScreen:[_presentingView convertRect:rect toView:nil]];
    auto visibleSuggestionCount = std::min(_suggestions.size(), dropdownMaxSuggestions);
    CGFloat height = visibleSuggestionCount * (dropdownRowHeight + [_table intercellSpacing].height) + (dropdownVerticalPadding * 2);
    return NSMakeRect(NSMinX(windowRect), NSMinY(windowRect) - height - dropdownTopMargin, rect.width(), height);
}

- (void)showSuggestionsDropdown:(WebKit::WebDataListSuggestionsDropdownMac*)dropdown
{
    _dropdown = dropdown;
    [[_enclosingWindow contentView] addSubview:[_table enclosingScrollView]];
    [_table reload];
    [[_presentingView window] addChildWindow:_enclosingWindow.get() ordered:NSWindowAbove];
    [[_table enclosingScrollView] flashScrollers];

    // Notify accessibility clients of datalist becoming visible.
    NSString *currentSelectedString = [self currentSelectedString];
    NSString *info = [NSString stringWithFormat:WEB_UI_STRING("Suggestions list visible, %@", "Accessibility announcement that the suggestions list became visible. The format argument is for the first option in the list."), currentSelectedString];
    [self notifyAccessibilityClients:info];
}

- (void)selectedRow:(NSTableView *)sender
{
    auto selectedString = self.currentSelectedString;
    if (!selectedString)
        return;

    _dropdown->didSelectOption(selectedString);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _suggestions.size();
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    return dropdownRowHeight;
}

- (NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row
{
    return [[[WKDataListSuggestionTableRowView alloc] init] autorelease];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    WKDataListSuggestionView *result = [tableView makeViewWithIdentifier:suggestionCellReuseIdentifier owner:self];

    if (!result) {
        result = [[[WKDataListSuggestionView alloc] initWithFrame:NSMakeRect(0, 0, tableView.frame.size.width, dropdownRowHeight)] autorelease];
        [result setIdentifier:suggestionCellReuseIdentifier];
    }

    result.text = _suggestions.at(row);

    return result;
}

@end

#endif // ENABLE(DATALIST_ELEMENT) && USE(APPKIT)
