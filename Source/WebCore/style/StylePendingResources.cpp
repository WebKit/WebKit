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

#include "CSSCursorImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "Document.h"
#include "DocumentResourceLoader.h"
#include "DocumentView.h"
#include "NodeInlinesLight.h"
#include "ResourceRequest.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGURIReference.h"
#include "Settings.h"
#include "StyleClipPath.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleCursor.h"
#include "StyleFilter.h"
#include "StyleFilterReference.h"
#include "StyleImage.h"
#include "StyleSVGMarkerResource.h"

namespace WebCore {
namespace Style {

// <https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-settings-attributes>
enum class LoadPolicy { CORS, NoCORS, Anonymous };

static void loadPendingImage(Document& document, const Image* image, const Element* element, LoadPolicy loadPolicy = LoadPolicy::NoCORS)
{
    if (!image || !image->isPending())
        return;

    bool isInUserAgentShadowTree = element && element->isInUserAgentShadowTree();
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = isInUserAgentShadowTree ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
    options.shouldEnableContentExtensionsCheck = isInUserAgentShadowTree ? ShouldEnableContentExtensionsCheck::No : ShouldEnableContentExtensionsCheck::Yes;

    if (!isInUserAgentShadowTree && document.settings().useAnonymousModeWhenFetchingMaskImages()) {
        switch (loadPolicy) {
        case LoadPolicy::Anonymous:
            options.storedCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;
            [[fallthrough]];
        case LoadPolicy::CORS:
            options.mode = FetchOptions::Mode::Cors;
            options.credentials = FetchOptions::Credentials::SameOrigin;
            options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
            break;
        case LoadPolicy::NoCORS:
            break;
        }
    }

    const_cast<Image&>(*image).load(protect(document.cachedResourceLoader()), options);
}

static void loadExternalSVGResource(Document& document, const WTF::URL& url)
{
    if (url.isNull())
        return;

    if (!url.protocolIsData() && !SVGURIReference::isExternalURIReference(url.string(), document))
        return;

    auto documentURL = url;
    documentURL.removeFragmentIdentifier();

    CheckedRef extensions = document.svgExtensions();
    if (extensions->hasExternalSVGResource(documentURL))
        return;

    auto options = CachedResourceLoader::defaultCachedResourceOptions();
    options.mode = FetchOptions::Mode::SameOrigin;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    CachedResourceRequest request(ResourceRequest(WTF::URL { documentURL }), options);
    request.setInitiatorType(cachedResourceRequestInitiatorTypes().css);
    RefPtr cachedImage = protect(document.cachedResourceLoader())->requestImage(WTF::move(request)).value_or(nullptr);
    if (!cachedImage)
        return;
    extensions->addExternalSVGResource(documentURL, *cachedImage, document);
}

static void loadPendingExternalSVGPaint(Document& document, const Style::SVGPaint& paint)
{
    if (auto url = paint.tryAnyURL())
        loadExternalSVGResource(document, url->resolved);
}

static void loadPendingExternalSVGMarker(Document& document, const Style::SVGMarkerResource& marker)
{
    if (auto url = marker.tryURL())
        loadExternalSVGResource(document, url->resolved);
}

static void loadPendingExternalSVGClipPath(Document& document, const Style::ClipPath& clipPath)
{
    if (auto referencePath = clipPath.tryReference())
        loadExternalSVGResource(document, referencePath->url().resolved);
}

void loadPendingResources(Style::ComputedStyle& style, Document& document, const Element* element)
{
    for (auto& backgroundLayer : style.backgroundLayers().usedValues())
        loadPendingImage(document, backgroundLayer.image().tryStyleImage().get(), element);

    if (auto* contentData = style.content().tryData()) {
        for (auto& contentItem : contentData->visible) {
            WTF::switchOn(contentItem,
                [&](const Style::Content::Image& image) {
                    loadPendingImage(document, image.image.value.ptr(), element);
                },
                [](const auto&) { }
            );
        }
    }

    if (auto cursorImages = style.cursor().images) {
        for (auto& cursorImage : *cursorImages)
            loadPendingImage(document, cursorImage.image.ptr(), element);
    }

    loadPendingImage(document, style.listStyleImage().tryStyleImage().get(), element);
    loadPendingImage(document, style.borderImageSource().tryStyleImage().get(), element);
    loadPendingImage(document, style.maskBorderSource().tryStyleImage().get(), element);

    if (auto reflection = style.boxReflect().tryReflection())
        loadPendingImage(document, reflection->mask.source().tryStyleImage().get(), element);

    // Masking operations may be sensitive to timing attacks that can be used to reveal the pixel data of
    // the image used as the mask. As a means to mitigate such attacks CSS mask images and shape-outside
    // images are retrieved in "Anonymous" mode, which uses a potentially CORS-enabled fetch.
    for (auto& maskLayer : style.maskLayers().usedValues())
        loadPendingImage(document, maskLayer.image().tryStyleImage().get(), element, LoadPolicy::CORS);

    if (RefPtr shapeValueImage = style.shapeOutside().image())
        loadPendingImage(document, shapeValueImage.get(), element, LoadPolicy::Anonymous);

    // External/data: SVG paint servers and filters resolve through SVGResources, which only applies
    // to SVG elements. Restrict loading to them so CSS filter on non-SVG elements (a separate path)
    // isn't pulled through here.
    if (document.settings().svgExternalResourcesEnabled() && element && is<SVGElement>(*element)) {
        loadPendingExternalSVGPaint(document, style.fill());
        loadPendingExternalSVGPaint(document, style.stroke());

        loadPendingExternalSVGMarker(document, style.markerStart());
        loadPendingExternalSVGMarker(document, style.markerMid());
        loadPendingExternalSVGMarker(document, style.markerEnd());

        loadPendingExternalSVGClipPath(document, style.clipPath());

        if (style.filter().size() == 1) {
            WTF::switchOn(style.filter().first(),
                [&](const Style::FilterReference& filterReference) {
                    loadExternalSVGResource(document, filterReference.url.resolved);
                },
                [](const auto&) { }
            );
        }
    }

    // Are there other pseudo-elements that need resource loading?
    if (CheckedPtr firstLineStyle = style.pseudoElementStyle({ PseudoElementType::FirstLine }))
        loadPendingResources(*firstLineStyle, document, element);
}

}
}
