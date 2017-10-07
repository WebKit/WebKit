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

#import "config.h"
#import "WebContentReader.h"

#import "ArchiveResource.h"
#import "Blob.h"
#import "CachedResourceLoader.h"
#import "DOMURL.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "HTMLBodyElement.h"
#import "HTMLImageElement.h"
#import "LegacyWebArchive.h"
#import "Page.h"
#import "Settings.h"
#import "WebArchiveResourceFromNSAttributedString.h"
#import "WebArchiveResourceWebResourceHandler.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/SoftLinking.h>

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300)
@interface NSAttributedString ()
- (NSString *)_htmlDocumentFragmentString:(NSRange)range documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end
#elif PLATFORM(IOS)
SOFT_LINK_PRIVATE_FRAMEWORK(WebKitLegacy)
#elif PLATFORM(MAC)
SOFT_LINK_FRAMEWORK_IN_UMBRELLA(WebKit, WebKitLegacy)
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300)
SOFT_LINK(WebKitLegacy, _WebCreateFragment, void, (WebCore::Document& document, NSAttributedString *string, WebCore::FragmentAndResources& result), (document, string, result))
#endif

namespace WebCore {

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300)

static NSDictionary *attributesForAttributedStringConversion()
{
    // This function needs to be kept in sync with identically named one in WebKitLegacy, which is used on older OS versions.
    RetainPtr<NSArray> excludedElements = adoptNS([[NSArray alloc] initWithObjects:
        // Omit style since we want style to be inline so the fragment can be easily inserted.
        @"style",
        // Omit xml so the result is not XHTML.
        @"xml",
        // Omit tags that will get stripped when converted to a fragment anyway.
        @"doctype", @"html", @"head", @"body",
        // Omit deprecated tags.
        @"applet", @"basefont", @"center", @"dir", @"font", @"menu", @"s", @"strike", @"u",
        // Omit object so no file attachments are part of the fragment.
        @"object", nil]);

#if PLATFORM(IOS)
    static NSString * const NSExcludedElementsDocumentAttribute = @"ExcludedElements";
#endif

    return @{
        NSExcludedElementsDocumentAttribute: excludedElements.get(),
        @"InterchangeNewline": @YES,
        @"CoalesceTabSpans": @YES,
        @"OutputBaseURL": [(NSURL *)URL::fakeURLWithRelativePart(emptyString()) retain], // The value needs +1 refcount, as NSAttributedString over-releases it.
        @"WebResourceHandler": [WebArchiveResourceWebResourceHandler new],
    };
}

static FragmentAndResources createFragment(Frame& frame, NSAttributedString *string)
{
    FragmentAndResources result;
    Document& document = *frame.document();

    NSArray *subresources = nil;
    NSString *fragmentString = [string _htmlDocumentFragmentString:NSMakeRange(0, [string length]) documentAttributes:attributesForAttributedStringConversion() subresources:&subresources];
    auto fragment = DocumentFragment::create(document);
    fragment->parseHTML(fragmentString, document.body(), DisallowScriptingAndPluginContent);

    result.fragment = WTFMove(fragment);
    for (WebArchiveResourceFromNSAttributedString *resource in subresources)
        result.resources.append(*resource->resource);

    return result;
}

#else

static FragmentAndResources createFragment(Frame& frame, NSAttributedString *string)
{
    FragmentAndResources result;
    _WebCreateFragment(*frame.document(), string, result);
    return result;
}

#endif

class DeferredLoadingScope {
public:
    DeferredLoadingScope(Frame& frame)
        : m_frame(frame)
        , m_cachedResourceLoader(frame.document()->cachedResourceLoader())
    {
        if (!frame.page()->defersLoading()) {
            frame.page()->setDefersLoading(true);
            m_didEnabledDeferredLoading = true;
        }

        if (m_cachedResourceLoader->imagesEnabled()) {
            m_cachedResourceLoader->setImagesEnabled(false);
            m_didDisableImage = true;
        }
    }

    ~DeferredLoadingScope()
    {
        if (m_didEnabledDeferredLoading)
            m_cachedResourceLoader->setImagesEnabled(true);
        if (m_didDisableImage)
            m_frame->page()->setDefersLoading(false);
    }

private:
    Ref<Frame> m_frame;
    Ref<CachedResourceLoader> m_cachedResourceLoader;
    bool m_didEnabledDeferredLoading { false };
    bool m_didDisableImage { false };
};

RefPtr<DocumentFragment> createFragmentAndAddResources(Frame& frame, NSAttributedString *string)
{
    if (!frame.page() || !frame.document())
        return nullptr;

    auto& document = *frame.document();
    if (!document.isHTMLDocument() || !string)
        return nullptr;

    DeferredLoadingScope scope(frame);
    auto fragmentAndResources = createFragment(frame, string);
    if (!fragmentAndResources.fragment)
        return nullptr;

    HashMap<AtomicString, AtomicString> blobURLMap;
    for (const Ref<ArchiveResource>& subresource : fragmentAndResources.resources) {
        auto blob = Blob::create(subresource->data(), subresource->mimeType());
        String blobURL = DOMURL::createObjectURL(document, blob);
        blobURLMap.set(subresource->url().string(), blobURL);
    }
    replaceSubresourceURLs(*fragmentAndResources.fragment, WTFMove(blobURLMap));

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

bool WebContentReader::readHTML(const String& string)
{
    String stringOmittingMicrosoftPrefix = string;
    
#if PLATFORM(MAC)
    // This code was added to make HTML paste from Microsoft Word on Mac work, back in 2004.
    // It's a simple-minded way to ignore the CF_HTML clipboard format, just skipping over the
    // description part and parsing the entire context plus fragment.
    if (string.startsWith("Version:")) {
        size_t location = string.findIgnoringCase("<html");
        if (location != notFound)
            stringOmittingMicrosoftPrefix = string.substring(location);
    }
#endif

    if (stringOmittingMicrosoftPrefix.isEmpty())
        return false;

    if (!frame.document())
        return false;
    Document& document = *frame.document();

    addFragment(createFragmentFromMarkup(document, stringOmittingMicrosoftPrefix, emptyString(), DisallowScriptingAndPluginContent));
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

bool WebContentReader::readImage(Ref<SharedBuffer>&& buffer, const String& type)
{
    auto blob = Blob::create(buffer.get(), type);
    ASSERT(frame.document());
    auto& document = *frame.document();
    String blobURL = DOMURL::createObjectURL(document, blob);
    addFragment(createFragmentForImageAndURL(document, blobURL));
    return true;
}

}
