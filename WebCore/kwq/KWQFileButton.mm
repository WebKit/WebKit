/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQFileButton.h"

// FIXME: These need to be localized.
#define BUTTON_LABEL ("Choose File")
#define NO_FILE_SELECTED (@"no file selected")

#define AFTER_BUTTON_SPACING 4
#define ICON_HEIGHT 16
#define ICON_WIDTH 16
#define ICON_FILENAME_SPACING 2
// FIXME: Is it OK to hard-code the width of the filename part of this control?
#define FILENAME_WIDTH 200

#define ADDITIONAL_WIDTH (AFTER_BUTTON_SPACING + ICON_WIDTH + ICON_FILENAME_SPACING + FILENAME_WIDTH)

// FIXME: Clicks on the text should pull up the sheet too.

@interface KWQFileButtonAdapter : NSObject
{
    KWQFileButton *button;
}

- initWithKWQFileButton:(KWQFileButton *)button;
- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end

KWQFileButton::KWQFileButton()
    : QPushButton(BUTTON_LABEL, 0)
    , _textChanged(this, SIGNAL(textChanged(const QString &)))
    , _adapter([[KWQFileButtonAdapter alloc] initWithKWQFileButton:this])
    , _icon(nil)
    , _label([NO_FILE_SELECTED retain])
{
}

KWQFileButton::~KWQFileButton()
{
    _adapter->button = 0;
    [_adapter release];
    [_icon release];
    [_label release];
}
    
void KWQFileButton::setFilename(const QString &f)
{
    if (_filename == f) {
        return;
    }
    _filename = f;
    _textChanged.call(_filename);
    
    // Get the label.
    [_label release];
    if (_filename.isEmpty()) {
        _label = [NO_FILE_SELECTED retain];
    } else {
        _label = [[[[NSFileManager defaultManager] componentsToDisplayForPath:_filename.getNSString()] lastObject] copy];
    }
    
    // Get the icon.
    [_icon release];
    _icon = [[[NSWorkspace sharedWorkspace] iconForFile:_filename.getNSString()] retain];
    
    // Dirty the part of the view past the button, including the icon and text.
    QRect r = QPushButton::frameGeometry();
    r.setX(r.x() + r.width() + AFTER_BUTTON_SPACING);
    r.setWidth(ADDITIONAL_WIDTH - AFTER_BUTTON_SPACING);
    [[getView() superview] setNeedsDisplayInRect:frameGeometry()];
}

void KWQFileButton::clicked()
{
    NSOpenPanel *sheet = [NSOpenPanel openPanel];
    
    [sheet setPrompt:@"Choose"];
    
    [_adapter retain];
    
    [sheet beginSheetForDirectory:@"~" file:@"" types:nil
        modalForWindow:[getView() window] modalDelegate:_adapter
        didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
        contextInfo:nil];

    QPushButton::clicked();
}

QSize KWQFileButton::sizeHint() const 
{
    QSize s = QPushButton::sizeHint();
    s.setWidth(s.width() + ADDITIONAL_WIDTH);
    return s;
}

QRect KWQFileButton::frameGeometry() const
{
    QRect r = QPushButton::frameGeometry();
    r.setWidth(r.width() + ADDITIONAL_WIDTH);
    return r;
}

void KWQFileButton::setFrameGeometry(const QRect &rect)
{
    QRect r = rect;
    r.setWidth(r.width() - ADDITIONAL_WIDTH);
    QPushButton::setFrameGeometry(r);
}

int KWQFileButton::baselinePosition() const
{
    return QPushButton::baselinePosition();
}

void KWQFileButton::paint(QPainter *p, const QRect &r)
{
    if (p->paintingDisabled()) {
        return;
    }
    
    QPushButton::paint(p, r);
    
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(NSIntersectionRect(frameGeometry(), r));

    int left = x() + width() - ADDITIONAL_WIDTH + AFTER_BUTTON_SPACING;

    if (_icon) {
        [_icon drawInRect:NSMakeRect(left, y() + (height() - ICON_HEIGHT) / 2, ICON_WIDTH, ICON_HEIGHT)
            fromRect:NSMakeRect(0, 0, [_icon size].width, [_icon size].height)
            operation:NSCompositeSourceOver fraction:1.0];
        left += ICON_WIDTH + ICON_FILENAME_SPACING;
    }

    // FIXME: Ellipsize the text to fit in the space available.
    NSFont *font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
    NSDictionary *attributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
    [_label drawAtPoint:NSMakePoint(left, y() + baselinePosition() - [font ascender]) withAttributes:attributes];

    [NSGraphicsContext restoreGraphicsState];
}

@implementation KWQFileButtonAdapter

- initWithKWQFileButton:(KWQFileButton *)b
{
    [super init];
    button = b;
    return self;
}

- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (button && returnCode == NSOKButton && [[sheet filenames] count] == 1) {
        button->setFilename(QString::fromNSString([[sheet filenames] objectAtIndex:0]));
    }
    [self release];
}

@end
