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
#import "PDFPluginChoiceAnnotation.h"

#if ENABLE(PDF_PLUGIN) && PLATFORM(MAC)

#import "PDFAnnotationTypeHelpers.h"
#import "PDFKitSPI.h"
#import <WebCore/CSSPrimitiveValue.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/ColorMac.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/Page.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

Ref<PDFPluginChoiceAnnotation> PDFPluginChoiceAnnotation::create(PDFAnnotation *annotation, PDFPluginBase* plugin)
{
    ASSERT(PDFAnnotationTypeHelpers::annotationIsWidgetOfType(annotation, WidgetType::Choice));
    return adoptRef(*new PDFPluginChoiceAnnotation(annotation, plugin));
}

void PDFPluginChoiceAnnotation::updateGeometry()
{
    PDFPluginAnnotation::updateGeometry();

    Ref styledElement = downcast<StyledElement>(*element());
    styledElement->setInlineStyleProperty(CSSPropertyFontSize, annotation().font.pointSize * plugin()->contentScaleFactor(), CSSUnitType::CSS_PX);
}

void PDFPluginChoiceAnnotation::commit()
{
    annotation().widgetStringValue = downcast<HTMLSelectElement>(element())->value();

    PDFPluginAnnotation::commit();
}

Ref<Element> PDFPluginChoiceAnnotation::createAnnotationElement()
{
    Ref document = parent()->document();
    RetainPtr choiceAnnotation = annotation();

    Ref element = downcast<StyledElement>(document->createElement(selectTag, false));

    // FIXME: Match font weight and style as well?
    element->setInlineStyleProperty(CSSPropertyColor, serializationForHTML(colorFromCocoaColor([choiceAnnotation fontColor])));
    element->setInlineStyleProperty(CSSPropertyFontFamily, [[choiceAnnotation font] familyName]);

    NSArray *choices = [choiceAnnotation choices];
    NSString *selectedChoice = [choiceAnnotation widgetStringValue];

    for (NSString *choice in choices) {
        auto choiceOption = document->createElement(optionTag, false);
        choiceOption->setAttributeWithoutSynchronization(valueAttr, choice);
        choiceOption->setTextContent(choice);

        if (choice == selectedChoice)
            choiceOption->setAttributeWithoutSynchronization(selectedAttr, "selected"_s);

        element->appendChild(choiceOption);
    }

    return element;
}

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN) && PLATFORM(MAC)
