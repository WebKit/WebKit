/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <KWQInvisibleButton.h>

#import <qevent.h>
#import <khtmlview.h>
#import <render_form.h>
#import <dom2_eventsimpl.h>

@interface KWQInvisibleButtonView : NSView
{
    khtml::RenderImageButton *imageButton;
}

-(void)setImageButton:(khtml::RenderImageButton *)theImageButton;

@end


@implementation KWQInvisibleButtonView

-(void)setImageButton:(khtml::RenderImageButton *)theImageButton
{
    imageButton = theImageButton;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    DOM::HTMLFormElementImpl *element;
    
    element = static_cast<DOM::HTMLGenericFormElementImpl *>(imageButton->element())->form();
    
    for (QPtrListIterator<DOM::HTMLGenericFormElementImpl> it(element->formElements); it.current(); ++it) {
        DOM::HTMLGenericFormElementImpl* current = it.current();
        if (current->renderer() && 
            (strcmp(current->renderer()->renderName(), "RenderLineEdit") == 0 || strcmp(current->renderer()->renderName(), "RenderTextArea") == 0)) {
            static_cast<khtml::RenderWidget *>(current->renderer())->widget()->endEditing();
            break;
        }
    }
}

- (void)mouseUp:(NSEvent *)theEvent
{
    NSPoint mouse = [theEvent locationInWindow];
    QMouseEvent e2(QEvent::MouseButtonRelease, QPoint((int)mouse.x, (int)mouse.y), 0, 0);
    imageButton->element()->dispatchMouseEvent(&e2, DOM::EventImpl::KHTML_CLICK_EVENT, 1);
}

@end

KWQInvisibleButton::KWQInvisibleButton(khtml::RenderImageButton *theImageButton)
{
    imageButton = theImageButton;
    buttonView = nil;
}

KWQInvisibleButton::~KWQInvisibleButton()
{
    [buttonView removeFromSuperview];
    [buttonView release];
}

void KWQInvisibleButton::setFrameInView(int x, int y, int w, int h, KHTMLView *khtmlview)
{
    if (!buttonView) {
        buttonView = [[KWQInvisibleButtonView alloc] init];
        [buttonView setImageButton:imageButton];
        NSView *nsview = khtmlview->getView();    
        if ([nsview isKindOfClass:[NSScrollView class]]) {
            NSScrollView *scrollView = (NSScrollView *)nsview;
            nsview = [scrollView documentView];
        }
        [nsview addSubview:buttonView];
    }
    [buttonView setFrame:NSMakeRect(x, y, w, h)];
}
