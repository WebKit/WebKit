/*
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ImageInputType.h"

#include "CachedImage.h"
#include "DOMFormData.h"
#include "ElementInlines.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include "MouseEvent.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

ImageInputType::ImageInputType(HTMLInputElement& element)
    : BaseButtonInputType(Type::Image, element)
{
}

const AtomString& ImageInputType::formControlType() const
{
    return InputTypeNames::image();
}

bool ImageInputType::isFormDataAppendable() const
{
    return true;
}

bool ImageInputType::appendFormData(DOMFormData& formData) const
{
    ASSERT(element());
    if (!element()->isActivatedSubmit())
        return false;

    auto& name = element()->name();
    if (name.isEmpty()) {
        formData.append("x"_s, String::number(m_clickLocation.x()));
        formData.append("y"_s, String::number(m_clickLocation.y()));
        return true;
    }

    formData.append(makeString(name, ".x"), String::number(m_clickLocation.x()));
    formData.append(makeString(name, ".y"), String::number(m_clickLocation.y()));

    return true;
}

void ImageInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    Ref<HTMLInputElement> protectedElement(*element());
    if (protectedElement->isDisabledFormControl() || !protectedElement->form())
        return;

    Ref<HTMLFormElement> protectedForm(*protectedElement->form());

    protectedElement->setActivatedSubmit(true);

    m_clickLocation = IntPoint();
    if (event.underlyingEvent()) {
        Event& underlyingEvent = *event.underlyingEvent();
        if (auto* mouseEvent = dynamicDowncast<MouseEvent>(underlyingEvent)) {
            if (!mouseEvent->isSimulated())
                m_clickLocation = IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY());
        }
    }

    // Update layout before processing form actions in case the style changes
    // the Form or button relationships.
    protectedElement->document().updateLayoutIgnorePendingStylesheets();

    if (auto currentForm = protectedElement->form())
        currentForm->submitIfPossible(&event, element()); // Event handlers can run.

    protectedElement->setActivatedSubmit(false);
    event.setDefaultHandled();
}

RenderPtr<RenderElement> ImageInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return createRenderer<RenderImage>(RenderObject::Type::Image, *element(), WTFMove(style));
}

void ImageInputType::attributeChanged(const QualifiedName& name)
{
    if (name == altAttr) {
        if (auto* element = this->element()) {
            auto* renderer = element->renderer();
            if (auto* renderImage = dynamicDowncast<RenderImage>(renderer))
                renderImage->updateAltText();
        }
    } else if (name == srcAttr) {
        if (auto* element = this->element()) {
            if (element->renderer())
                element->ensureImageLoader().updateFromElementIgnoringPreviousError();
        }
    }
    BaseButtonInputType::attributeChanged(name);
}

void ImageInputType::attach()
{
    BaseButtonInputType::attach();

    ASSERT(element());
    HTMLImageLoader& imageLoader = element()->ensureImageLoader();
    imageLoader.updateFromElement();

    auto* renderer = downcast<RenderImage>(element()->renderer());
    if (!renderer)
        return;

    if (imageLoader.hasPendingBeforeLoadEvent())
        return;

    auto& imageResource = renderer->imageResource();
    imageResource.setCachedImage(imageLoader.image());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!imageLoader.image() && !imageResource.cachedImage())
        renderer->setImageSizeForAltText();
}

bool ImageInputType::shouldRespectAlignAttribute()
{
    return true;
}

bool ImageInputType::canBeSuccessfulSubmitButton()
{
    return true;
}

bool ImageInputType::shouldRespectHeightAndWidthAttributes()
{
    return true;
}

unsigned ImageInputType::height() const
{
    ASSERT(element());
    Ref<HTMLInputElement> element(*this->element());

    element->document().updateLayout({ LayoutOptions::ContentVisibilityForceLayout }, element.ptr());

    if (auto* renderer = element->renderer())
        return adjustForAbsoluteZoom(downcast<RenderBox>(*renderer).contentHeight(), *renderer);

    // Check the attribute first for an explicit pixel value.
    if (auto optionalHeight = parseHTMLNonNegativeInteger(element->attributeWithoutSynchronization(heightAttr)))
        return optionalHeight.value();

    // If the image is available, use its height.
    auto* imageLoader = element->imageLoader();
    if (imageLoader && imageLoader->image())
        return imageLoader->image()->imageSizeForRenderer(element->renderer(), 1).height().toUnsigned();

    return 0;
}

unsigned ImageInputType::width() const
{
    ASSERT(element());
    Ref<HTMLInputElement> element(*this->element());

    element->document().updateLayout({ LayoutOptions::ContentVisibilityForceLayout }, element.ptr());

    if (auto* renderer = element->renderer())
        return adjustForAbsoluteZoom(downcast<RenderBox>(*renderer).contentWidth(), *renderer);

    // Check the attribute first for an explicit pixel value.
    if (auto optionalWidth = parseHTMLNonNegativeInteger(element->attributeWithoutSynchronization(widthAttr)))
        return optionalWidth.value();

    // If the image is available, use its width.
    auto* imageLoader = element->imageLoader();
    if (imageLoader && imageLoader->image())
        return imageLoader->image()->imageSizeForRenderer(element->renderer(), 1).width().toUnsigned();

    return 0;
}

String ImageInputType::resultForDialogSubmit() const
{
    return makeString(m_clickLocation.x(), ',', m_clickLocation.y());
}

bool ImageInputType::dirAutoUsesValue() const
{
    return false;
}

} // namespace WebCore
