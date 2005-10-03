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

#include "config.h"
#import "KWQLineEdit.h"

#import "KWQButton.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQLogging.h"
#import "KWQPalette.h"
#import "KWQTextField.h"
#import "WebCoreBridge.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreViewFactory.h"

@interface NSSearchField (SearchFieldSecrets)
- (void)_addStringToRecentSearches:(NSString *)string;
@end

QLineEdit::QLineEdit(Type type)
    : m_returnPressed(this, SIGNAL(returnPressed()))
    , m_textChanged(this, SIGNAL(textChanged(const QString &)))
    , m_clicked(this, SIGNAL(clicked()))
    , m_performSearch(this, SIGNAL(performSearch()))
    , m_selectionChanged(this, SIGNAL(selectionChanged()))
    , m_type(type)
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

void QLineEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
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

void QLineEdit::setPalette(const QPalette &palette)
{
    QWidget::setPalette(palette);

    NSTextField *textField = (NSTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;

    // Below we've added a special case that maps any completely transparent color to white.  This is a workaround for the following
    // AppKit problems: <rdar://problem/3142730> and <rdar://problem/3036580>.  Without this special case we have black
    // backgrounds on some text fields as described in <rdar://problem/3854383>.  Text fields will still not be able to display
    // transparent and translucent backgrounds, which will need to be fixed in the future.  See  <rdar://problem/3865114>.
        
    [textField setTextColor:nsColor(palette.foreground())];

    QColor background = palette.background();
    if (!background.isValid() || background.alpha() == 0)
        background = Qt::white;
    [textField setBackgroundColor:nsColor(background)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setText(const QString &s)
{
    NSTextField *textField = (NSTextField *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [textField setStringValue:s.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QString QLineEdit::text() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([m_controller string]);
    KWQ_UNBLOCK_EXCEPTIONS;
    return QString();
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

QSize QLineEdit::sizeForCharacterWidth(int numCharacters) const
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

    NSFont *font = [textField font];

    size.height += [font defaultLineHeightForFont];

    id <WebCoreTextRenderer> renderer = [[WebCoreTextRendererFactory sharedFactory]
        rendererWithFont:font usingPrinterFont:![NSGraphicsContext currentContextDrawingToScreen]];

    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;

    const UniChar zero = '0';
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, &zero, 1, 0, 1);

    size.width += ceilf([renderer floatWidthForRun:&run style:&style widths:0] * numCharacters);

    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize(size);
}

int QLineEdit::baselinePosition(int height) const
{
    NSTextField *textField = (NSTextField *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSFont *font = [textField font];
    float lineHeight = [font defaultLineHeightForFont];
    NSRect bounds = NSMakeRect(0, 0, 100, textFieldMargins.height + lineHeight); // bounds width is arbitrary, height same as what sizeForCharacterWidth returns
    NSRect drawingRect = [[textField cell] drawingRectForBounds:bounds];
    return static_cast<int>(ceilf(drawingRect.origin.y - bounds.origin.y + lineHeight + [font descender]));
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

void QLineEdit::clicked()
{
    m_clicked.call();
}

void QLineEdit::setAlignment(AlignmentFlags alignment)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSTextField *textField = (NSTextField *)getView();
    [textField setAlignment:KWQNSTextAlignmentForAlignmentFlags(alignment)];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QLineEdit::setWritingDirection(QPainter::TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;
    [m_controller setBaseWritingDirection:(direction == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QWidget::FocusPolicy QLineEdit::focusPolicy() const
{
    FocusPolicy policy = QWidget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool QLineEdit::checksDescendantsForFocus() const
{
    return true;
}

NSTextAlignment KWQNSTextAlignmentForAlignmentFlags(Qt::AlignmentFlags a)
{
    switch (a) {
        default:
            ERROR("unsupported alignment");
        case Qt::AlignLeft:
            return NSLeftTextAlignment;
        case Qt::AlignRight:
            return NSRightTextAlignment;
        case Qt::AlignHCenter:
            return NSCenterTextAlignment;
    }
}

void QLineEdit::setLiveSearch(bool liveSearch)
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] setSendsWholeSearchString:!liveSearch];
}

void QLineEdit::setAutoSaveName(const QString& name)
{
    if (m_type != Search)
        return;
    
    QString autosave;
    if (!name.isEmpty())
        autosave = "com.apple.WebKit.searchField:" + name;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [searchField setRecentsAutosaveName:autosave.getNSString()];
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

void QLineEdit::setPlaceholderString(const QString& placeholder)
{
    NSTextField *textField = (NSTextField *)getView();
    [[textField cell] setPlaceholderString:placeholder.getNSString()];
}

void QLineEdit::addSearchResult()
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] _addStringToRecentSearches:[searchField stringValue]];
}

