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

#import "KWQButton.h"
#import "KWQSlider.h"

#import "KWQExceptions.h"

@interface KWQSlider : NSSlider
{
    QSlider* slider;
}

- (id)initWithQSlider:(QSlider*)s;

@end

@implementation KWQSlider

- (id)initWithQSlider:(QSlider*)s
{
    slider = s;

    id result = [self init];
    [self setTarget:self];
    [self setAction:@selector(slide:)];
    [self setContinuous: YES]; // Our sliders are always continuous by default.
    [self setMinValue: 0.0];
    [self setMaxValue: 100.0];
    [self setDoubleValue: 50.0];
    return result;
}

-(IBAction)slide:(NSSlider*)sender
{
    slider->sliderValueChanged();
}
@end

enum {
    dimWidth,
    dimHeight
};

QSlider::QSlider()
:m_sliderValueChanged(this, SIGNAL(sliderValueChanged())), m_minVal(0.0), m_maxVal(100.0), m_val(50.0)
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQSlider* slider = [[KWQSlider alloc] initWithQSlider:this];
    [[slider cell] setControlSize:NSSmallControlSize];
    setView(slider);
    [slider release];
    KWQ_UNBLOCK_EXCEPTIONS;
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

QSize QSlider::sizeHint() const 
{
    return QSize(dimensions()[dimWidth], dimensions()[dimHeight]);
}

void QSlider::setValue(double v)
{
    double val = kMax(m_minVal, kMin(v, m_maxVal));
    
    KWQSlider* slider = (KWQSlider*)getView();
    [slider setDoubleValue: val];
    
    sliderValueChanged(); // Emit the signal that indicates our value has changed.
}

void QSlider::setMinValue(double v)
{
    if (v == m_minVal) return;

    KWQSlider* slider = (KWQSlider*)getView();
    [slider setMinValue: v];
}

void QSlider::setMaxValue(double v)
{
    if (v == m_maxVal) return;

    KWQSlider* slider = (KWQSlider*)getView();
    [slider setMaxValue: v];
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
