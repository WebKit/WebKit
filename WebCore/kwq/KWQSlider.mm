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

#include "config.h"
#import "KWQSlider.h"

#import "KWQLineEdit.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQView.h"
#import "WebCoreBridge.h"
#import "render_form.h"

using khtml::RenderWidget;
using khtml::RenderLayer;

@interface KWQSlider : NSSlider <KWQWidgetHolder>
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
    QWidget::beforeMouseDown(self);
    [super mouseDown:event];
    QWidget::afterMouseDown(self);
    if (slider) {
        slider->sendConsumedMouseUp();
    }
    if (slider) {
        slider->clicked();
    }
}

- (IBAction)slide:(NSSlider*)sender
{
    if (slider) {
        slider->sliderValueChanged();
    }
}

- (QWidget *)widget
{
    return slider;
}

// FIXME: All the firstResponder and keyView code here is replicated in KWQButton and
// other KWQ classes. We should find a way to share this code.
- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become && slider) {
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(slider)) {
            RenderWidget *widget = const_cast<RenderWidget *> (static_cast<const RenderWidget *>(slider->eventFilterObject()));
            RenderLayer *layer = widget->enclosingLayer();
            if (layer)
                layer->scrollRectToVisible(widget->absoluteBoundingBoxRect());
        }

        if (slider) {
            QFocusEvent event(QEvent::FocusIn);
            if (slider->eventFilterObject())
                const_cast<QObject *>(slider->eventFilterObject())->eventFilter(slider, &event);
        }
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && slider) {
        QFocusEvent event(QEvent::FocusOut);
        if (slider->eventFilterObject()) {
            const_cast<QObject *>(slider->eventFilterObject())->eventFilter(slider, &event);
            [KWQKHTMLPart::bridgeForWidget(slider) formControlIsResigningFirstResponder:self];
        }
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
            view = KWQKHTMLPart::nextKeyViewForWidget(slider, KWQSelectingNext);
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
            view = KWQKHTMLPart::nextKeyViewForWidget(slider, KWQSelectingPrevious);
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
    if (slider && !KWQKHTMLPart::partForWidget(slider)->tabsToAllControls()) {
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
: m_sliderValueChanged(this, SIGNAL(sliderValueChanged())), 
  m_clicked(this, SIGNAL(clicked())),
  m_minVal(0.0), m_maxVal(100.0), m_val(50.0)
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQSlider* slider = [[KWQSlider alloc] initWithQSlider:this];
    [[slider cell] setControlSize:NSSmallControlSize];
    setView(slider);
    [slider release];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QSlider::~QSlider()
{
    KWQSlider* slider = (KWQSlider*)getView();
    [slider detachQSlider];
}

void QSlider::setFont(const QFont &f)
{
    KWQ_BLOCK_EXCEPTIONS;
    
    QWidget::setFont(f);
    
    const NSControlSize size = KWQNSControlSizeForFont(f);    
    NSControl * const slider = static_cast<NSControl *>(getView());
    [[slider cell] setControlSize:size];
    [slider setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    
    KWQ_UNBLOCK_EXCEPTIONS;
}

QWidget::FocusPolicy QSlider::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(this);
    if (!bridge || ![bridge part] || ![bridge part]->tabsToAllControls()) {
        return NoFocus;
    }
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return QWidget::focusPolicy();
}

QSize QSlider::sizeHint() const 
{
    return QSize(dimensions()[dimWidth], dimensions()[dimHeight]);
}

void QSlider::setValue(double v)
{
    double val = kMax(m_minVal, kMin(v, m_maxVal));
    
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
        m_sliderValueChanged.call();
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
    
    KWQ_BLOCK_EXCEPTIONS;
    return w[[[slider cell] controlSize]];
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return w[NSSmallControlSize];
}

void QSlider::clicked()
{
    m_clicked.call();
}
