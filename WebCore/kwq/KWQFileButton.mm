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

#define BUTTON_FILENAME_SPACING 4
#define FILENAME_WIDTH 200
#define FONT_FAMILY ("Lucida Grande")

// FIXME: Clicks on the text should pull up the sheet too.

@interface KWQFileButtonAdapter : NSObject
{
    KWQFileButton *button;
}

- initWithKWQFileButton:(KWQFileButton *)button;
- (void)openPanelDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end

KWQFileButton::KWQFileButton()
    // FIXME: Needs to be localized.
    : QPushButton("Choose File", 0)
    , _textChanged(this, SIGNAL(textChanged(const QString &)))
    , _adapter([[KWQFileButtonAdapter alloc] initWithKWQFileButton:this])
{
}

KWQFileButton::~KWQFileButton()
{
    _adapter->button = 0;
    [_adapter release];
}
    
void KWQFileButton::setFilename(const QString &f)
{
    if (_filename == f) {
        return;
    }
    _filename = f;
    _textChanged.call(_filename);
    
    // Dirty the filename part of the view.
    QRect r = QPushButton::frameGeometry();
    r.setX(r.x() + r.width() + BUTTON_FILENAME_SPACING);
    r.setWidth(FILENAME_WIDTH);
    [[getView() superview] setNeedsDisplayInRect:frameGeometry()];
}

void KWQFileButton::clicked()
{
    NSOpenPanel *sheet = [NSOpenPanel openPanel];
    
    [_adapter retain];
    
    [sheet beginSheetForDirectory:@"" file:@"" types:nil
        modalForWindow:[getView() window] modalDelegate:_adapter
        didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:)
        contextInfo:nil];

    QPushButton::paint();
}

QSize KWQFileButton::sizeHint() const 
{
    QSize s = QPushButton::sizeHint();
    s.setWidth(s.width() + (BUTTON_FILENAME_SPACING + FILENAME_WIDTH));
    return s;
}

QRect KWQFileButton::frameGeometry() const
{
    QRect r = QPushButton::frameGeometry();
    r.setWidth(r.width() + (BUTTON_FILENAME_SPACING + FILENAME_WIDTH));
    return r;
}

void KWQFileButton::setFrameGeometry(const QRect &rect)
{
    QRect r = rect;
    r.setWidth(r.width() - (BUTTON_FILENAME_SPACING + FILENAME_WIDTH));
    QPushButton::setFrameGeometry(r);
}

int KWQFileButton::baselinePosition() const
{
    return QPushButton::baselinePosition();
}

void KWQFileButton::paint()
{
    QPushButton::paint();
    
    QString leafName = _filename;
    int slashPosition = leafName.findRev('/');
    if (slashPosition >= 0 || leafName == "/") {
        leafName.remove(0, slashPosition + 1);
    }
    
    // FIXME: Use same font as button, don't hardcode Lucida Grande.
    // FIXME: Ellipsize the text to fit in the box.
    QPainter painter;
    QFont font;
    font.setFamily(FONT_FAMILY);
    font.setPixelSize([NSFont smallSystemFontSize]);
    painter.save(); // wouldn't be needed in real Qt, but we need it
    painter.addClip(frameGeometry());
    painter.setFont(font);
    painter.drawText(x() + width() - FILENAME_WIDTH, y() + baselinePosition(), 0, 0, 0, leafName);
    painter.restore(); // wouldn't be needed in real Qt, but we need it
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
