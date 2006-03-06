/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "KWQLineEdit.h"

#import "Color.h"
#import "IntSize.h"
#import "KWQExceptions.h"
#import "Font.h"
#import "Logging.h"
#import "KWQTextField.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreViewFactory.h"
#import "WidgetClient.h"

using namespace WebCore;

@interface NSSearchField (SearchFieldSecrets)
- (void)_addStringToRecentSearches:(NSString *)string;
@end

NSControlSize KWQNSControlSizeForFont(const Font& f)
{
    const int fontSize = f.pixelSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

QLineEdit::QLineEdit(Type type)
    : m_type(type)
{
    KWQ_BLOCK_EXCEPTIONS;
    id view = nil;
    switch (type) {
        case Normal:
            view = [KWQTextField alloc];
            break;
        case Password:
            view = [KWQSecureTextField alloc];
            break;
        case Search:
            view = [KWQSearchField alloc];
            break;
    }
    ASSERT(view);
    [view initWithQLineEdit:this];
    m_controller = [view controller];
    setView((NSView *)view);
    [view release];
    [view setSelectable:YES]; // must do this explicitly so setEditable:NO does not make it NO
    KWQ_UNBLOCK_EXCEPTIONS;
}

QLineEdit::~QLineEdit()
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_controller detachQLineEdit];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setCursorPosition(int pos)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSRange tempRange = {pos, 0}; // 4213314
    [m_controller setSelectedRange:tempRange];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int QLineEdit::cursorPosition() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return [m_controller selectedRange].location;
    KWQ_UNBLOCK_EXCEPTIONS;
    return 0;
}

void QLineEdit::setFont(const Font &font)
{
    Widget::setFont(font);
    if (m_type == Search) {
        const NSControlSize size = KWQNSControlSizeForFont(font);    
        NSControl * const searchField = static_cast<NSControl *>(getView());
        [[searchField cell] setControlSize:size];
        [searchField setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    }
    else {
        NSTextField *textField = (NSTextField *)getView();
        KWQ_BLOCK_EXCEPTIONS;
        [textField setFont:font.getNSFont()];
        KWQ_UNBLOCK_EXCEPTIONS;
    }
}

void QLineEdit::setColors(const Color& background, const Color& foreground)
{
    NSTextField *textField = (NSTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;

    // Below we've added a special case that maps any completely transparent color to white.  This is a workaround for the following
    // AppKit problems: <rdar://problem/3142730> and <rdar://problem/3036580>.  Without this special case we have black
    // backgrounds on some text fields as described in <rdar://problem/3854383>.  Text fields will still not be able to display
    // transparent and translucent backgrounds, which will need to be fixed in the future.  See  <rdar://problem/3865114>.
        
    [textField setTextColor:nsColor(foreground)];

    Color bg = background;
    if (!bg.isValid() || bg.alpha() == 0)
        bg = Color::white;
    [textField setBackgroundColor:nsColor(bg)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setText(const DOMString& s)
{
    NSTextField *textField = (NSTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setStringValue:s];
    KWQ_UNBLOCK_EXCEPTIONS;
}

DOMString QLineEdit::text() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return DOMString([m_controller string]);
    KWQ_UNBLOCK_EXCEPTIONS;
    return DOMString();
}

void QLineEdit::setMaxLength(int len)
{
    [m_controller setMaximumLength:len];
}

bool QLineEdit::isReadOnly() const
{
    NSTextField *textField = (NSTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    return ![textField isEditable];
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

void QLineEdit::setReadOnly(bool flag)
{
    NSTextField *textField = (NSTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setEditable:!flag];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int QLineEdit::maxLength() const
{
    return [m_controller maximumLength];
}


void QLineEdit::selectAll()
{
    if (!hasFocus()) {
        // Do the makeFirstResponder ourselves explicitly (by calling setFocus)
        // so WebHTMLView will know it's programmatic and not the user clicking.
        setFocus();
    } else {
        KWQ_BLOCK_EXCEPTIONS;
        NSTextField *textField = (NSTextField *)getView();
        [textField selectText:nil];
        KWQ_UNBLOCK_EXCEPTIONS;
    }
}

int QLineEdit::selectionStart() const
{
    KWQ_BLOCK_EXCEPTIONS;
    if ([m_controller hasSelection]) {
        return [m_controller selectedRange].location;
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    return -1;
}

QString QLineEdit::selectedText() const
{
    KWQ_BLOCK_EXCEPTIONS;
    NSRange range = [m_controller selectedRange];
    NSString *str = [m_controller string];
    return QString::fromNSString([str substringWithRange:range]);
    KWQ_UNBLOCK_EXCEPTIONS;
    return QString();
}

void QLineEdit::setSelection(int start, int length)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSRange tempRange = {start, length}; // 4213314
    [m_controller setSelectedRange:tempRange];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QLineEdit::hasSelectedText() const
{
    return [m_controller hasSelection];
}

bool QLineEdit::edited() const
{
    return [m_controller edited];
}

void QLineEdit::setEdited(bool flag)
{
    [m_controller setEdited:flag];
}

static const NSSize textFieldMargins = { 8, 6 };

IntSize QLineEdit::sizeForCharacterWidth(int numCharacters) const
{
    // Figure out how big a text field needs to be for a given number of characters
    // (using "0" as the nominal character).

    NSTextField *textField = (NSTextField *)getView();

    ASSERT(numCharacters > 0);

    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow, but bug 3711080 shows we can't yet.
    // Note: baselinePosition below also has the height computation.
    NSSize size = textFieldMargins;

    KWQ_BLOCK_EXCEPTIONS;

    WebCoreFont font;
    WebCoreInitializeFont(&font);
    font.font = [textField font];
    font.forPrinter = ![NSGraphicsContext currentContextDrawingToScreen];
    id <WebCoreTextRenderer> renderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font];

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    size.height += [layoutManager defaultLineHeightForFont:font.font];
    [layoutManager release];

    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;

    const UniChar zero = '0';
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, &zero, 1, 0, 1);

    size.width += ceilf([renderer floatWidthForRun:&run style:&style] * numCharacters);

    return IntSize(size);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntSize(0, 0);
}

int QLineEdit::baselinePosition(int height) const
{
    NSTextField *textField = (NSTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSFont *font = [textField font];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    float lineHeight = [layoutManager defaultLineHeightForFont:font];
    [layoutManager release];
    NSRect bounds = NSMakeRect(0, 0, 100, textFieldMargins.height + lineHeight); // bounds width is arbitrary, height same as what sizeForCharacterWidth returns
    NSRect drawingRect = [[textField cell] drawingRectForBounds:bounds];
    return static_cast<int>(ceilf(drawingRect.origin.y - bounds.origin.y + lineHeight + [font descender]));
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

void QLineEdit::setAlignment(HorizontalAlignment alignment)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSTextField *textField = (NSTextField *)getView();
    [textField setAlignment:KWQNSTextAlignment(alignment)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setWritingDirection(TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_controller setBaseWritingDirection:(direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];
    KWQ_UNBLOCK_EXCEPTIONS;
}

Widget::FocusPolicy QLineEdit::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool QLineEdit::checksDescendantsForFocus() const
{
    return true;
}

NSTextAlignment KWQNSTextAlignment(HorizontalAlignment a)
{
    switch (a) {
        case AlignLeft:
            return NSLeftTextAlignment;
        case AlignRight:
            return NSRightTextAlignment;
        case AlignHCenter:
            return NSCenterTextAlignment;
    }
    LOG_ERROR("unsupported alignment");
    return NSLeftTextAlignment;
}

void QLineEdit::setLiveSearch(bool liveSearch)
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] setSendsWholeSearchString:!liveSearch];
}

void QLineEdit::setAutoSaveName(const DOMString& name)
{
    if (m_type != Search)
        return;
    
    DOMString autosave;
    if (!name.isEmpty())
        autosave = "com.apple.WebKit.searchField:" + name;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [searchField setRecentsAutosaveName:autosave];
}

void QLineEdit::setMaxResults(int maxResults)
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    id searchCell = [searchField cell];
    if (maxResults == -1) {
        [searchCell setSearchButtonCell:nil];
        [searchCell setSearchMenuTemplate:nil];
    }
    else {
        NSMenu* cellMenu = [searchCell searchMenuTemplate];
        NSButtonCell* buttonCell = [searchCell searchButtonCell];
        if (!buttonCell)
            [searchCell resetSearchButtonCell];
        if (cellMenu && !maxResults)
            [searchCell setSearchMenuTemplate:nil];
        else if (!cellMenu && maxResults)
            [searchCell setSearchMenuTemplate:[[WebCoreViewFactory sharedFactory] cellMenuForSearchField]];
    }
    
    [searchCell setMaximumRecents:maxResults];
}

void QLineEdit::setPlaceholderString(const DOM::DOMString& placeholder)
{
    NSTextField *textField = (NSTextField *)getView();
    [[textField cell] setPlaceholderString:placeholder];
}

void QLineEdit::addSearchResult()
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] _addStringToRecentSearches:[searchField stringValue]];
}

