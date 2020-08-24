/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "BaseClickableWithKeyInputType.h"
#include "BaseDateAndTimeInputType.h"
#include "DateTimeChooser.h"
#include "DateTimeChooserClient.h"

namespace WebCore {

class BaseChooserOnlyDateAndTimeInputType : public BaseDateAndTimeInputType, private DateTimeChooserClient {
protected:
    explicit BaseChooserOnlyDateAndTimeInputType(HTMLInputElement& element) : BaseDateAndTimeInputType(element) { }
    ~BaseChooserOnlyDateAndTimeInputType();

private:
    void updateInnerTextValue() override;
    void closeDateTimeChooser();

    // InputType functions:
    void createShadowSubtree() override;
    void detach() override;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) override;
    void handleDOMActivateEvent(Event&) override;
    void elementDidBlur() final;
    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) override;
    void handleKeypressEvent(KeyboardEvent&) override;
    void handleKeyupEvent(KeyboardEvent&) override;
    bool accessKeyAction(bool sendMouseEvents) override;
    bool isMouseFocusable() const override;
    bool isPresentingAttachedView() const final;
    void attributeChanged(const QualifiedName&) override;

    // DateTimeChooserClient functions:
    void didChooseValue(StringView) final;
    void didEndChooser() final;

    std::unique_ptr<DateTimeChooser> m_dateTimeChooser;
};

} // namespace WebCore

#endif
