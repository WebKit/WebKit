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
#import "KWQExceptions.h"
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
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged(const QString &)))
    , _adapter(0)
{
    KWQ_BLOCK_NS_EXCEPTIONS;

    setView([KWQ(part)->bridge() fileButton]);
    _adapter = [[KWQFileButtonAdapter alloc] initWithKWQFileButton:this];

    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

KWQFileButton::~KWQFileButton()
{
    _adapter->button = 0;
    KWQ_BLOCK_NS_EXCEPTIONS;
    [_adapter release];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}
    
void KWQFileButton::setFilename(const QString &f)
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [button setFilename:f.getNSString()];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

QSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);
    NSView <WebCoreFileButton> *button = getView();

    NSSize size = {0,0};
    KWQ_BLOCK_NS_EXCEPTIONS;
    size = [button bestVisualFrameSizeForCharacterCount:characters];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return QSize(size);
}

QRect KWQFileButton::frameGeometry() const
{
    NSView <WebCoreFileButton> *button = getView();

    NSRect frame = {{0,0},{0,0}};
    KWQ_BLOCK_NS_EXCEPTIONS;
    frame = [button visualFrame];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return QRect(frame);
}

void KWQFileButton::setFrameGeometry(const QRect &rect)
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [button setVisualFrame:rect];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

int KWQFileButton::baselinePosition() const
{
    NSView <WebCoreFileButton> *button = getView();

    volatile int position = 0;
    KWQ_BLOCK_NS_EXCEPTIONS;
    position = (int)([button frame].origin.y + [button baseline] - [button visualFrame].origin.y);
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return position;
}

void KWQFileButton::filenameChanged()
{
    NSView <WebCoreFileButton> *button = getView();

    QString filename;
    KWQ_BLOCK_NS_EXCEPTIONS;
    filename = QString::fromNSString([button filename]);
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    _textChanged.call();
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
