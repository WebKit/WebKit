/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef AccessibilityMediaControls_h
#define AccessibilityMediaControls_h

#if ENABLE(VIDEO)

#include "AccessibilitySlider.h"
#include "MediaControlElements.h"

namespace WebCore {

class AccessibilityMediaControl : public AccessibilityRenderObject {

public:
    static PassRefPtr<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaControl() { }

    virtual AccessibilityRole roleValue() const OVERRIDE;

    virtual String title() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;
    virtual String helpText() const OVERRIDE;

protected:
    explicit AccessibilityMediaControl(RenderObject*);
    MediaControlElementType controlType() const;
    String controlTypeName() const;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;

private:
    virtual void accessibilityText(Vector<AccessibilityText>&) OVERRIDE;
};


class AccessibilityMediaTimeline : public AccessibilitySlider {

public:
    static PassRefPtr<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeline() { }

    virtual bool isMediaTimeline() const OVERRIDE { return true; }

    virtual String helpText() const OVERRIDE;
    virtual String valueDescription() const OVERRIDE;
    const AtomicString& getAttribute(const QualifiedName& attribute) const;

private:
    explicit AccessibilityMediaTimeline(RenderObject*);
};


class AccessibilityMediaControlsContainer : public AccessibilityMediaControl {

public:
    static PassRefPtr<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaControlsContainer() { }

    virtual AccessibilityRole roleValue() const OVERRIDE { return ToolbarRole; }

    virtual String helpText() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;

private:
    explicit AccessibilityMediaControlsContainer(RenderObject*);
    bool controllingVideoElement() const;
    const String elementTypeName() const;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


class AccessibilityMediaTimeDisplay : public AccessibilityMediaControl {

public:
    static PassRefPtr<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeDisplay() { }

    virtual AccessibilityRole roleValue() const OVERRIDE { return ApplicationTimerRole; }

    virtual String stringValue() const OVERRIDE;
    virtual String accessibilityDescription() const OVERRIDE;

private:
    explicit AccessibilityMediaTimeDisplay(RenderObject*);
    virtual bool isMediaControlLabel() const OVERRIDE { return true; }
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
};


} // namespace WebCore

#endif // ENABLE(VIDEO)

#endif // AccessibilityMediaControls_h
