/*
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NumberInputType.h"

#include "BeforeTextInsertedEvent.h"
#include "ContainerNodeInlines.h"
#include "Decimal.h"
#include "ElementInlines.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "NodeInlines.h"
#include "NodeName.h"
#include "PlatformLocale.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderObjectInlines.h"
#include "RenderTextControl.h"
#include "StepRange.h"
#include <limits>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NumberInputType);

using namespace HTMLNames;

static const int numberDefaultStep = 1;
static const int numberDefaultStepBase = 0;
static const int numberStepScaleFactor = 1;

struct RealNumberRenderSize {
    unsigned sizeBeforeDecimalPoint;
    unsigned sizeAfteDecimalPoint;

    RealNumberRenderSize max(const RealNumberRenderSize& other) const
    {
        return {
            std::max(sizeBeforeDecimalPoint, other.sizeBeforeDecimalPoint),
            std::max(sizeAfteDecimalPoint, other.sizeAfteDecimalPoint)
        };
    }
};

static RealNumberRenderSize calculateRenderSize(const Decimal& value)
{
    ASSERT(value.isFinite());
    const unsigned sizeOfDigits = String::number(value.value().coefficient()).length();
    const unsigned sizeOfSign = value.isNegative() ? 1 : 0;
    const int exponent = value.exponent();
    if (exponent >= 0)
        return { sizeOfSign + sizeOfDigits, 0 };

    const int sizeBeforeDecimalPoint = exponent + sizeOfDigits;
    if (sizeBeforeDecimalPoint > 0) {
        // In case of "123.456"
        return { sizeOfSign + sizeBeforeDecimalPoint, sizeOfDigits - sizeBeforeDecimalPoint };
    }

    // In case of "0.00012345"
    const unsigned sizeOfZero = 1;
    const unsigned numberOfZeroAfterDecimalPoint = -sizeBeforeDecimalPoint;
    return { sizeOfSign + sizeOfZero , numberOfZeroAfterDecimalPoint + sizeOfDigits };
}

const AtomString& NumberInputType::formControlType() const
{
    return InputTypeNames::number();
}

void NumberInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    ASSERT(element());
    if (!valueChanged && sanitizedValue.isEmpty() && !protectedElement()->innerTextValue().isEmpty())
        updateInnerTextValue();
    TextFieldInputType::setValue(sanitizedValue, valueChanged, eventBehavior, selection);
}

double NumberInputType::valueAsDouble() const
{
    ASSERT(element());
    return parseToDoubleForNumberType(protectedElement()->value().get());
}

ExceptionOr<void> NumberInputType::setValueAsDouble(double newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    protectedElement()->setValue(serializeForNumberType(newValue), eventBehavior);
    return { };
}

ExceptionOr<void> NumberInputType::setValueAsDecimal(const Decimal& newValue, TextFieldEventBehavior eventBehavior) const
{
    ASSERT(element());
    protectedElement()->setValue(serializeForNumberType(newValue), eventBehavior);
    return { };
}

bool NumberInputType::typeMismatchFor(const String& value) const
{
    return !value.isEmpty() && !std::isfinite(parseToDoubleForNumberType(value));
}

bool NumberInputType::typeMismatch() const
{
    ASSERT(element());
    ASSERT(!typeMismatchFor(protectedElement()->value()));
    return false;
}

ValueOrReference<String> NumberInputType::stripInvalidNumberCharacters(const String& input)
{
    auto allowedChars = StringView::fromLatin1("0123456789.Ee-+");
    auto length = input.length();
    LOG(Editing, "stripInvalidNumberCharacters: input=[%s], length=%u", input.utf8().data(), length);

    auto needsFiltering = false;
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        LOG(Editing, "Check char at %u: [%c] (code=%d)", i, character, character);
        if (allowedChars.find(character) == notFound) {
            LOG(Editing, "Character [%c] is not allowed, will filter", character);
            needsFiltering = true;
            break;
        }
    }

    if (!needsFiltering) {
        LOG(Editing, "No filtering needed, returning original");
        return input;
    }

    LOG(Editing, "Filtering needed, building new string");
    StringBuilder builder;
    builder.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        if (allowedChars.find(character) != notFound) {
            builder.append(character);
            LOG(Editing, "Appending allowed char: [%c]", character);
        } else
            LOG(Editing, "Skipping disallowed char: [%c]", character);
    }
    LOG(Editing, "Filtering complete, result=[%s]", builder.toString().utf8().data());
    return String { builder.toString() };
}

ValueOrReference<String> NumberInputType::normalizeFullWidthNumberChars(const String& input) const
{
    auto length = input.length();

    auto needsNormalization = false;
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        if ((character >= fullwidthDigitZero && character <= fullwidthDigitNine)
            || character == katakanaHiraganaProlongedSoundMark
            || character == fullwidthHyphenMinus
            || character == minusSign
            || character == fullwidthFullStop) {
            needsNormalization = true;
            break;
        }
    }

    if (!needsNormalization)
        return input;

    StringBuilder result;
    result.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        auto character = input[i];
        if (character >= fullwidthDigitZero && character <= fullwidthDigitNine) {
            // Convert full-width digits (０-９, U+FF10-U+FF19) to ASCII digits (0-9)
            result.append(static_cast<char16_t>(character - fullwidthDigitZero + digitZeroCharacter));
        } else if (character == katakanaHiraganaProlongedSoundMark
            || character == fullwidthHyphenMinus
            || character == minusSign) {
            // Convert minus-like characters commonly produced by IMEs to ASCII '-'.
            //
            // Note: On Japanese IMEs, typing a minus sign in full-width mode can
            // produce 'ー' (U+30FC), '－' (U+FF0D), or '−' (U+2212), depending on the
            // platform and input mode.
            //
            // The following are three common variants depending on input context:
            // - 'ー' (U+30FC): Japanese long sound mark, often produced in full-width kana mode.
            // - '－' (U+FF0D): Full-width hyphen-minus, typically seen on Windows in full-width alphanumeric mode.
            // - '−' (U+2212): Unicode minus sign, commonly produced on macOS or by smart IMEs.
            //
            // Especially, when **only the symbol is typed**, IMEs tend to insert 'ー' (U+30FC)
            // as a long sound mark. If digits follow, the symbol remains unchanged.
            // For example, entering "ー2" instead of "-2" is a typical case.
            //
            // Since users generally intend to input negative numbers in such cases,
            // we normalize 'ー' (U+30FC), '－' (U+FF0D), and '−' (U+2212) to ASCII minus '-' (U+002D).
            result.append(static_cast<char16_t>(hyphenMinus));
        } else if (character == fullwidthFullStop) {
            // Convert full-width full stop (．, U+FF0E) to ASCII dot (.)
            result.append(static_cast<char16_t>(fullStopCharacter));
        } else {
            // Preserve other characters
            // Unreachable in theory, since only normalization-needed characters reach here.
            result.append(static_cast<char16_t>(character));
        }
    }
    return String { result.toString() };
}

StepRange NumberInputType::createStepRange(AnyStepHandling anyStepHandling) const
{
    static NeverDestroyed<const StepRange::StepDescription> stepDescription(numberDefaultStep, numberDefaultStepBase, numberStepScaleFactor);

    ASSERT(element());
    const Decimal stepBase = findStepBase(numberDefaultStepBase);

    const Decimal doubleMax = Decimal::doubleMax();
    Ref element = *this->element();

    RangeLimitations rangeLimitations = RangeLimitations::Invalid;
    auto extractBound = [&] (const QualifiedName& attributeName, const Decimal& defaultValue) -> Decimal {
        const AtomString& attributeValue = element->attributeWithoutSynchronization(attributeName);
        Decimal valueFromAttribute = parseToNumberOrNaN(attributeValue);
        if (valueFromAttribute.isFinite()) {
            rangeLimitations = RangeLimitations::Valid;
            return valueFromAttribute;
        }
        return defaultValue;
    };
    Decimal minimum = extractBound(minAttr, -doubleMax);
    Decimal maximum = extractBound(maxAttr, doubleMax);

    const Decimal step = StepRange::parseStep(anyStepHandling, stepDescription, element->attributeWithoutSynchronization(stepAttr));
    return StepRange(stepBase, rangeLimitations, minimum, maximum, step, stepDescription);
}

bool NumberInputType::sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const
{
    preferredSize = defaultSize;

    ASSERT(element());
    Ref element = *this->element();
    auto& stepString = element->attributeWithoutSynchronization(stepAttr);
    if (equalLettersIgnoringASCIICase(stepString, "any"_s))
        return false;

    const Decimal minimum = parseToDecimalForNumberType(element->attributeWithoutSynchronization(minAttr));
    if (!minimum.isFinite())
        return false;

    const Decimal maximum = parseToDecimalForNumberType(element->attributeWithoutSynchronization(maxAttr));
    if (!maximum.isFinite())
        return false;

    const Decimal step = parseToDecimalForNumberType(stepString, 1);
    ASSERT(step.isFinite());

    RealNumberRenderSize size = calculateRenderSize(minimum).max(calculateRenderSize(maximum).max(calculateRenderSize(step)));

    preferredSize = size.sizeBeforeDecimalPoint + size.sizeAfteDecimalPoint + (size.sizeAfteDecimalPoint ? 1 : 0);

    return true;
}

float NumberInputType::decorationWidth(float inputWidth) const
{
    ASSERT(element());

    float width = 0;
    RefPtr spinButton = protectedElement()->innerSpinButtonElement();
    if (CheckedPtr spinRenderer = spinButton ? spinButton->renderBox() : nullptr) {
        width += spinRenderer->borderAndPaddingLogicalWidth();

        // Since the width of spinRenderer is not calculated yet, spinRenderer->logicalWidth() returns 0.
        // So computedStyle()->logicalWidth() is used instead.

        // FIXME <https://webkit.org/b/294858>: This is incorrect for anything other than fixed widths.
        if (auto fixedLogicalWidth = spinButton->computedStyle()->logicalWidth().tryFixed())
            width += fixedLogicalWidth->resolveZoom(Style::ZoomNeeded { });
        else if (auto percentageLogicalWidth = spinButton->computedStyle()->logicalWidth().tryPercentage()) {
            auto percentageLogicalWidthValue = percentageLogicalWidth->value;
            if (percentageLogicalWidthValue != 100.f)
                width += inputWidth * percentageLogicalWidthValue / (100 - percentageLogicalWidthValue);
        }
    }
    return width;
}

auto NumberInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    handleKeydownEventForSpinButton(event);
    if (!event.defaultHandled())
        return TextFieldInputType::handleKeydownEvent(event);
    return ShouldCallBaseEventHandler::Yes;
}

Decimal NumberInputType::parseToNumber(const String& src, const Decimal& defaultValue) const
{
    return parseToDecimalForNumberType(src, defaultValue);
}

String NumberInputType::serialize(const Decimal& value) const
{
    if (!value.isFinite())
        return String();
    return serializeForNumberType(value);
}

static bool isE(char16_t ch)
{
    return ch == 'e' || ch == 'E';
}

static bool isPlusSign(char16_t ch)
{
    return ch == '+';
}

static bool isSignPrefix(char16_t ch)
{
    return ch == '+' || ch == '-';
}

static bool isDigit(char16_t ch)
{
    return ch >= '0' && ch <= '9';
}

static bool isDecimalSeparator(char16_t ch)
{
    return ch == '.';
}

static bool hasTwoSignChars(const String& string)
{
    unsigned count = 0;
    for (unsigned i = 0; i < string.length(); ++i) {
        auto character = string[i];
        if (isSignPrefix(character))
            ++count;
        if (count >= 2)
            return true;
    }
    return false;
}

static bool hasDecimalSeparator(const String& string)
{
    return string.find('.') != notFound;
}

static bool hasSignNotAfterE(const String& string)
{
    for (unsigned i = 0; i < string.length(); ++i) {
        if (isSignPrefix(string[i]))
            return !i || !isE(string[i - 1]);
    }
    return false;
}

void NumberInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent& event)
{
    // Normalize full-width digits and minus sign to ASCII
    auto normalizedText = normalizeFullWidthNumberChars(event.text()).get();
    LOG(Editing, "normalizeFullWidthNumberChars() -> [%s]", normalizedText.utf8().data());

    auto localizedText = protectedElement()->locale().convertFromLocalizedNumber(normalizedText);

    // If the cleaned up text doesn't match input text, don't insert partial input
    // since it could be an incorrect paste.
    auto updatedEventText = stripInvalidNumberCharacters(localizedText).get();
    LOG(Editing, "stripInvalidNumberCharacters() -> [%s]", updatedEventText.utf8().data());

    // Get left and right of cursor
    ASSERT(element());
    Ref element = *this->element();

    auto originalValue = element->innerTextValue();
    auto selectionStart = element->selectionStart();
    auto selectionEnd = element->selectionEnd();

    auto leftHalf = originalValue.substring(0, selectionStart);
    LOG(Editing, "leftHalf after length=%u", leftHalf.length());

    auto rightHalf = originalValue.substring(selectionEnd);
    LOG(Editing, "rightHalf after length=%u", rightHalf.length());

    // Process 1 char at a time
    auto length = updatedEventText.length();
    StringBuilder finalEventText;
    LOG(Editing, "Processing updatedEventText of length %u", updatedEventText.length());
    for (unsigned i = 0; i < length; ++i) {
        auto character = updatedEventText[i];
        LOG(Editing, "Loop index %u, char [%c] (code %d)", i, character, character);
        if (isDecimalSeparator(character)) {
            // For a decimal point input:
            // - Reject if the editing value already contains another decimal point
            // - Reject if the editing value contains 'e' and the caret is placed
            // after the 'e'.
            // - Reject if the editing value contains '+' or '-' and the caret is
            // placed before it unless it's after an e
            LOG(Editing, "isDecimalSeparator TRUE");
            if (hasDecimalSeparator(leftHalf)
                || hasDecimalSeparator(rightHalf)
                || leftHalf.find('e') != notFound
                || leftHalf.find('E') != notFound
                || hasSignNotAfterE(rightHalf))
                continue;
        } else if (isE(character)) {
            // For 'e' input:
            // - Reject if the editing value already contains another 'e'
            // - Reject if the editing value contains a decimal point, and the caret
            // is placed before it
            LOG(Editing, "isE TRUE");

            // Disallow inserting 'e' if the first character is '+'
            if (!leftHalf.isEmpty() && isPlusSign(leftHalf[0]))
                continue;

            // Reject inserting 'e' at the beginning if the first character is sign
            if (leftHalf.isEmpty() && !rightHalf.isEmpty() && isSignPrefix(rightHalf[0]))
                continue;

            if (leftHalf.find('e') != notFound
                || leftHalf.find('E') != notFound
                || rightHalf.find('e') != notFound
                || rightHalf.find('E') != notFound
                || hasDecimalSeparator(rightHalf))
                continue;
        } else if (isSignPrefix(character)) {
            // For '-' or '+' input:
            // - Reject if the editing value already contains two signs
            // - Reject if the editing value contains 'e' and the caret is placed
            // neither at the beginning of the value nor just after 'e'
            LOG(Editing, "isSignPrefix TRUE");
            StringBuilder bothHalvesBuilder;
            bothHalvesBuilder.append(leftHalf);
            bothHalvesBuilder.append(rightHalf);
            String bothHalves = bothHalvesBuilder.toString();
            if (hasTwoSignChars(bothHalves))
                continue;

            auto hasE = leftHalf.find('e') != notFound || leftHalf.find('E') != notFound
                || rightHalf.find('e') != notFound || rightHalf.find('E') != notFound;

            if (leftHalf.isEmpty()) {
                // Caret is at the start of the value
                if (!rightHalf.isEmpty() && isSignPrefix(rightHalf[0])) {
                    // Reject if there's already a sign at the start (to avoid --1 or ++1)
                    continue;
                }
                if (hasE && isPlusSign(character)) {
                    // If there is already an 'e' in the value, disallow inserting a leading '+'
                    continue;
                }
                // Otherwise, allow inserting '-' or other allowed characters at the start
            } else if (!leftHalf.isEmpty()) {
                if (hasE) {
                    // Must be just after 'e'
                    auto lastCharacterOnLeftHalf = leftHalf[leftHalf.length() - 1];
                    if (!isE(lastCharacterOnLeftHalf))
                        continue;
                    // Reject if there is already a sign after 'e'
                    if (!rightHalf.isEmpty() && isSignPrefix(rightHalf[0]))
                        continue;
                } else {
                    // No 'e' and not at start: reject
                    continue;
                }
            }
        } else if (isDigit(character)) {
            // For a digit input:
            // - Reject if the first letter of the editing value is a sign and the
            // caret is placed just before it
            // - Reject if the editing value contains 'e' + a sign, and the caret is
            // placed between them.
            LOG(Editing, "isDigit TRUE");
            if (leftHalf.isEmpty() && !rightHalf.isEmpty()
                && isSignPrefix(rightHalf[0]))
                continue;

            if (!leftHalf.isEmpty()
                && isE(leftHalf[leftHalf.length() - 1])
                && !rightHalf.isEmpty()
                && isSignPrefix(rightHalf[0]))
                continue;
        } else
            LOG(Editing, "No condition matched for char [%c]", character);

        // Add character
        StringBuilder leftHalfBuilder;
        leftHalfBuilder.append(leftHalf);
        leftHalfBuilder.append(character);
        leftHalf = leftHalfBuilder.toString();
        finalEventText.append(character);
    }
    LOG(Editing, "finalEventText: [%s]", finalEventText.toString().utf8().data());
    event.setText(finalEventText.toString());
}

String NumberInputType::localizeValue(const String& proposedValue) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    // We don't localize scientific notations.
    if (proposedValue.find(isE) != notFound)
        return proposedValue;
    ASSERT(element());
    return protectedElement()->locale().convertToLocalizedNumber(proposedValue);
}

String NumberInputType::visibleValue() const
{
    ASSERT(element());
    return localizeValue(protectedElement()->value());
}

String NumberInputType::convertFromVisibleValue(const String& visibleValue) const
{
    if (visibleValue.isEmpty())
        return visibleValue;
    // We don't localize scientific notations.
    if (visibleValue.find(isE) != notFound)
        return visibleValue;
    ASSERT(element());
    return protectedElement()->locale().convertFromLocalizedNumber(visibleValue);
}

ValueOrReference<String> NumberInputType::sanitizeValue(const String& proposedValue LIFETIME_BOUND) const
{
    if (proposedValue.isEmpty())
        return proposedValue;
    if (std::isfinite(parseToDoubleForNumberType(proposedValue)))
        return proposedValue;
    return emptyString();
}

bool NumberInputType::hasBadInput() const
{
    ASSERT(element());
    String standardValue = convertFromVisibleValue(protectedElement()->innerTextValue());
    return !standardValue.isEmpty() && !std::isfinite(parseToDoubleForNumberType(standardValue));
}

String NumberInputType::badInputText() const
{
    return validationMessageBadInputForNumberText();
}

bool NumberInputType::supportsPlaceholder() const
{
    return true;
}

void NumberInputType::attributeChanged(const QualifiedName& name)
{
    ASSERT(element());
    switch (name.nodeName()) {
    case AttributeNames::maxAttr:
    case AttributeNames::minAttr:
        if (RefPtr element = this->element()) {
            element->invalidateStyleForSubtree();
            if (CheckedPtr renderer = element->renderer())
                renderer->setNeedsLayoutAndPreferredWidthsUpdate();
        }
        break;
    case AttributeNames::classAttr:
    case AttributeNames::stepAttr:
        if (RefPtr element = this->element()) {
            if (CheckedPtr renderer = element->renderer())
                renderer->setNeedsLayoutAndPreferredWidthsUpdate();
        }
        break;
    default:
        break;
    }

    TextFieldInputType::attributeChanged(name);
}

} // namespace WebCore
