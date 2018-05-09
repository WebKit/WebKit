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
#import "BlobURL.h"
#import "CachedResourceLoader.h"
#import "DOMURL.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "File.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "HTMLAnchorElement.h"
#import "HTMLAttachmentElement.h"
#import "HTMLBRElement.h"
#import "HTMLBodyElement.h"
#import "HTMLIFrameElement.h"
#import "HTMLImageElement.h"
#import "HTMLObjectElement.h"
#import "LegacyWebArchive.h"
#import "Page.h"
#import "PublicURLManager.h"
#import "RuntimeEnabledFeatures.h"
#import "Settings.h"
#import "SocketProvider.h"
#import "TypedElementDescendantIterator.h"
#import "URLParser.h"
#import "UTIUtilities.h"
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
    RetainPtr<NSMutableArray> excludedElements = adoptNS([[NSMutableArray alloc] initWithObjects:
        // Omit style since we want style to be inline so the fragment can be easily inserted.
        @"style",
        // Omit xml so the result is not XHTML.
        @"xml",
        // Omit tags that will get stripped when converted to a fragment anyway.
        @"doctype", @"html", @"head", @"body",
        // Omit deprecated tags.
        @"applet", @"basefont", @"center", @"dir", @"font", @"menu", @"s", @"strike", @"u",
#if !ENABLE(ATTACHMENT_ELEMENT)
        // Omit object so no file attachments are part of the fragment.
        @"object",
#endif
        nil]);

#if ENABLE(ATTACHMENT_ELEMENT)
    if (!RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled())
        [excludedElements addObject:@"object"];
#endif

#if PLATFORM(IOS)
    static NSString * const NSExcludedElementsDocumentAttribute = @"ExcludedElements";
#endif

    return @{
        NSExcludedElementsDocumentAttribute: excludedElements.get(),
        @"InterchangeNewline": @YES,
        @"CoalesceTabSpans": @YES,
        @"OutputBaseURL": [(NSURL *)URL::fakeURLWithRelativePart(emptyString()) retain], // The value needs +1 refcount, as NSAttributedString over-releases it.
        @"WebResourceHandler": [[WebArchiveResourceWebResourceHandler new] autorelease],
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
        if (m_didDisableImage)
            m_cachedResourceLoader->setImagesEnabled(true);
        if (m_didEnabledDeferredLoading)
            m_frame->page()->setDefersLoading(false);
    }

private:
    Ref<Frame> m_frame;
    Ref<CachedResourceLoader> m_cachedResourceLoader;
    bool m_didEnabledDeferredLoading { false };
    bool m_didDisableImage { false };
};


static bool shouldReplaceSubresourceURL(const URL& url)
{
    return !(url.protocolIsInHTTPFamily() || url.protocolIsData());
}

static bool shouldReplaceRichContentWithAttachments()
{
#if ENABLE(ATTACHMENT_ELEMENT)
    return RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled();
#else
    return false;
#endif
}

static Ref<DocumentFragment> createFragmentForImageAttachment(Document& document, Ref<Blob>&& blob)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
    attachment->setFile(File::create(blob, AtomicString("image")), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
    attachment->updateDisplayMode(AttachmentDisplayMode::InPlace);

    auto fragment = document.createDocumentFragment();
    fragment->appendChild(attachment);

    return fragment;
#else
    UNUSED_PARAM(blob);
    return document.createDocumentFragment();
#endif
}

static void replaceRichContentWithAttachments(DocumentFragment& fragment, const Vector<Ref<ArchiveResource>>& subresources)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    struct AttachmentReplacementInfo {
        AttachmentDisplayMode displayMode;
        Ref<File> file;
        Ref<Element> elementToReplace;
    };

    ASSERT(RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled());
    if (subresources.isEmpty())
        return;

    // FIXME: Handle resources in subframe archives.
    HashMap<AtomicString, Ref<Blob>> urlToBlobMap;
    for (const Ref<ArchiveResource>& subresource : subresources) {
        auto& url = subresource->url();
        if (shouldReplaceSubresourceURL(url))
            urlToBlobMap.set(url.string(), Blob::create(subresource->data(), subresource->mimeType()));
    }

    Vector<Ref<Element>> elementsToRemove;
    Vector<AttachmentReplacementInfo> attachmentReplacementInfo;
    for (auto& image : descendantsOfType<HTMLImageElement>(fragment)) {
        auto resourceURLString = image.attributeWithoutSynchronization(HTMLNames::srcAttr);
        if (resourceURLString.isEmpty())
            continue;

        auto blob = urlToBlobMap.get(resourceURLString);
        if (!blob)
            continue;

        auto title = URLParser { resourceURLString }.result().lastPathComponent();
        if (title.isEmpty())
            title = AtomicString("media");

        attachmentReplacementInfo.append({ AttachmentDisplayMode::InPlace, File::create(*blob, title), image });
    }

    for (auto& object : descendantsOfType<HTMLObjectElement>(fragment)) {
        auto resourceURLString = object.attributeWithoutSynchronization(HTMLNames::dataAttr);
        if (resourceURLString.isEmpty()) {
            elementsToRemove.append(object);
            continue;
        }

        auto blob = urlToBlobMap.get(resourceURLString);
        if (!blob) {
            elementsToRemove.append(object);
            continue;
        }

        auto title = URLParser { resourceURLString }.result().lastPathComponent();
        if (title.isEmpty())
            title = AtomicString("file");

        attachmentReplacementInfo.append({ AttachmentDisplayMode::AsIcon, File::create(*blob, title), object });
    }

    for (auto& info : attachmentReplacementInfo) {
        auto file = WTFMove(info.file);
        auto elementToReplace = WTFMove(info.elementToReplace);
        auto parent = makeRefPtr(elementToReplace->parentNode());
        if (!parent)
            continue;

        auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, fragment.document());
        attachment->setFile(WTFMove(file), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
        attachment->updateDisplayMode(info.displayMode);
        parent->replaceChild(attachment, elementToReplace);
    }

    for (auto& elementToRemove : elementsToRemove)
        elementToRemove->remove();
#else
    UNUSED_PARAM(fragment);
    UNUSED_PARAM(subresources);
#endif
}

static void replaceSubresourceURLsWithURLsFromClient(DocumentFragment& fragment, const Vector<Ref<ArchiveResource>>& subresources, Vector<Ref<ArchiveResource>>& outUnreplacedResources)
{
    ASSERT(fragment.document().frame());
    auto& frame = *fragment.document().frame();
    HashMap<AtomicString, AtomicString> subresourceURLToClientURLMap;
    for (auto& subresource : subresources) {
        auto& originalURL = subresource->url();
        if (!shouldReplaceSubresourceURL(originalURL)) {
            outUnreplacedResources.append(subresource.copyRef());
            continue;
        }

        auto replacementURL = frame.editor().clientReplacementURLForResource(subresource->data(), subresource->mimeType());
        if (replacementURL.isEmpty()) {
            outUnreplacedResources.append(subresource.copyRef());
            continue;
        }

        subresourceURLToClientURLMap.set(originalURL.string(), replacementURL);
    }

    if (!subresourceURLToClientURLMap.isEmpty())
        replaceSubresourceURLs(fragment, WTFMove(subresourceURLToClientURLMap));
}

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

    if (!RuntimeEnabledFeatures::sharedFeatures().customPasteboardDataEnabled()) {
        if (DocumentLoader* loader = frame.loader().documentLoader()) {
            for (auto& resource : fragmentAndResources.resources)
                loader->addArchiveResource(resource.copyRef());
        }
        return WTFMove(fragmentAndResources.fragment);
    }

    Vector<Ref<ArchiveResource>> unreplacedResources;
    replaceSubresourceURLsWithURLsFromClient(*fragmentAndResources.fragment, fragmentAndResources.resources, unreplacedResources);

    if (shouldReplaceRichContentWithAttachments()) {
        replaceRichContentWithAttachments(*fragmentAndResources.fragment, unreplacedResources);
        return WTFMove(fragmentAndResources.fragment);
    }

    HashMap<AtomicString, AtomicString> blobURLMap;
    for (const Ref<ArchiveResource>& subresource : unreplacedResources) {
        auto blob = Blob::create(subresource->data(), subresource->mimeType());
        String blobURL = DOMURL::createObjectURL(document, blob);
        blobURLMap.set(subresource->url().string(), blobURL);
    }

    replaceSubresourceURLs(*fragmentAndResources.fragment, WTFMove(blobURLMap));
    return WTFMove(fragmentAndResources.fragment);
}

struct MarkupAndArchive {
    String markup;
    Ref<ArchiveResource> mainResource;
    Ref<Archive> archive;
};

static std::optional<MarkupAndArchive> extractMarkupAndArchive(SharedBuffer& buffer, const std::function<bool(const String)>& canShowMIMETypeAsHTML)
{
    auto archive = LegacyWebArchive::create(URL(), buffer);
    if (!archive)
        return std::nullopt;

    RefPtr<ArchiveResource> mainResource = archive->mainResource();
    if (!mainResource)
        return std::nullopt;

    auto type = mainResource->mimeType();
    if (!canShowMIMETypeAsHTML(type))
        return std::nullopt;

    return MarkupAndArchive { String::fromUTF8(mainResource->data().data(), mainResource->data().size()), mainResource.releaseNonNull(), archive.releaseNonNull() };
}

static String sanitizeMarkupWithArchive(Document& destinationDocument, MarkupAndArchive& markupAndArchive, MSOListQuirks msoListQuirks, const std::function<bool(const String)>& canShowMIMETypeAsHTML)
{
    auto page = createPageForSanitizingWebContent();
    Document* stagingDocument = page->mainFrame().document();
    ASSERT(stagingDocument);
    auto fragment = createFragmentFromMarkup(*stagingDocument, markupAndArchive.markup, markupAndArchive.mainResource->url(), DisallowScriptingAndPluginContent);

    Vector<Ref<ArchiveResource>> unreplacedResources;
    replaceSubresourceURLsWithURLsFromClient(fragment, markupAndArchive.archive->subresources(), unreplacedResources);

    if (shouldReplaceRichContentWithAttachments()) {
        replaceRichContentWithAttachments(fragment, unreplacedResources);
        return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, markupAndArchive.markup);
    }

    HashMap<AtomicString, AtomicString> blobURLMap;
    for (const Ref<ArchiveResource>& subresource : unreplacedResources) {
        auto& subresourceURL = subresource->url();
        if (!shouldReplaceSubresourceURL(subresourceURL))
            continue;
        auto blob = Blob::create(subresource->data(), subresource->mimeType());
        String blobURL = DOMURL::createObjectURL(destinationDocument, blob);
        blobURLMap.set(subresourceURL.string(), blobURL);
    }

    auto contentOrigin = SecurityOrigin::create(markupAndArchive.mainResource->url());
    for (const Ref<Archive>& subframeArchive : markupAndArchive.archive->subframeArchives()) {
        RefPtr<ArchiveResource> subframeMainResource = subframeArchive->mainResource();
        if (!subframeMainResource)
            continue;

        auto type = subframeMainResource->mimeType();
        if (!canShowMIMETypeAsHTML(type))
            continue;

        auto subframeURL = subframeMainResource->url();
        if (!shouldReplaceSubresourceURL(subframeURL))
            continue;

        MarkupAndArchive subframeContent = { String::fromUTF8(subframeMainResource->data().data(), subframeMainResource->data().size()),
            subframeMainResource.releaseNonNull(), subframeArchive.copyRef() };
        auto subframeMarkup = sanitizeMarkupWithArchive(destinationDocument, subframeContent, MSOListQuirks::Disabled, canShowMIMETypeAsHTML);

        CString utf8 = subframeMarkup.utf8();
        Vector<uint8_t> blobBuffer;
        blobBuffer.reserveCapacity(utf8.length());
        blobBuffer.append(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length());
        auto blob = Blob::create(WTFMove(blobBuffer), type);

        String subframeBlobURL = DOMURL::createObjectURL(destinationDocument, blob);
        blobURLMap.set(subframeURL.string(), subframeBlobURL);
    }

    replaceSubresourceURLs(fragment.get(), WTFMove(blobURLMap));

    return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, markupAndArchive.markup);
}

bool WebContentReader::readWebArchive(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages() || !frame.document())
        return false;

    DeferredLoadingScope scope(frame);
    auto result = extractMarkupAndArchive(buffer, [&] (const String& type) {
        return frame.loader().client().canShowMIMETypeAsHTML(type);
    });
    if (!result)
        return false;
    
    if (!RuntimeEnabledFeatures::sharedFeatures().customPasteboardDataEnabled()) {
        fragment = createFragmentFromMarkup(*frame.document(), result->markup, result->mainResource->url(), DisallowScriptingAndPluginContent);
        if (DocumentLoader* loader = frame.loader().documentLoader())
            loader->addAllArchiveResources(result->archive.get());
        return true;
    }

    if (!shouldSanitize()) {
        fragment = createFragmentFromMarkup(*frame.document(), result->markup, result->mainResource->url(), DisallowScriptingAndPluginContent);
        return true;
    }

    String sanitizedMarkup = sanitizeMarkupWithArchive(*frame.document(), *result, msoListQuirksForMarkup(), [&] (const String& type) {
        return frame.loader().client().canShowMIMETypeAsHTML(type);
    });
    fragment = createFragmentFromMarkup(*frame.document(), sanitizedMarkup, blankURL(), DisallowScriptingAndPluginContent);

    if (!fragment)
        return false;

    return true;
}

bool WebContentMarkupReader::readWebArchive(SharedBuffer& buffer)
{
    if (!frame.document())
        return false;

    auto result = extractMarkupAndArchive(buffer, [&] (const String& type) {
        return frame.loader().client().canShowMIMETypeAsHTML(type);
    });
    if (!result)
        return false;

    if (!shouldSanitize()) {
        markup = result->markup;
        return true;
    }

    markup = sanitizeMarkupWithArchive(*frame.document(), *result, msoListQuirksForMarkup(), [&] (const String& type) {
        return frame.loader().client().canShowMIMETypeAsHTML(type);
    });

    return true;
}

static String stripMicrosoftPrefix(const String& string)
{
#if PLATFORM(MAC)
    // This code was added to make HTML paste from Microsoft Word on Mac work, back in 2004.
    // It's a simple-minded way to ignore the CF_HTML clipboard format, just skipping over the
    // description part and parsing the entire context plus fragment.
    if (string.startsWith("Version:")) {
        size_t location = string.findIgnoringASCIICase("<html");
        if (location != notFound)
            return string.substring(location);
    }
#endif
    return string;
}

bool WebContentReader::readHTML(const String& string)
{
    if (frame.settings().preferMIMETypeForImages() || !frame.document())
        return false;
    Document& document = *frame.document();

    String stringOmittingMicrosoftPrefix = stripMicrosoftPrefix(string);
    if (stringOmittingMicrosoftPrefix.isEmpty())
        return false;

    String markup;
    if (RuntimeEnabledFeatures::sharedFeatures().customPasteboardDataEnabled() && shouldSanitize()) {
        markup = sanitizeMarkup(stringOmittingMicrosoftPrefix, msoListQuirksForMarkup(), WTF::Function<void (DocumentFragment&)> { [] (DocumentFragment& fragment) {
            removeSubresourceURLAttributes(fragment, [] (const URL& url) {
                return shouldReplaceSubresourceURL(url);
            });
        } });
    } else
        markup = stringOmittingMicrosoftPrefix;

    addFragment(createFragmentFromMarkup(document, markup, emptyString(), DisallowScriptingAndPluginContent));
    return true;
}

bool WebContentMarkupReader::readHTML(const String& string)
{
    if (!frame.document())
        return false;

    String rawHTML = stripMicrosoftPrefix(string);
    if (shouldSanitize()) {
        markup = sanitizeMarkup(rawHTML, msoListQuirksForMarkup(), WTF::Function<void (DocumentFragment&)> { [] (DocumentFragment& fragment) {
            removeSubresourceURLAttributes(fragment, [] (const URL& url) {
                return shouldReplaceSubresourceURL(url);
            });
        } });
    } else
        markup = rawHTML;

    return !markup.isEmpty();
}

bool WebContentReader::readRTFD(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages() || !frame.document())
        return false;

    auto string = adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragmentAndAddResources(frame, string.get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentMarkupReader::readRTFD(SharedBuffer& buffer)
{
    if (!frame.document())
        return false;
    auto string = adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragmentAndAddResources(frame, string.get());
    if (!fragment)
        return false;

    markup = createMarkup(*fragment);
    return true;
}

bool WebContentReader::readRTF(SharedBuffer& buffer)
{
    if (frame.settings().preferMIMETypeForImages())
        return false;

    auto string = adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragmentAndAddResources(frame, string.get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentMarkupReader::readRTF(SharedBuffer& buffer)
{
    if (!frame.document())
        return false;
    auto string = adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragmentAndAddResources(frame, string.get());
    if (!fragment)
        return false;
    markup = createMarkup(*fragment);
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
    ASSERT(frame.document());
    auto& document = *frame.document();

    auto replacementURL = frame.editor().clientReplacementURLForResource(buffer.copyRef(), isDeclaredUTI(type) ? MIMETypeFromUTI(type) : type);
    if (!replacementURL.isEmpty()) {
        addFragment(createFragmentForImageAndURL(document, replacementURL));
        return true;
    }

    auto blob = Blob::create(buffer.get(), type);
    if (shouldReplaceRichContentWithAttachments())
        addFragment(createFragmentForImageAttachment(document, WTFMove(blob)));
    else
        addFragment(createFragmentForImageAndURL(document, DOMURL::createObjectURL(document, blob)));

    return fragment;
}

bool WebContentReader::readFilePaths(const Vector<String>& paths)
{
    if (paths.isEmpty() || !frame.document())
        return false;

    auto& document = *frame.document();
    if (!fragment)
        fragment = document.createDocumentFragment();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled()) {
        for (auto& path : paths) {
            auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
            attachment->setFile(File::create(path), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
            fragment->appendChild(attachment);
        }
    }
#endif

    return true;
}

bool WebContentReader::readURL(const URL& url, const String& title)
{
    if (url.isEmpty())
        return false;

#if PLATFORM(IOS)
    // FIXME: This code shouldn't be accessing selection and changing the behavior.
    if (!frame.editor().client()->hasRichlyEditableSelection()) {
        if (readPlainText([(NSURL *)url absoluteString]))
            return true;
    }

    if ([(NSURL *)url isFileURL])
        return false;
#endif // PLATFORM(IOS)

    auto document = makeRef(*frame.document());
    auto anchor = HTMLAnchorElement::create(document.get());
    anchor->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, url.string());

    NSString *linkText = title.isEmpty() ? [(NSURL *)url absoluteString] : (NSString *)title;
    anchor->appendChild(document->createTextNode([linkText precomposedStringWithCanonicalMapping]));

    auto newFragment = document->createDocumentFragment();
    if (fragment)
        newFragment->appendChild(HTMLBRElement::create(document.get()));
    newFragment->appendChild(anchor);
    addFragment(WTFMove(newFragment));
    return true;
}

}
