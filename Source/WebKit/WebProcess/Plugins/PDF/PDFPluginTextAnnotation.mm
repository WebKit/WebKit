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

#import "config.h"
#import "PDFPluginTextAnnotation.h"

#if ENABLE(PDFKIT_PLUGIN)

#import "PDFAnnotationTextWidgetDetails.h"
#import "PDFKitImports.h"
#import "PDFLayerControllerSPI.h"
#import "PDFPlugin.h"
#import <PDFKit/PDFKit.h>
#import <WebCore/CSSPrimitiveValue.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/ColorMac.h>
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
        return "left";
    case NSTextAlignmentRight:
        return "right";
    case NSTextAlignmentCenter:
        return "center";
    case NSTextAlignmentJustified:
        return "justify";
    case NSTextAlignmentNatural:
        return "-webkit-start";
    }
    ASSERT_NOT_REACHED();
    return String();
}

Ref<PDFPluginTextAnnotation> PDFPluginTextAnnotation::create(PDFAnnotation *annotation, PDFLayerController *pdfLayerController, PDFPlugin* plugin)
{
    return adoptRef(*new PDFPluginTextAnnotation(annotation, pdfLayerController, plugin));
}

PDFPluginTextAnnotation::~PDFPluginTextAnnotation()
{
    element()->removeEventListener(eventNames().keydownEvent, *eventListener(), false);
}

Ref<Element> PDFPluginTextAnnotation::createAnnotationElement()
{
    Document& document = parent()->document();
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    PDFAnnotationTextWidget *textAnnotation = this->textAnnotation();
    ALLOW_DEPRECATED_DECLARATIONS_END
    bool isMultiline = textAnnotation.isMultiline;

    auto element = document.createElement(isMultiline ? textareaTag : inputTag, false);
    element->addEventListener(eventNames().keydownEvent, *eventListener(), false);

    auto& styledElement = downcast<StyledElement>(element.get());

    if (!textAnnotation)
        return element;

    // FIXME: Match font weight and style as well?
    styledElement.setInlineStyleProperty(CSSPropertyColor, colorFromNSColor(textAnnotation.fontColor).serialized());
    styledElement.setInlineStyleProperty(CSSPropertyFontFamily, textAnnotation.font.familyName);
    styledElement.setInlineStyleProperty(CSSPropertyTextAlign, cssAlignmentValueForNSTextAlignment(textAnnotation.alignment));

    if (isMultiline)
        downcast<HTMLTextAreaElement>(styledElement).setValue(textAnnotation.stringValue);
    else
        downcast<HTMLInputElement>(styledElement).setValue(textAnnotation.stringValue);

    return element;
}

void PDFPluginTextAnnotation::updateGeometry()
{
    PDFPluginAnnotation::updateGeometry();

    StyledElement* styledElement = static_cast<StyledElement*>(element());
    styledElement->setInlineStyleProperty(CSSPropertyFontSize, textAnnotation().font.pointSize * pdfLayerController().contentScaleFactor, CSSPrimitiveValue::CSS_PX);
}

void PDFPluginTextAnnotation::commit()
{
    textAnnotation().stringValue = value();
    PDFPluginAnnotation::commit();
}

String PDFPluginTextAnnotation::value() const
{
    return downcast<HTMLTextFormControlElement>(element())->value();
}

bool PDFPluginTextAnnotation::handleEvent(Event& event)
{
    if (PDFPluginAnnotation::handleEvent(event))
        return true;

    if (event.isKeyboardEvent() && event.type() == eventNames().keydownEvent) {
        auto& keyboardEvent = downcast<KeyboardEvent>(event);

        if (keyboardEvent.keyIdentifier() == "U+0009") {
            if (keyboardEvent.ctrlKey() || keyboardEvent.metaKey() || keyboardEvent.altGraphKey())
                return false;

            if (keyboardEvent.shiftKey())
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

#endif // ENABLE(PDFKIT_PLUGIN)
