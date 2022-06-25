/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "PDFPluginPasswordField.h"

#if ENABLE(PDFKIT_PLUGIN)

#import "PDFLayerControllerSPI.h"
#import "PDFPlugin.h"
#import <Quartz/Quartz.h>
#import <WebCore/AddEventListenerOptions.h>
#import <WebCore/Event.h>
#import <WebCore/EventNames.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/KeyboardEvent.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

Ref<PDFPluginPasswordField> PDFPluginPasswordField::create(PDFLayerController *pdfLayerController, PDFPlugin* plugin)
{
    return adoptRef(*new PDFPluginPasswordField(pdfLayerController, plugin));
}

PDFPluginPasswordField::~PDFPluginPasswordField()
{
    element()->removeEventListener(eventNames().keyupEvent, *eventListener(), false);
}

Ref<Element> PDFPluginPasswordField::createAnnotationElement()
{
    auto element = PDFPluginTextAnnotation::createAnnotationElement();
    element->setAttribute(typeAttr, "password"_s);
    element->addEventListener(eventNames().keyupEvent, *eventListener(), false);
    return element;
}

void PDFPluginPasswordField::updateGeometry()
{
}

bool PDFPluginPasswordField::handleEvent(WebCore::Event& event)
{
    if (event.isKeyboardEvent() && event.type() == eventNames().keyupEvent) {
        auto& keyboardEvent = downcast<KeyboardEvent>(event);

        if (keyboardEvent.keyIdentifier() == "Enter"_s) {
            plugin()->attemptToUnlockPDF(value());
            event.preventDefault();
            return true;
        }
    }

    return false;
}
    
} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
