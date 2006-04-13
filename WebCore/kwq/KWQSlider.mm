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

#import "config.h"
#import "KWQSlider.h"

#import "BlockExceptions.h"
#import "KWQLineEdit.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreWidgetHolder.h"
#import "render_form.h"

using namespace std;
using namespace WebCore;

@interface KWQSlider : NSSlider <WebCoreWidgetHolder>
{
    QSlider* slider;
    BOOL inNextValidKeyView;
}

- (id)initWithQSlider:(QSlider*)s;
- (void)detachQSlider;

@end

@implementation KWQSlider

- (id)initWithQSlider:(QSlider*)s
{
    self = [self init];

    slider = s;

    [self setTarget:self];
    [self setAction:@selector(slide:)];
    [self setContinuous:YES]; // Our sliders are always continuous by default.
    [self setMinValue:0.0];
    [self setMaxValue:100.0];
    [self setDoubleValue:50.0];

    return self;
}

- (void)detachQSlider
{
    [self setTarget:nil];
    slider = 0;
}

- (void)mouseDown:(NSEvent *)event
{
    Widget::beforeMouseDown(self);
    [super mouseDown:event];
    Widget::afterMouseDown(self);
    if (slider)
        slider->sendConsumedMouseUp();
    if (slider && slider->client())
        slider->client()->clicked(slider);
}

- (IBAction)slide:(NSSlider*)sender
{
    if (slider)
        slider->sliderValueChanged();
}

- (Widget *)widget
{
    return slider;
}

// FIXME: All the firstResponder and keyView code here is replicated in KWQButton and
// other KWQ classes. We should find a way to share this code.
- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become && slider) {
        if (slider && slider->client() && !FrameMac::currentEventIsMouseDownInWidget(slider))
            slider->client()->scrollToVisible(slider);
        if (slider && slider->client())
            slider->client()->focusIn(slider);
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && slider && slider->client()) {
        slider->client()->focusOut(slider);
        if (slider)
            [FrameMac::bridgeForWidget(slider) formControlIsResigningFirstResponder:self];
    }
    return resign;
}

-(NSView *)nextKeyView
{
    NSView *view = nil;
    if (slider && inNextValidKeyView) {
        // resign so we send a blur before setting focus on
        // the next widget, otherwise the blur for this
        // widget will remove focus from the widget after
        // we tab to it
        [self resignFirstResponder];
        if (slider) {
            view = FrameMac::nextKeyViewForWidget(slider, KWQSelectingNext);
        } else {
            view = [super nextKeyView];
        }
    } else { 
        view = [super nextKeyView];
    }
    return view;
}

-(NSView *)previousKeyView
{
    NSView *view = nil;
    if (slider && inNextValidKeyView) {
        // resign so we send a blur before setting focus on
        // the next widget, otherwise the blur for this
        // widget will remove focus from the widget after
        // we tab to it
        [self resignFirstResponder];
        if (slider) {
            view = FrameMac::nextKeyViewForWidget(slider, KWQSelectingPrevious);
        } else {
            view = [super previousKeyView];
        }
    } else { 
        view = [super previousKeyView];
    }
    return view;
}

- (BOOL)canBecomeKeyView
{
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    if (slider && !FrameMac::frameForWidget(slider)->tabsToAllControls()) {
        return NO;
    }
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

-(NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

-(NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

@end

enum {
    dimWidth,
    dimHeight
};

QSlider::QSlider()
    : m_minVal(0.0), m_maxVal(100.0), m_val(50.0)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQSlider* slider = [[KWQSlider alloc] initWithQSlider:this];
    [[slider cell] setControlSize:NSSmallControlSize];
    setView(slider);
    [slider release];
    END_BLOCK_OBJC_EXCEPTIONS;
}

QSlider::~QSlider()
{
    KWQSlider* slider = (KWQSlider*)getView();
    [slider detachQSlider];
}

void QSlider::setFont(const Font& f)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    Widget::setFont(f);
    
    const NSControlSize size = KWQNSControlSizeForFont(f);    
    NSControl * const slider = static_cast<NSControl *>(getView());
    [[slider cell] setControlSize:size];
    [slider setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

Widget::FocusPolicy QSlider::focusPolicy() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(this);
    if (!bridge || ![bridge impl] || ![bridge impl]->tabsToAllControls()) {
        return NoFocus;
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return Widget::focusPolicy();
}

IntSize QSlider::sizeHint() const 
{
    return IntSize(dimensions()[dimWidth], dimensions()[dimHeight]);
}

void QSlider::setValue(double v)
{
    double val = max(m_minVal, min(v, m_maxVal));
    
    KWQSlider* slider = (KWQSlider*)getView();
    [slider setDoubleValue: val];
    m_val = val;
}

void QSlider::setMinValue(double v)
{
    if (v == m_minVal) return;

    KWQSlider* slider = (KWQSlider*)getView();
    [slider setMinValue: v];
    m_minVal = v;
}

void QSlider::setMaxValue(double v)
{
    if (v == m_maxVal) return;

    KWQSlider* slider = (KWQSlider*)getView();
    [slider setMaxValue: v];
    m_maxVal = v;
}

double QSlider::value() const
{
    return m_val;
}

double QSlider::minValue() const
{
    return m_minVal;
}

double QSlider::maxValue() const
{
    return m_maxVal;
}

void QSlider::sliderValueChanged()
{
    KWQSlider* slider = (KWQSlider*)getView();
    double v = [slider doubleValue];
    if (m_val != v) {
        m_val = v;
        if (client())
            client()->valueChanged(this);
    }
}

const int* QSlider::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][2] = {
        { 129, 21 },
        { 129, 15 },
        { 129, 12 },
    };
    NSControl * const slider = static_cast<NSControl *>(getView());
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return w[[[slider cell] controlSize]];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return w[NSSmallControlSize];
}
