/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageControlsRootElementMac.h"

#if ENABLE(IMAGE_CONTROLS)

#include "ContextMenuController.h"
#include "Event.h"
#include "Page.h"
#include "Text.h"

namespace WebCore {

ImageControlsRootElementMac::ImageControlsRootElementMac(Document& document)
    : ImageControlsRootElement(document)
{
}

ImageControlsRootElementMac::~ImageControlsRootElementMac()
{
}

PassRefPtr<ImageControlsRootElement> ImageControlsRootElement::maybeCreate(Document& document)
{
    return ImageControlsRootElementMac::maybeCreate(document);
}

PassRefPtr<ImageControlsRootElementMac> ImageControlsRootElementMac::maybeCreate(Document& document)
{
    if (!document.page())
        return nullptr;

    RefPtr<ImageControlsRootElementMac> controls = adoptRef(new ImageControlsRootElementMac(document));
    controls->setAttribute(HTMLNames::classAttr, "x-webkit-imagemenu");
    ExceptionCode ec;
    controls->appendChild(Text::create(document, ""), ec);
    if (ec)
        return nullptr;

    return controls.release();
}

void ImageControlsRootElementMac::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().clickEvent) {
        if (Page* page = document().page())
            page->contextMenuController().showImageControlsMenu(event);

        return;
    }
    
    HTMLDivElement::defaultEventHandler(event);
}

} // namespace WebCore

#endif // ENABLE(IMAGE_CONTROLS)
