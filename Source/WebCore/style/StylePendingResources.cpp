/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "StylePendingResources.h"

#include "CachedResourceLoader.h"
#include "CachedSVGDocumentReference.h"
#include "ContentData.h"
#include "CursorData.h"
#include "CursorList.h"
#include "Document.h"
#include "RenderStyle.h"
#include "SVGURIReference.h"
#include "StyleCachedImage.h"
#include "StyleCachedImageSet.h"
#include "StyleGeneratedImage.h"
#include "StylePendingImage.h"
#include "TransformFunctions.h"

namespace WebCore {
namespace Style {

enum class LoadPolicy { Normal, ShapeOutside };
static RefPtr<StyleImage> loadPendingImage(Document& document, const StyleImage& image, const Element* element, LoadPolicy loadPolicy = LoadPolicy::Normal)
{
    auto& pendingImage = downcast<StylePendingImage>(image);
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.setContentSecurityPolicyImposition(element && element->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);

    // FIXME: Why does shape-outside have different policy than other properties?
    if (loadPolicy == LoadPolicy::ShapeOutside) {
        options.mode = FetchOptions::Mode::Cors;
        options.setAllowCredentials(DoNotAllowStoredCredentials);
    }

    if (auto imageValue = pendingImage.cssImageValue())
        return imageValue->cachedImage(document.cachedResourceLoader(), options);

    if (auto imageGeneratorValue = pendingImage.cssImageGeneratorValue()) {
        imageGeneratorValue->loadSubimages(document.cachedResourceLoader(), options);
        return StyleGeneratedImage::create(*imageGeneratorValue);
    }

    if (auto cursorImageValue = pendingImage.cssCursorImageValue())
        return cursorImageValue->cachedImage(document.cachedResourceLoader(), options);

#if ENABLE(CSS_IMAGE_SET)
    if (auto imageSetValue = pendingImage.cssImageSetValue())
        return imageSetValue->cachedImageSet(document.cachedResourceLoader(), options);
#endif

    return nullptr;
}

static void loadPendingImages(const PendingResources& pendingResources, Document& document, RenderStyle& style, const Element* element)
{
    for (auto currentProperty : pendingResources.pendingImages.keys()) {
        switch (currentProperty) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = &style.ensureBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                auto* styleImage = backgroundLayer->image();
                if (is<StylePendingImage>(styleImage))
                    backgroundLayer->setImage(loadPendingImage(document, *styleImage, element));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(style.contentData()); contentData; contentData = contentData->next()) {
                if (is<ImageContentData>(*contentData)) {
                    auto& styleImage = downcast<ImageContentData>(*contentData).image();
                    if (is<StylePendingImage>(styleImage)) {
                        if (auto loadedImage = loadPendingImage(document, styleImage, element))
                            downcast<ImageContentData>(*contentData).setImage(WTFMove(loadedImage));
                    }
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = style.cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    auto* styleImage = currentCursor.image();
                    if (is<StylePendingImage>(styleImage))
                        currentCursor.setImage(loadPendingImage(document, *styleImage, element));
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            auto* styleImage = style.listStyleImage();
            if (is<StylePendingImage>(styleImage))
                style.setListStyleImage(loadPendingImage(document, *styleImage, element));
            break;
        }
        case CSSPropertyBorderImageSource: {
            auto* styleImage = style.borderImageSource();
            if (is<StylePendingImage>(styleImage))
                style.setBorderImageSource(loadPendingImage(document, *styleImage, element));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = style.boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                auto* styleImage = maskImage.image();
                if (is<StylePendingImage>(styleImage)) {
                    auto loadedImage = loadPendingImage(document, *styleImage, element);
                    reflection->setMask(NinePieceImage(WTFMove(loadedImage), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            auto* styleImage = style.maskBoxImageSource();
            if (is<StylePendingImage>(styleImage))
                style.setMaskBoxImageSource(loadPendingImage(document, *styleImage, element));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = &style.ensureMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                auto* styleImage = maskLayer->image();
                if (is<StylePendingImage>(styleImage))
                    maskLayer->setImage(loadPendingImage(document, *styleImage, element));
            }
            break;
        }
#if ENABLE(CSS_SHAPES)
        case CSSPropertyWebkitShapeOutside: {
            if (!style.shapeOutside())
                return;

            StyleImage* image = style.shapeOutside()->image();
            if (is<StylePendingImage>(image))
                style.shapeOutside()->setImage(loadPendingImage(document, *image, element, LoadPolicy::ShapeOutside));

            break;
        }
#endif
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

static void loadPendingSVGFilters(const PendingResources& pendingResources, Document& document, const Element* element)
{
    if (pendingResources.pendingSVGFilters.isEmpty())
        return;

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.setContentSecurityPolicyImposition(element && element->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck);

    for (auto& filterOperation : pendingResources.pendingSVGFilters)
        filterOperation->getOrCreateCachedSVGDocumentReference()->load(document.cachedResourceLoader(), options);
}

void loadPendingResources(const PendingResources& pendingResources, Document& document, RenderStyle& style, const Element* element)
{
    loadPendingImages(pendingResources, document, style, element);
    loadPendingSVGFilters(pendingResources, document, element);
}

}
}
