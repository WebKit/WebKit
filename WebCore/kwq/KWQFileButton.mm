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

#import "KWQExceptions.h"
#import "FoundationExtras.h"
#import "MacFrame.h"
#import "WebCoreFrameBridge.h"
#import "render_form.h"
#import <kxmlcore/Assertions.h>

using namespace WebCore;

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

KWQFileButton::KWQFileButton(Frame *frame)
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged(const QString &)))
    , _adapter(0)
{
    KWQ_BLOCK_EXCEPTIONS;

    _adapter = KWQRetainNSRelease([[KWQFileButtonAdapter alloc] initWithKWQFileButton:this]);
    setView([Mac(frame)->bridge() fileButtonWithDelegate:_adapter]);

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

IntSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);
    NSView <WebCoreFileButton> *button = getView();

    NSSize size = {0,0};
    KWQ_BLOCK_EXCEPTIONS;
    size = [button bestVisualFrameSizeForCharacterCount:characters];
    return IntSize(size);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntSize(0, 0);
}

IntRect KWQFileButton::frameGeometry() const
{
    NSView <WebCoreFileButton> *button = getView();

    KWQ_BLOCK_EXCEPTIONS;
    return enclosingIntRect([button visualFrame]);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntRect();
}

void KWQFileButton::setFrameGeometry(const IntRect &rect)
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

Widget::FocusPolicy KWQFileButton::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    WebCoreFrameBridge *bridge = MacFrame::bridgeForWidget(this);
    if (!bridge || ![bridge impl] || ![bridge impl]->tabsToAllControls()) {
        return NoFocus;
    }
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return Widget::focusPolicy();
}

void KWQFileButton::filenameChanged(const QString &filename)
{
    _textChanged.call(filename);
}

void KWQFileButton::focusChanged(bool nowHasFocus)
{
    if (nowHasFocus) {
        if (!MacFrame::currentEventIsMouseDownInWidget(this)) {
            RenderWidget *widget = const_cast<RenderWidget *> (static_cast<const RenderWidget *>(eventFilterObject()));
            RenderLayer *layer = widget->enclosingLayer();
            layer = layer->renderer()->enclosingLayer();
            if (layer)
                layer->scrollRectToVisible(widget->absoluteBoundingBoxRect());
        }        
        eventFilterObject()->eventFilterFocusIn();
    } else
        eventFilterObject()->eventFilterFocusOut();
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
