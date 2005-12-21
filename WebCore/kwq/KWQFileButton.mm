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

#include "config.h"
#import "KWQFileButton.h"

#import <kxmlcore/Assertions.h>
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQFoundationExtras.h"
#import "WebCoreBridge.h"
#import "render_form.h"

using khtml::RenderWidget;
using khtml::RenderLayer;

@interface KWQFileButtonAdapter : NSObject <WebCoreFileButtonDelegate>
{
@public
    KWQFileButton *button;
}

- initWithKWQFileButton:(KWQFileButton *)button;

- (void)filenameChanged:(NSString *)filename;
- (void)focusChanged:(BOOL)nowHasFocus;
- (void)clicked;

@end

KWQFileButton::KWQFileButton(KHTMLPart *part)
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged(const QString &)))
    , _adapter(0)
{
    KWQ_BLOCK_EXCEPTIONS;

    _adapter = KWQRetainNSRelease([[KWQFileButtonAdapter alloc] initWithKWQFileButton:this]);
    setView([KWQ(part)->bridge() fileButtonWithDelegate:_adapter]);

    KWQ_UNBLOCK_EXCEPTIONS;
}

KWQFileButton::~KWQFileButton()
{
    _adapter->button = 0;
    KWQ_BLOCK_EXCEPTIONS;
    CFRelease(_adapter);
    KWQ_UNBLOCK_EXCEPTIONS;
}
    
void KWQFileButton::setFilename(const QString &f)
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_EXCEPTIONS;
    [button setFilename:f.getNSString()];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void KWQFileButton::click(bool sendMouseEvents)
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_EXCEPTIONS;
    [button performClick];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);
    NSView <WebCoreFileButton> *button = getView();

    NSSize size = {0,0};
    KWQ_BLOCK_EXCEPTIONS;
    size = [button bestVisualFrameSizeForCharacterCount:characters];
    return QSize(size);
    KWQ_UNBLOCK_EXCEPTIONS;
    return QSize(0, 0);
}

QRect KWQFileButton::frameGeometry() const
{
    NSView <WebCoreFileButton> *button = getView();

    NSRect frame = {{0,0},{0,0}};
    KWQ_BLOCK_EXCEPTIONS;
    frame = [button visualFrame];
    return QRect(frame);
    KWQ_UNBLOCK_EXCEPTIONS;
    return QRect();
}

void KWQFileButton::setFrameGeometry(const QRect &rect)
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_EXCEPTIONS;
    [button setVisualFrame:rect];
    KWQ_UNBLOCK_EXCEPTIONS;
}

int KWQFileButton::baselinePosition(int height) const
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_EXCEPTIONS;
    return (int)([button frame].origin.y + [button baseline] - [button visualFrame].origin.y);
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

QWidget::FocusPolicy KWQFileButton::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(this);
    if (!bridge || ![bridge part] || ![bridge part]->tabsToAllControls()) {
        return NoFocus;
    }
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return QWidget::focusPolicy();
}

void KWQFileButton::filenameChanged(const QString &filename)
{
    _textChanged.call(filename);
}

void KWQFileButton::focusChanged(bool nowHasFocus)
{
    if (nowHasFocus) {
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(this)) {
            RenderWidget *widget = const_cast<RenderWidget *> (static_cast<const RenderWidget *>(eventFilterObject()));
            RenderLayer *layer = widget->enclosingLayer();
            layer = layer->renderer()->enclosingLayer();
            if (layer)
                layer->scrollRectToVisible(widget->absoluteBoundingBoxRect());
        }        
        QFocusEvent event(QEvent::FocusIn);
        const_cast<QObject *>(eventFilterObject())->eventFilter(this, &event);
    }
    else {
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(eventFilterObject())->eventFilter(this, &event);
    }
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
    return self;
}

- (void)filenameChanged:(NSString *)filename
{
    if (button) {
        button->filenameChanged(QString::fromNSString(filename));
    }
}

- (void)focusChanged:(BOOL)nowHasFocus
{
    if (button) {
        button->focusChanged(nowHasFocus);
    }
}

-(void)clicked
{
    if (button) {
        button->sendConsumedMouseUp();
        button->clicked();
    }
}

@end
