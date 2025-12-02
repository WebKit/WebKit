/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "ContainerNodeInlines.h"
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
#include "RenderElementStyleInlines.h"
#include "RenderImage.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageInputType);

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
    Ref element = *this->element();
    if (!element->isActivatedSubmit())
        return false;

    auto& name = element->name();
    if (name.isEmpty()) {
        formData.append("x"_s, String::number(m_clickLocation.x()));
        formData.append("y"_s, String::number(m_clickLocation.y()));
        return true;
    }

    formData.append(makeString(name, ".x"_s), String::number(m_clickLocation.x()));
    formData.append(makeString(name, ".y"_s), String::number(m_clickLocation.y()));

    return true;
}

void ImageInputType::handleDOMActivateEvent(Event& event)
{
    ASSERT(element());
    Ref element = *this->element();
    if (element->isDisabledFormControl() || !element->form())
        return;

    Ref protectedForm = *element->form();

    m_clickLocation = IntPoint();
    if (RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event.underlyingEvent())) {
        if (!mouseEvent->isSimulated())
            m_clickLocation = IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY());
    }

    // Update layout before processing form actions in case the style changes
    // the Form or button relationships.
    element->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    if (RefPtr currentForm = element->form())
        currentForm->submitIfPossible(&event, element.ptr()); // Event handlers can run.

    event.setDefaultHandled();
}

RenderPtr<RenderElement> ImageInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    // FIXME: https://github.com/llvm/llvm-project/pull/142471 Moving style is not unsafe.
    SUPPRESS_UNCOUNTED_ARG return createRenderer<RenderImage>(RenderObject::Type::Image, *protectedElement(), WTFMove(style));
}

void ImageInputType::attributeChanged(const QualifiedName& name)
{
    if (name == altAttr) {
        if (RefPtr element = this->element()) {
            if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(element->renderer()))
                renderImage->updateAltText();
        }
    } else if (name == srcAttr) {
        if (RefPtr element = this->element()) {
            if (element->renderer())
                element->ensureProtectedImageLoader()->updateFromElementIgnoringPreviousError();
        }
    }
    BaseButtonInputType::attributeChanged(name);
}

void ImageInputType::attach()
{
    BaseButtonInputType::attach();

    ASSERT(element());
    Ref imageLoader = protectedElement()->ensureImageLoader();
    imageLoader->updateFromElement();

    CheckedPtr renderer = downcast<RenderImage>(element()->renderer());
    if (!renderer)
        return;

    if (imageLoader->hasPendingBeforeLoadEvent())
        return;

    CheckedRef imageResource = renderer->imageResource();
    imageResource->setCachedImage(imageLoader->protectedImage());

    // If we have no image at all because we have no src attribute, set
    // image height and width for the alt text instead.
    if (!imageLoader->image() && !imageResource->cachedImage())
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
    Ref element = *this->element();

    element->protectedDocument()->updateLayout({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element.ptr());

    CheckedPtr renderer = element->renderer();
    if (renderer)
        return adjustForAbsoluteZoom(downcast<RenderBox>(*renderer).contentBoxHeight(), *renderer);

    // Check the attribute first for an explicit pixel value.
    if (auto optionalHeight = parseHTMLNonNegativeInteger(element->attributeWithoutSynchronization(heightAttr)))
        return optionalHeight.value();

    // If the image is available, use its height.
    RefPtr imageLoader = element->imageLoader();
    if (imageLoader && imageLoader->image())
        return imageLoader->image()->imageSizeForRenderer(renderer.get(), 1).height().toUnsigned();

    return 0;
}

unsigned ImageInputType::width() const
{
    ASSERT(element());
    Ref element = *this->element();

    element->protectedDocument()->updateLayout({ LayoutOptions::TreatContentVisibilityHiddenAsVisible, LayoutOptions::TreatContentVisibilityAutoAsVisible }, element.ptr());

    CheckedPtr renderer = element->renderer();
    if (renderer)
        return adjustForAbsoluteZoom(downcast<RenderBox>(*renderer).contentBoxWidth(), *renderer);

    // Check the attribute first for an explicit pixel value.
    if (auto optionalWidth = parseHTMLNonNegativeInteger(element->attributeWithoutSynchronization(widthAttr)))
        return optionalWidth.value();

    // If the image is available, use its width.
    RefPtr imageLoader = element->imageLoader();
    if (imageLoader && imageLoader->image())
        return imageLoader->image()->imageSizeForRenderer(renderer.get(), 1).width().toUnsigned();

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
