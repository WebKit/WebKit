/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INPUT_TYPE_COLOR)

#include "BaseClickableWithKeyInputType.h"
#include "ColorChooser.h"
#include "ColorChooserClient.h"

namespace WebCore {

class ColorInputType final : public BaseClickableWithKeyInputType, private ColorChooserClient {
public:
    explicit ColorInputType(HTMLInputElement& element) : BaseClickableWithKeyInputType(element) { }
    virtual ~ColorInputType();

private:
    void didChooseColor(const Color&) final;
    void didEndChooser() final;
    IntRect elementRectRelativeToRootView() const final;
    Vector<Color> suggestedColors() const final;
    bool isMouseFocusable() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isColorControl() const final;
    bool isPresentingAttachedView() const final;
    const AtomString& formControlType() const final;
    bool supportsRequired() const final;
    String fallbackValue() const final;
    String sanitizeValue(const String&) const final;
    void createShadowSubtree() final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) final;
    void handleDOMActivateEvent(Event&) final;
    void detach() final;
    void elementDidBlur() final;
    bool shouldRespectListAttribute() final;
    bool typeMismatchFor(const String&) const final;
    bool shouldResetOnDocumentActivation() final;
    Color valueAsColor() const final;
    void selectColor(StringView) final;

    void endColorChooser();
    void updateColorSwatch();
    HTMLElement* shadowColorSwatch() const;

    std::unique_ptr<ColorChooser> m_chooser;
};

} // namespace WebCore

#endif // ENABLE(INPUT_TYPE_COLOR)
