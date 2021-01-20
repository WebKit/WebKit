/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateTimeChooser.h"
#include "DateTimeChooserClient.h"
#include "DateTimeEditElement.h"
#include "DateTimeFormat.h"
#include "InputType.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class DateComponents;

// A super class of date, datetime, datetime-local, month, time, and week types.
class BaseDateAndTimeInputType : public InputType, private DateTimeChooserClient, private DateTimeEditElement::EditControlOwner {
protected:
    enum class DateTimeFormatValidationResults : uint8_t {
        HasYear = 1 << 0,
        HasMonth = 1 << 1,
        HasWeek = 1 << 2,
        HasDay = 1 << 3,
        HasHour = 1 << 4,
        HasMinute = 1 << 5,
        HasSecond = 1 << 6,
        HasMeridiem = 1 << 7,
    };

    BaseDateAndTimeInputType(HTMLInputElement& element) : InputType(element) { }
    ~BaseDateAndTimeInputType();

    Decimal parseToNumber(const String&, const Decimal&) const override;
    String serialize(const Decimal&) const override;
    String serializeWithComponents(const DateComponents&) const;

    bool shouldHaveSecondField(const DateComponents&) const;
    bool shouldHaveMillisecondField(const DateComponents&) const;

private:
    class DateTimeFormatValidator final : public DateTimeFormat::TokenHandler {
    public:
        DateTimeFormatValidator() { }

        void visitField(DateTimeFormat::FieldType, int);
        void visitLiteral(const String&) { }

        bool validateFormat(const String& format, const BaseDateAndTimeInputType&);

    private:
        OptionSet<DateTimeFormatValidationResults> m_results;
    };

    virtual Optional<DateComponents> parseToDateComponents(const StringView&) const = 0;
    virtual Optional<DateComponents> setMillisecondToDateComponents(double) const = 0;
    virtual void setupLayoutParameters(DateTimeEditElement::LayoutParameters&, const DateComponents&) const = 0;
    virtual bool isValidFormat(OptionSet<DateTimeFormatValidationResults>) const = 0;
    virtual String serializeWithMilliseconds(double) const;

    // InputType functions:
    String visibleValue() const final;
    String sanitizeValue(const String&) const final;
    void setValue(const String&, bool valueChanged, TextFieldEventBehavior) final;
    double valueAsDate() const override;
    ExceptionOr<void> setValueAsDate(double) const override;
    double valueAsDouble() const final;
    ExceptionOr<void> setValueAsDecimal(const Decimal&, TextFieldEventBehavior) const final;
    bool typeMismatchFor(const String&) const final;
    bool typeMismatch() const final;
    bool valueMissing(const String&) const final;
    Decimal defaultValueForStepUp() const override;
    bool isSteppable() const final;
    String localizeValue(const String&) const final;
    bool supportsReadOnly() const final;
    bool shouldRespectListAttribute() final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isMouseFocusable() const final;

    void handleDOMActivateEvent(Event&) override;
    void createShadowSubtree() final;
    void destroyShadowSubtree() final;
    void updateInnerTextValue() final;
    bool hasCustomFocusLogic() const final;
    void attributeChanged(const QualifiedName&) final;
    bool isPresentingAttachedView() const final;
    void elementDidBlur() final;
    void detach() final;

    ShouldCallBaseEventHandler handleKeydownEvent(KeyboardEvent&) override;
    void handleKeypressEvent(KeyboardEvent&) override;
    void handleKeyupEvent(KeyboardEvent&) override;
    void handleFocusEvent(Node* oldFocusedNode, FocusDirection) final;
    bool accessKeyAction(bool sendMouseEvents) override;

    // DateTimeEditElement::EditControlOwner functions:
    void didBlurFromControl() final;
    void didChangeValueFromControl() final;
    bool isEditControlOwnerDisabled() const final;
    bool isEditControlOwnerReadOnly() const final;
    AtomString localeIdentifier() const final;

    // DateTimeChooserClient functions:
    void didChooseValue(StringView) final;
    void didEndChooser() final;

    void closeDateTimeChooser();

    std::unique_ptr<DateTimeChooser> m_dateTimeChooser;
    RefPtr<DateTimeEditElement> m_dateTimeEditElement;
};

} // namespace WebCore

#endif
