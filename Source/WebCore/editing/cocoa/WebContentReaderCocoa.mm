/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebContentReader.h"

#include "ArchiveResource.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLImageElement.h"
#include "LegacyWebArchive.h"
#include "Page.h"
#include "Settings.h"
#include "WebNSAttributedStringExtras.h"
#include "markup.h"
#include <pal/spi/cocoa/NSAttributedStringSPI.h>
#include <wtf/SoftLinking.h>

#if PLATFORM(IOS)
SOFT_LINK_PRIVATE_FRAMEWORK(WebKitLegacy)
#endif

#if PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(WebKit, WebKitLegacy)
#endif

// FIXME: Get rid of this and change NSAttributedString conversion so it doesn't use WebKitLegacy (cf. rdar://problem/30597352).
SOFT_LINK(WebKitLegacy, _WebCreateFragment, void, (WebCore::Document& document, NSAttributedString *string, WebCore::FragmentAndResources& result), (document, string, result))

namespace WebCore {

static FragmentAndResources createFragment(Frame& frame, NSAttributedString *string)
{
    // FIXME: The algorithm to convert an attributed string into HTML should be implemented here in WebCore.
    // For now, though, we call into WebKitLegacy, which in turn calls into AppKit/TextKit.
    FragmentAndResources result;
    _WebCreateFragment(*frame.document(), string, result);
    return result;
}

RefPtr<DocumentFragment> createFragmentAndAddResources(Frame& frame, NSAttributedString *string)
{
    if (!frame.page() || !frame.document())
        return nullptr;

    auto& document = *frame.document();
    if (!document.isHTMLDocument() || !string)
        return nullptr;

    bool wasDeferringCallbacks = frame.page()->defersLoading();
    if (!wasDeferringCallbacks)
        frame.page()->setDefersLoading(true);

    auto& cachedResourceLoader = document.cachedResourceLoader();
    bool wasImagesEnabled = cachedResourceLoader.imagesEnabled();
    if (wasImagesEnabled)
        cachedResourceLoader.setImagesEnabled(false);

    auto fragmentAndResources = createFragment(frame, string);
    if (auto* loader = frame.loader().documentLoader()) {
        for (auto& resource : fragmentAndResources.resources)
            loader->addArchiveResource(WTFMove(resource));
    }

    if (wasImagesEnabled)
        cachedResourceLoader.setImagesEnabled(true);
    if (!wasDeferringCallbacks)
        frame.page()->setDefersLoading(false);

    return WTFMove(fragmentAndResources.fragment);
}

bool WebContentReader::readWebArchive(SharedBuffer* buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    if (!frame.document())
        return false;

    if (!buffer)
        return false;

    auto archive = LegacyWebArchive::create(URL(), *buffer);
    if (!archive)
        return false;

    RefPtr<ArchiveResource> mainResource = archive->mainResource();
    if (!mainResource)
        return false;

    auto type = mainResource->mimeType();
    if (!frame.loader().client().canShowMIMETypeAsHTML(type))
        return false;

    // FIXME: The code in createFragmentAndAddResources calls setDefersLoading(true). Don't we need that here?
    if (DocumentLoader* loader = frame.loader().documentLoader())
        loader->addAllArchiveResources(*archive);

    auto markupString = String::fromUTF8(mainResource->data().data(), mainResource->data().size());
    addFragment(createFragmentFromMarkup(*frame.document(), markupString, mainResource->url(), DisallowScriptingAndPluginContent));
    return true;
}

bool WebContentReader::readRTFD(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    auto fragment = createFragmentAndAddResources(frame, adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]).get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentReader::readRTF(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    auto fragment = createFragmentAndAddResources(frame, adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]).get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentReader::readPlainText(const String& text)
{
    if (!allowPlainText)
        return false;

    addFragment(createFragmentFromText(context, [text precomposedStringWithCanonicalMapping]));

    madeFragmentFromPlainText = true;
    return true;
}

}
