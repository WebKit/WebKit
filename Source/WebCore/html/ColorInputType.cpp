/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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
#include "ColorInputType.h"

#include "AXObjectCache.h"
#include "CSSPropertyParserConsumer+ColorInlines.h"
#include "Chrome.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "ColorTypes.h"
#include "ContainerNodeInlines.h"
#include "ElementRareData.h"
#include "Event.h"
#include "HTMLDataListElement.h"
#include "HTMLDivElement.h"
#include "HTMLInputElement.h"
#include "HTMLOptionElement.h"
#include "InputTypeNames.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgentParts.h"
#include "UserGestureIndicator.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ColorInputType);

using namespace HTMLNames;

// https://html.spec.whatwg.org/multipage/infrastructure.html#valid-simple-colour
static bool isValidSimpleColor(StringView string)
{
    if (string.length() != 7)
        return false;
    if (string[0] != '#')
        return false;
    for (unsigned i = 1; i < 7; ++i) {
        if (!isASCIIHexDigit(string[i]))
            return false;
    }
    return true;
}

// https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#rules-for-parsing-simple-colour-values
static std::optional<SRGBA<uint8_t>> parseSimpleColorValue(StringView string)
{
    if (!isValidSimpleColor(string))
        return std::nullopt;
    return { { toASCIIHexValue(string[1], string[2]), toASCIIHexValue(string[3], string[4]), toASCIIHexValue(string[5], string[6]) } };
}

static std::optional<Color> parseColorValue(StringView string, HTMLInputElement& context)
{
    if (context.colorSpace().isNull())
        return parseSimpleColorValue(string);
    using namespace CSSPropertyParserHelpers;
    Ref document = context.document();
    auto parserContext = document->cssParserContext();
    parserContext.mode = HTMLStandardMode;
    auto colorString = string.toString();
    auto color = parseColorRawSimple(colorString, parserContext);
    if (color.isValid())
        return color;

    CSSColorParsingOptions options;
    CSS::PlatformColorResolutionState state {
        .resolvedCurrentColor = Color::black
    };
    color = parseColorRawGeneral(colorString, parserContext, document, options, state);
    if (color.isValid())
        return color;

    return { };
}

static String serializeColorValue(Color input, HTMLInputElement& context)
{
    auto alpha = context.alpha();
    auto colorSpace = context.colorSpace();

    if (!alpha)
        input = input.opaqueColor();

    if (colorSpace.isNull() || colorSpace == "limited-srgb"_s) {
        auto inputAsRGBA = input.toColorTypeLossy<SRGBA<uint8_t>>();
        // When the alpha attribute is set the specification requires the modern color() serialization.
        if (alpha)
            input = { inputAsRGBA, { Color::Flags::UseColorFunctionSerialization } };
        else
            input = inputAsRGBA.resolved();
    } else {
        ASSERT(colorSpace == "display-p3"_s);
        input = input.toColorTypeLossy<ExtendedDisplayP3<float>>().resolved();
    }

    return serializationForHTML(input);
}

ColorInputType::~ColorInputType()
{
    endColorChooser();
}

bool ColorInputType::isMouseFocusable() const
{
    ASSERT(element());
    return protectedElement()->isTextFormControlFocusable();
}

bool ColorInputType::isKeyboardFocusable(const FocusEventData&) const
{
    ASSERT(element());
#if PLATFORM(IOS_FAMILY)
    return element()->isTextFormControlFocusable();
#else
    return false;
#endif
}

bool ColorInputType::isPresentingAttachedView() const
{
    return !!m_chooser;
}

const AtomString& ColorInputType::formControlType() const
{
    return InputTypeNames::color();
}

bool ColorInputType::supportsRequired() const
{
    return false;
}

ValueOrReference<String> ColorInputType::fallbackValue() const
{
    ASSERT(element());
    return serializeColorValue(Color::black, *protectedElement());
}

ValueOrReference<String> ColorInputType::sanitizeValue(const String& proposedValue LIFETIME_BOUND) const
{
    ASSERT(element());
    Ref input = *element();
    auto color = parseColorValue(proposedValue, input);

    if (!color)
        return fallbackValue();

    return serializeColorValue(*color, input);
}

Color ColorInputType::valueAsColor() const
{
    ASSERT(element());
    Ref input = *element();
    auto color = parseColorValue(input->value().get(), input);
    ASSERT(!!color);
    // FIXME: This is a speculative fix for rdar://144872437.
    if (!color)
        return Color::black;
    return *color;
}

void ColorInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(element());
    ASSERT(element()->shadowRoot());

    Ref document = element()->document();
    Ref wrapperElement = HTMLDivElement::create(document);
    Ref colorSwatch = HTMLDivElement::create(document);

    Ref shadowRoot = *protectedElement()->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };
    shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, wrapperElement);

    wrapperElement->appendChild(ContainerNode::ChildChange::Source::Parser, colorSwatch);
    wrapperElement->setUserAgentPart(UserAgentParts::webkitColorSwatchWrapper());
    colorSwatch->setUserAgentPart(UserAgentParts::webkitColorSwatch());

    RenderTheme::singleton().createColorWellSwatchSubtree(colorSwatch.get());

    updateColorSwatch();
}

void ColorInputType::setValue(const String& value, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    InputType::setValue(value, valueChanged, eventBehavior, selection);

    if (!valueChanged)
        return;

    updateColorSwatch();
    if (RefPtr chooser = m_chooser)
        chooser->setSelectedColor(valueAsColor());
}

void ColorInputType::attributeChanged(const QualifiedName& name)
{
    if (name == valueAttr) {
        updateColorSwatch();

        Ref input = *element();
        if (CheckedPtr cache = input->protectedDocument()->existingAXObjectCache())
            cache->valueChanged(input);
    }

    InputType::attributeChanged(name);
}

void ColorInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    if (element()->isDisabledFormControl() || !element()->renderer())
        return;

    if (!UserGestureIndicator::processingUserGesture())
        return;

    showPicker();
    event.setDefaultHandled();
}

void ColorInputType::showPicker()
{
    if (Chrome* chrome = this->chrome()) {
        if (RefPtr chooser = m_chooser)
            chooser->reattachColorChooser(valueAsColor());
        else
            m_chooser = chrome->createColorChooser(*this, valueAsColor());
    }
}

bool ColorInputType::allowsShowPickerAcrossFrames()
{
    return true;
}

void ColorInputType::detach()
{
    endColorChooser();
}

void ColorInputType::elementDidBlur()
{
    endColorChooser();
}

bool ColorInputType::shouldRespectListAttribute()
{
    return true;
}

bool ColorInputType::shouldResetOnDocumentActivation()
{
    return true;
}

void ColorInputType::didChooseColor(const Color& color)
{
    ASSERT(element());

    Ref input = *element();
    if (input->isDisabledFormControl())
        return;

    auto serializedColor = serializeColorValue(color, input);
    if (serializedColor == input->value())
        return;

    EventQueueScope scope;
    input->setValueFromRenderer(serializedColor);
    updateColorSwatch();
    input->dispatchFormControlChangeEvent();

    if (CheckedPtr cache = input->protectedDocument()->existingAXObjectCache())
        cache->valueChanged(input);
}

void ColorInputType::didEndChooser()
{
    m_chooser = nullptr;
    if (CheckedPtr renderer = protectedElement()->renderer())
        renderer->repaint();
}

void ColorInputType::endColorChooser()
{
    if (RefPtr chooser = m_chooser)
        chooser->endChooser();
}

void ColorInputType::updateColorSwatch()
{
    RefPtr colorSwatch = shadowColorSwatch();
    if (!colorSwatch)
        return;

    RenderTheme::singleton().setColorWellSwatchBackground(*colorSwatch, valueAsColor());
}

HTMLElement* ColorInputType::shadowColorSwatch() const
{
    ASSERT(element());
    RefPtr shadow = protectedElement()->userAgentShadowRoot();
    if (!shadow)
        return nullptr;

    RefPtr wrapper = shadow->firstChild();
    return wrapper ? downcast<HTMLElement>(wrapper->firstChild()) : nullptr;
}

IntRect ColorInputType::elementRectRelativeToRootView() const
{
    ASSERT(element());
    Ref element = *this->element();
    CheckedPtr renderer = element->renderer();
    if (!renderer)
        return IntRect();
    return element->protectedDocument()->protectedView()->contentsToRootView(renderer->absoluteBoundingBoxRect());
}

bool ColorInputType::supportsAlpha() const
{
    ASSERT(element());
    return protectedElement()->alpha();
}

Vector<Color> ColorInputType::suggestedColors() const
{
    Vector<Color> suggestions;
    ASSERT(element());
    Ref input = *element();
    if (auto dataList = input->dataList()) {
        for (Ref option : dataList->suggestions()) {
            if (auto color = parseColorValue(option->value(), input))
                suggestions.append(*color);
        }
    }
    return suggestions;
}

void ColorInputType::selectColor(StringView string)
{
    ASSERT(element());
    if (auto color = parseColorValue(string, *protectedElement()))
        didChooseColor(*color);
}

} // namespace WebCore
