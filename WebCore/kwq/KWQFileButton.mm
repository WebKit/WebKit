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

#import "KWQFileButton.h"

#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"
#import "WebCoreBridge.h"

NSString *WebCoreFileButtonFilenameChanged = @"WebCoreFileButtonFilenameChanged";
NSString *WebCoreFileButtonClicked = @"WebCoreFileButtonClicked";


@interface KWQFileButtonAdapter : NSObject
{
    KWQFileButton *button;
}

- initWithKWQFileButton:(KWQFileButton *)button;

@end

KWQFileButton::KWQFileButton(KHTMLPart *part)
    : QWidget([KWQ(part)->bridge() fileButton])
    , _clicked(this, SIGNAL(clicked()))
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
    [(NSView <WebCoreFileButton> *)getView() setFilename:f.getNSString()];
}

QSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);
    return QSize([(NSView <WebCoreFileButton> *)getView() bestVisualFrameSizeForCharacterCount:characters]);
}

QRect KWQFileButton::frameGeometry() const
{
    return QRect([(NSView <WebCoreFileButton> *)getView() visualFrame]);
}

void KWQFileButton::setFrameGeometry(const QRect &rect)
{
    [(NSView <WebCoreFileButton> *)getView() setVisualFrame:rect];
}

int KWQFileButton::baselinePosition() const
{
    NSView <WebCoreFileButton> *button = (NSView <WebCoreFileButton> *)getView();
    return (int)(NSMaxY([button frame]) - [button baseline] - [button visualFrame].origin.y);
}

void KWQFileButton::filenameChanged()
{
    _textChanged.call(QString::fromNSString([(NSView <WebCoreFileButton> *)getView() filename]));
}

void KWQFileButton::clicked()
{
    _clicked.call();
}


@implementation KWQFileButtonAdapter

- initWithKWQFileButton:(KWQFileButton *)b
{
    [super init];
    button = b;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(filenameChanged:)
        name:WebCoreFileButtonFilenameChanged object:b->getView()];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(clicked:)
        name:WebCoreFileButtonClicked object:b->getView()];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)filenameChanged:(NSNotification *)notification
{
    button->filenameChanged();
}

-(void)clicked:(NSNotification *)notification
{
    button->sendConsumedMouseUp();
    button->clicked();
}

@end
