/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PDFPluginTextAnnotation.h"

#if ENABLE(PDF_PLUGIN)

#import "PDFAnnotationTypeHelpers.h"
#import "PDFKitSPI.h"
#import <WebCore/AddEventListenerOptions.h>
#import <WebCore/CSSPrimitiveValue.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/Page.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

static const String cssAlignmentValueForNSTextAlignment(NSTextAlignment alignment)
{
    switch (alignment) {
    case NSTextAlignmentLeft:
        return "left"_s;
    case NSTextAlignmentRight:
        return "right"_s;
    case NSTextAlignmentCenter:
        return "center"_s;
    case NSTextAlignmentJustified:
        return "justify"_s;
    case NSTextAlignmentNatural:
        return "-webkit-start"_s;
    }
    ASSERT_NOT_REACHED();
    return String();
}

Ref<PDFPluginTextAnnotation> PDFPluginTextAnnotation::create(PDFAnnotation *annotation, PDFPluginBase* plugin)
{
    ASSERT(PDFAnnotationTypeHelpers::annotationIsWidgetOfType(annotation, WidgetType::Text));
    return adoptRef(*new PDFPluginTextAnnotation(annotation, plugin));
}

PDFPluginTextAnnotation::~PDFPluginTextAnnotation()
{
    element()->removeEventListener(eventNames().keydownEvent, *eventListener(), false);
}

Ref<Element> PDFPluginTextAnnotation::createAnnotationElement()
{
    Document& document = parent()->document();
    RetainPtr textAnnotation = annotation();
    bool isMultiline = [textAnnotation isMultiline];

    Ref element = downcast<HTMLTextFormControlElement>(document.createElement(isMultiline ? textareaTag : inputTag, false));
    element->addEventListener(eventNames().keydownEvent, *eventListener(), false);

    if (!textAnnotation)
        return element;

    // FIXME: Match font weight and style as well?
    element->setInlineStyleProperty(CSSPropertyColor, serializationForHTML(colorFromCocoaColor([textAnnotation fontColor])));
    element->setInlineStyleProperty(CSSPropertyFontFamily, [[textAnnotation font] familyName]);
    element->setInlineStyleProperty(CSSPropertyTextAlign, cssAlignmentValueForNSTextAlignment([textAnnotation alignment]));

    element->setValue([textAnnotation widgetStringValue]);

    return element;
}

void PDFPluginTextAnnotation::updateGeometry()
{
    PDFPluginAnnotation::updateGeometry();

    Ref styledElement = downcast<StyledElement>(*element());
    styledElement->setInlineStyleProperty(CSSPropertyFontSize, annotation().font.pointSize * plugin()->contentScaleFactor(), CSSUnitType::CSS_PX);
}

void PDFPluginTextAnnotation::commit()
{
    annotation().widgetStringValue = value();
    PDFPluginAnnotation::commit();
}

String PDFPluginTextAnnotation::value() const
{
    return downcast<HTMLTextFormControlElement>(element())->value();
}

void PDFPluginTextAnnotation::setValue(const String& value)
{
    downcast<HTMLTextFormControlElement>(element())->setValue(value);
}

bool PDFPluginTextAnnotation::handleEvent(Event& event)
{
    if (PDFPluginAnnotation::handleEvent(event))
        return true;

    if (auto* keyboardEvent = dynamicDowncast<KeyboardEvent>(event); keyboardEvent && keyboardEvent->type() == eventNames().keydownEvent) {
        if (keyboardEvent->keyIdentifier() == "U+0009"_s) {
            if (keyboardEvent->ctrlKey() || keyboardEvent->metaKey())
                return false;

            if (keyboardEvent->shiftKey())
                plugin()->focusPreviousAnnotation();
            else
                plugin()->focusNextAnnotation();
            
            event.preventDefault();
            return true;
        }
    }

    return false;
}

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
