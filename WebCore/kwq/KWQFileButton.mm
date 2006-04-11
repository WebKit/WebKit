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
#import "render_form.h"

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
    : _adapter(0)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    _adapter = KWQRetainNSRelease([[KWQFileButtonAdapter alloc] initWithKWQFileButton:this]);
    setView([Mac(frame)->bridge() fileButtonWithDelegate:_adapter]);

    END_BLOCK_OBJC_EXCEPTIONS;
}

KWQFileButton::~KWQFileButton()
{
    _adapter->button = 0;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    CFRelease(_adapter);
    END_BLOCK_OBJC_EXCEPTIONS;
}
    
void KWQFileButton::setFilename(const DeprecatedString &f)
{
    NSView <WebCoreFileButton> *button = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [button setFilename:f.getNSString()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void KWQFileButton::click(bool sendMouseEvents)
{
    NSView <WebCoreFileButton> *button = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [button performClick];
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntSize KWQFileButton::sizeForCharacterWidth(int characters) const
{
    ASSERT(characters > 0);
    NSView <WebCoreFileButton> *button = getView();

    NSSize size = {0,0};
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    size = [button bestVisualFrameSizeForCharacterCount:characters];
    return IntSize(size);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize(0, 0);
}

IntRect KWQFileButton::frameGeometry() const
{
    NSView <WebCoreFileButton> *button = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return enclosingIntRect([button visualFrame]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

void KWQFileButton::setFrameGeometry(const IntRect &rect)
{
    NSView <WebCoreFileButton> *button = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [button setVisualFrame:rect];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int KWQFileButton::baselinePosition(int height) const
{
    NSView <WebCoreFileButton> *button = getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return (int)([button frame].origin.y + [button baseline] - [button visualFrame].origin.y);
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

@implementation KWQFileButtonAdapter

- initWithKWQFileButton:(KWQFileButton *)b
{
    [super init];
    button = b;
    return self;
}

- (void)filenameChanged:(NSString *)filename
{
    if (button)
        button->filenameChanged(DeprecatedString::fromNSString(filename));
}

- (void)focusChanged:(BOOL)nowHasFocus
{
    if (nowHasFocus) {
        if (button && button->client() && !FrameMac::currentEventIsMouseDownInWidget(button))
            button->client()->scrollToVisible(button);
        if (button && button->client())
            button->client()->focusIn(button);
    } else {
        if (button && button->client())
            button->client()->focusOut(button);
    }
}

-(void)clicked
{
    if (button)
        button->sendConsumedMouseUp();
    if (button && button->client())
        button->client()->clicked(button);
}

@end
