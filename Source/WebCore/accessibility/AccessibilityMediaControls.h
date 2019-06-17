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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#if ENABLE(VIDEO)

#include "AccessibilitySlider.h"
#include "MediaControlElements.h"

namespace WebCore {

class AccessibilityMediaControl : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaControl() = default;

    AccessibilityRole roleValue() const override;

    String title() const override;
    String accessibilityDescription() const override;
    String helpText() const override;

protected:
    explicit AccessibilityMediaControl(RenderObject*);
    MediaControlElementType controlType() const;
    const String& controlTypeName() const;
    bool computeAccessibilityIsIgnored() const override;

private:
    void accessibilityText(Vector<AccessibilityText>&) const override;
};


class AccessibilityMediaTimeline final : public AccessibilitySlider {
public:
    static Ref<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeline() = default;

    String helpText() const override;
    String valueDescription() const override;
    const AtomString& getAttribute(const QualifiedName& attribute) const;

private:
    explicit AccessibilityMediaTimeline(RenderObject*);

    bool isMediaTimeline() const override { return true; }
};


class AccessibilityMediaControlsContainer final : public AccessibilityMediaControl {
public:
    static Ref<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaControlsContainer() = default;

    AccessibilityRole roleValue() const override { return AccessibilityRole::Toolbar; }

    String helpText() const override;
    String accessibilityDescription() const override;

private:
    explicit AccessibilityMediaControlsContainer(RenderObject*);
    bool controllingVideoElement() const;
    const String& elementTypeName() const;
    bool computeAccessibilityIsIgnored() const override;
};


class AccessibilityMediaTimeDisplay final : public AccessibilityMediaControl {
public:
    static Ref<AccessibilityObject> create(RenderObject*);
    virtual ~AccessibilityMediaTimeDisplay() = default;

    AccessibilityRole roleValue() const override { return AccessibilityRole::ApplicationTimer; }

    String stringValue() const override;
    String accessibilityDescription() const override;

private:
    explicit AccessibilityMediaTimeDisplay(RenderObject*);
    bool isMediaControlLabel() const override { return true; }
    bool computeAccessibilityIsIgnored() const override;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
