/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#import "KWQFileButton.h"

#import "BlockExceptions.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreStringTruncator.h"
#import "WebCoreViewFactory.h"
#import "render_form.h"

using namespace WebCore;

#define NO_FILE_SELECTED 

#define AFTER_BUTTON_SPACING 4
#define ICON_HEIGHT 16
#define ICON_WIDTH 16
#define ICON_FILENAME_SPACING 2

// We empirically determined that buttons have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define BUTTON_TOP_MARGIN 4
#define BUTTON_BOTTOM_MARGIN 6
#define BUTTON_LEFT_MARGIN 5
#define BUTTON_RIGHT_MARGIN 5

// AppKit calls this kThemePushButtonSmallTextOffset.
#define BUTTON_VERTICAL_FUDGE_FACTOR 2

@interface WebFileChooserButton : NSButton
{
    KWQFileButton *_widget;
}
- (id)initWithWidget:(KWQFileButton *)widget;
@end

@interface WebCoreFileButton : NSView <WebCoreOpenPanelResultListener>
{
    NSString *_filename;
    WebFileChooserButton *_button;
    NSImage *_icon;
    NSString *_label;
    BOOL _inNextValidKeyView;
    KWQFileButton *_widget;
    BOOL _isCurrentDragTarget;
}
- (void)setFilename:(NSString *)filename;
- (void)performClick;
- (NSString *)filename;
- (float)baseline;
- (void)setVisualFrame:(NSRect)rect;
- (NSRect)visualFrame;
- (NSSize)bestVisualFrameSizeForCharacterCount:(int)count;
- (id)initWithWidget:(KWQFileButton *)widget;
- (void)setEnabled:(BOOL)flag;
@end

@implementation WebCoreFileButton

- (void)positionButton
{
    [_button sizeToFit];
    [_button setFrameOrigin:NSMakePoint(0, 0)];
}

- (id)initWithWidget:(KWQFileButton*)widget
{
    self = [super init];
    if (!self)
        return nil;
    [self registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];

    _widget = widget;
    return self;
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _button = [[WebFileChooserButton alloc] initWithWidget:_widget];
        
        [_button setTitle:[[WebCoreViewFactory sharedFactory] fileButtonChooseFileLabel]];
        [[_button cell] setControlSize:NSSmallControlSize];
        [_button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
        [_button setBezelStyle:NSRoundedBezelStyle];
        [_button setTarget:self];
        [_button setAction:@selector(chooseButtonPressed:)];
        [_button setNextResponder:self];
        
        [self addSubview:_button];
        
        [self positionButton];
        [self setFilename:nil];
    }
    return self;
}

- (void)dealloc
{
    [_filename release];
    [_button release];
    [_icon release];
    [_label release];
    [super dealloc];
}

- (BOOL)isFlipped
{
    return YES;
}
        
- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(NSIntersectionRect(bounds, rect));
    
    if (_isCurrentDragTarget) {
        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.25f] set];
        NSRectFillUsingOperation([self bounds], NSCompositeSourceOver);
    }

    float left = NSMaxX([_button frame]) + AFTER_BUTTON_SPACING;

    if (_icon) {
        float top = (bounds.size.height - BUTTON_BOTTOM_MARGIN - BUTTON_TOP_MARGIN - ICON_HEIGHT) / 2
            + BUTTON_TOP_MARGIN;
        [_icon drawInRect:NSMakeRect(left, top, ICON_WIDTH, ICON_HEIGHT)
            fromRect:NSMakeRect(0, 0, [_icon size].width, [_icon size].height)
            operation:NSCompositeSourceOver fraction:1.0];
        left += ICON_WIDTH + ICON_FILENAME_SPACING;
    }

    NSFont *font = [_button font];
    NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
    [_label drawAtPoint:NSMakePoint(left, [self baseline] - [[_button font] ascender]) withAttributes:attributes];

    [NSGraphicsContext restoreGraphicsState];
}

- (void)updateLabel
{
    [_label release];
    
    NSString *label;
    if ([_filename length])
        label = _filename;
    else
        label = [[WebCoreViewFactory sharedFactory] fileButtonNoFileSelectedLabel];
    
    float left = NSMaxX([_button frame]) + AFTER_BUTTON_SPACING;
    if (_icon)
        left += ICON_WIDTH + ICON_FILENAME_SPACING;
    float labelWidth = [self bounds].size.width - left;

    _label = labelWidth <= 0 ? nil : [[WebCoreStringTruncator centerTruncateString:
        [[NSFileManager defaultManager] displayNameAtPath:label]
        toWidth:labelWidth withFont:[_button font]] copy];
}

- (void)setFilename:(NSString *)filename
{
    NSString *copy = [filename copy];
    [_filename release];
    _filename = copy;
    
    [_icon release];
    if ([_filename length] == 0 || [_filename characterAtIndex:0] != '/')
        _icon = nil;
    else {
        _icon = [[[NSWorkspace sharedWorkspace] iconForFile:_filename] retain];
        // I'm not sure why this has any effect, but including this line of code seems to make
        // the image appear right-side-up. As far as I know, the drawInRect method used above
        // in our drawRect method should work regardless of whether the image is flipped or not.
        [_icon setFlipped:YES];
    }
    
    [self updateLabel];
    
    [self setNeedsDisplay:YES];
}

- (NSString *)filename
{
    return [[_filename copy] autorelease];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    // FIXME: Can we just springs and struts instead of calling positionButton?
    [self positionButton];
    [self updateLabel];
}

- (NSSize)bestVisualFrameSizeForCharacterCount:(int)count
{
    ASSERT(count > 0);
    NSSize size = [[_button cell] cellSize];
    size.height -= BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    size.width -= BUTTON_LEFT_MARGIN + BUTTON_RIGHT_MARGIN;
    size.width += AFTER_BUTTON_SPACING + ICON_WIDTH + ICON_FILENAME_SPACING;
    size.width += count * [@"x" sizeWithAttributes:[NSDictionary dictionaryWithObject:[_button font] forKey:NSFontAttributeName]].width;
    return size;
}

- (NSRect)visualFrame
{
    ASSERT([self superview] == nil || [[self superview] isFlipped]);
    NSRect frame = [self frame];
    frame.origin.x += BUTTON_LEFT_MARGIN;
    frame.size.width -= BUTTON_LEFT_MARGIN;
    frame.origin.y += BUTTON_TOP_MARGIN;
    frame.size.height -= BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    return frame;
}

- (void)setVisualFrame:(NSRect)frame
{
    ASSERT([self superview] == nil || [[self superview] isFlipped]);
    frame.origin.x -= BUTTON_LEFT_MARGIN;
    frame.size.width += BUTTON_LEFT_MARGIN;
    frame.origin.y -= BUTTON_TOP_MARGIN;
    frame.size.height += BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN;
    [self setFrame:frame];
}

- (float)baseline
{
    // Button text is centered vertically, with a fudge factor to account for the shadow.
    ASSERT(_button);
    NSFont *buttonFont = [_button font];
    float ascender = [buttonFont ascender];
    float descender = [buttonFont descender];
    return -BUTTON_TOP_MARGIN
        + ([[_button cell] cellSize].height + BUTTON_TOP_MARGIN + BUTTON_BOTTOM_MARGIN - (ascender - descender)) / 2.0
        + ascender - BUTTON_VERTICAL_FUDGE_FACTOR;
}

- (void)beginSheet
{
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(_widget);
    [bridge retain];
    [bridge runOpenPanelForFileButtonWithResultListener:self];
}

- (void)changeFilename:(NSString *)filename
{
    // The != check here makes sure we don't consider a change from nil to nil as a change.
    if (_filename != filename && ![_filename isEqualToString:filename]) {
        [self setFilename:filename];
        if (_widget)
            _widget->filenameChanged(DeprecatedString::fromNSString(filename));
    }
}

- (void)chooseFilename:(NSString *)filename
{
    [self changeFilename:filename];
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(_widget);
    [bridge release];
}

- (void)cancel
{
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(_widget);
    [bridge release];
}

- (void)chooseButtonPressed:(id)sender
{
    if (_widget)
        _widget->sendConsumedMouseUp();
    if (_widget && _widget->client())
        _widget->client()->clicked(_widget);
    [self beginSheet];
}

- (void)mouseDown:(NSEvent *)event
{
    [self beginSheet];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [_button acceptsFirstResponder];
    if (become) {
        if (_widget && _widget->client() && !FrameMac::currentEventIsMouseDownInWidget(_widget))
            _widget->client()->scrollToVisible(_widget);
        if (_widget && _widget->client())
            _widget->client()->focusIn(_widget);
        [[self window] makeFirstResponder:_button];
    }
    return become;
}

- (NSView *)nextKeyView
{
    return _inNextValidKeyView
    ? FrameMac::nextKeyViewForWidget(_widget, KWQSelectingNext)
    : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return _inNextValidKeyView
    ? FrameMac::nextKeyViewForWidget(_widget, KWQSelectingPrevious)
    : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    _inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    _inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    _inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    _inNextValidKeyView = NO;
    return view;
}

- (void)performClick
{
    [_button performClick:nil];
}

- (void)setEnabled:(BOOL)flag
{
    [_button setEnabled:flag];
}

static NSString *validFilenameFromPasteboard(NSPasteboard* pBoard)
{
    NSArray *filenames = [pBoard propertyListForType:NSFilenamesPboardType];
    if ([filenames count] == 1) {
        NSString *filename = [filenames objectAtIndex:0];
        NSDictionary *fileAttributes = [[NSFileManager defaultManager] fileAttributesAtPath:filename traverseLink:YES];
        if ([[fileAttributes fileType] isEqualToString:NSFileTypeRegular])
            return filename;
    }
    return nil;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if (validFilenameFromPasteboard([sender draggingPasteboard])) {
        _isCurrentDragTarget = YES;
        [self setNeedsDisplay:YES];
        return NSDragOperationCopy;
    }
    return NSDragOperationNone;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
    if (_isCurrentDragTarget) {
        _isCurrentDragTarget = NO;
        [self setNeedsDisplay:YES];
    }
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    _isCurrentDragTarget = NO;
    NSString *filename = validFilenameFromPasteboard([sender draggingPasteboard]);
    if (filename) {
        [self changeFilename:filename];
        [self setNeedsDisplay:YES];
        return YES;
    }
    return NO;
}

@end

@implementation WebFileChooserButton

- (id)initWithWidget:(KWQFileButton *)widget
{
    [super init];
    _widget = widget;
    return self;
}

- (NSView *)nextValidKeyView
{
    return [[self superview] nextValidKeyView];
}

- (NSView *)previousValidKeyView
{
    return [[self superview] previousValidKeyView];
}

- (BOOL)resignFirstResponder
{
    if (_widget && _widget->client())
        _widget->client()->focusOut(_widget);
    return YES;
}

@end


KWQFileButton::KWQFileButton(Frame *frame)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    _buttonView = [[WebCoreFileButton alloc] initWithWidget:this];
    setView(_buttonView);
    [_buttonView release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void KWQFileButton::setFilename(const DeprecatedString &f)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_buttonView setFilename:f.getNSString()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void KWQFileButton::click(bool sendMouseEvents)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_buttonView performClick];
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);

    NSSize size = {0,0};
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    size = [_buttonView bestVisualFrameSizeForCharacterCount:characters];
    return IntSize(size);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize(0, 0);
}

IntRect KWQFileButton::frameGeometry() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return enclosingIntRect([_buttonView visualFrame]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

void KWQFileButton::setFrameGeometry(const IntRect &rect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_buttonView setVisualFrame:rect];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int KWQFileButton::baselinePosition(int height) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return (int)([_buttonView frame].origin.y + [_buttonView baseline] - [_buttonView visualFrame].origin.y);
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

Widget::FocusPolicy KWQFileButton::focusPolicy() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(this);
    if (!bridge || ![bridge impl] || ![bridge impl]->tabsToAllControls()) {
        return NoFocus;
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return Widget::focusPolicy();
}

void KWQFileButton::filenameChanged(const DeprecatedString& filename)
{
    m_name = filename;
    if (client())
        client()->valueChanged(this);
}

void KWQFileButton::setDisabled(bool flag)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [_buttonView setEnabled:!flag];
    END_BLOCK_OBJC_EXCEPTIONS;
}

