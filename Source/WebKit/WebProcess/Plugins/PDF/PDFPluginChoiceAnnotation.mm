/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN)

#import "config.h"
#import "PDFPluginChoiceAnnotation.h"

#import "PDFKitImports.h"
#import "PDFLayerControllerSPI.h"
#import <Quartz/Quartz.h>
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

Ref<PDFPluginChoiceAnnotation> PDFPluginChoiceAnnotation::create(PDFAnnotation *annotation, PDFLayerController *pdfLayerController, PDFPlugin* plugin)
{
    return adoptRef(*new PDFPluginChoiceAnnotation(annotation, pdfLayerController, plugin));
}

void PDFPluginChoiceAnnotation::updateGeometry()
{
    PDFPluginAnnotation::updateGeometry();

    StyledElement* styledElement = static_cast<StyledElement*>(element());
    styledElement->setInlineStyleProperty(CSSPropertyFontSize, choiceAnnotation().font.pointSize * pdfLayerController().contentScaleFactor, CSSUnitType::CSS_PX);
}

void PDFPluginChoiceAnnotation::commit()
{
    choiceAnnotation().stringValue = downcast<HTMLSelectElement>(element())->value();

    PDFPluginAnnotation::commit();
}

Ref<Element> PDFPluginChoiceAnnotation::createAnnotationElement()
{
    Document& document = parent()->document();
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    PDFAnnotationChoiceWidget *choiceAnnotation = this->choiceAnnotation();
    ALLOW_DEPRECATED_DECLARATIONS_END

    auto element = document.createElement(selectTag, false);

    auto& styledElement = downcast<StyledElement>(element.get());

    // FIXME: Match font weight and style as well?
    styledElement.setInlineStyleProperty(CSSPropertyColor, serializationForHTML(colorFromNSColor(choiceAnnotation.fontColor)));
    styledElement.setInlineStyleProperty(CSSPropertyFontFamily, choiceAnnotation.font.familyName);

    NSArray *choices = choiceAnnotation.choices;
    NSString *selectedChoice = choiceAnnotation.stringValue;

    for (NSString *choice in choices) {
        auto choiceOption = document.createElement(optionTag, false);
        choiceOption->setAttributeWithoutSynchronization(valueAttr, choice);
        choiceOption->setTextContent(choice);

        if (choice == selectedChoice)
            choiceOption->setAttributeWithoutSynchronization(selectedAttr, AtomString("selected", AtomString::ConstructFromLiteral));

        styledElement.appendChild(choiceOption);
    }

    return element;
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
