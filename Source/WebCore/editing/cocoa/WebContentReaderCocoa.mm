/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
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
#import "DeprecatedGlobalSettings.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentLoader.h"
#import "Editor.h"
#import "EditorClient.h"
#import "ElementInlines.h"
#import "File.h"
#import "FrameLoader.h"
#import "HTMLAnchorElement.h"
#import "HTMLAttachmentElement.h"
#import "HTMLBRElement.h"
#import "HTMLBodyElement.h"
#import "HTMLDivElement.h"
#import "HTMLIFrameElement.h"
#import "HTMLImageElement.h"
#import "HTMLObjectElement.h"
#import "HTMLPictureElement.h"
#import "HTMLSourceElement.h"
#import "LegacyWebArchive.h"
#import "LocalFrame.h"
#import "LocalFrameLoaderClient.h"
#import "MIMETypeRegistry.h"
#import "Page.h"
#import "PublicURLManager.h"
#import "Quirks.h"
#import "Range.h"
#import "RenderView.h"
#import "SerializedAttachmentData.h"
#import "Settings.h"
#import "SocketProvider.h"
#import "TypedElementDescendantIteratorInlines.h"
#import "UTIUtilities.h"
#import "WebArchiveResourceFromNSAttributedString.h"
#import "WebArchiveResourceWebResourceHandler.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/FileSystem.h>
#import <wtf/SoftLinking.h>
#import <wtf/URLParser.h>

#if PLATFORM(MAC)
#import "LocalDefaultSystemAppearance.h"
#endif

// FIXME: Do we really need to keep the legacy code path around for watchOS and tvOS?
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
@interface NSAttributedString ()
- (NSString *)_htmlDocumentFragmentString:(NSRange)range documentAttributes:(NSDictionary *)attributes subresources:(NSArray **)subresources;
@end
#else
SOFT_LINK_PRIVATE_FRAMEWORK(WebKitLegacy)
SOFT_LINK(WebKitLegacy, _WebCreateFragment, void, (WebCore::Document& document, NSAttributedString *string, WebCore::FragmentAndResources& result), (document, string, result))
#endif

namespace WebCore {

#if PLATFORM(MACCATALYST)

static FragmentAndResources createFragmentInternal(LocalFrame&, NSAttributedString *, OptionSet<FragmentCreationOptions>)
{
    return { };
}

#elif !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

static NSDictionary *attributesForAttributedStringConversion(bool useInterchangeNewlines)
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
    if (!DeprecatedGlobalSettings::attachmentElementEnabled())
        [excludedElements addObject:@"object"];
#endif

    NSURL *baseURL = URL::fakeURLWithRelativePart(emptyString());

    // The output base URL needs +1 refcount to work around the fact that NSHTMLReader over-releases it.
    CFRetain((__bridge CFTypeRef)baseURL);

    return @{
        NSExcludedElementsDocumentAttribute: excludedElements.get(),
        @"InterchangeNewline": @(useInterchangeNewlines),
        @"CoalesceTabSpans": @YES,
        @"OutputBaseURL": baseURL,
        @"WebResourceHandler": adoptNS([WebArchiveResourceWebResourceHandler new]).get(),
    };
}

static FragmentAndResources createFragmentInternal(LocalFrame& frame, NSAttributedString *string, OptionSet<FragmentCreationOptions> fragmentCreationOptions)
{
    FragmentAndResources result;
    Document& document = *frame.document();

#if PLATFORM(MAC)
    auto* view = frame.view();
    LocalDefaultSystemAppearance localAppearance(view ? view->useDarkAppearance() : false);
#endif

    NSArray *subresources = nil;
    NSString *fragmentString = [string _htmlDocumentFragmentString:NSMakeRange(0, [string length]) documentAttributes:attributesForAttributedStringConversion(!fragmentCreationOptions.contains(FragmentCreationOptions::NoInterchangeNewlines)) subresources:&subresources];

    auto fragment = DocumentFragment::create(document);
    auto dummyBodyToForceInBodyInsertionMode = HTMLBodyElement::create(document);
    auto markup = fragmentCreationOptions.contains(FragmentCreationOptions::SanitizeMarkup) ? sanitizeMarkup(fragmentString) : String(fragmentString);
    fragment->parseHTML(markup, dummyBodyToForceInBodyInsertionMode, { });

    result.fragment = WTFMove(fragment);
    for (WebArchiveResourceFromNSAttributedString *resource in subresources)
        result.resources.append(*resource->resource);

    return result;
}

#else

// FIXME: Do we really need to keep this legacy code path around for watchOS and tvOS?
static FragmentAndResources createFragmentInternal(LocalFrame& frame, NSAttributedString *string, OptionSet<FragmentCreationOptions>)
{
    FragmentAndResources result;
    _WebCreateFragment(*frame.document(), string, result);
    return result;
}

#endif

class DeferredLoadingScope {
public:
    DeferredLoadingScope(LocalFrame& frame)
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
    Ref<LocalFrame> m_frame;
    Ref<CachedResourceLoader> m_cachedResourceLoader;
    bool m_didEnabledDeferredLoading { false };
    bool m_didDisableImage { false };
};

static bool shouldReplaceSubresourceURLWithBlobDuringSanitization(const URL& url)
{
    return !url.protocolIsData() && !url.protocolIsInHTTPFamily();
}

static bool shouldReplaceRichContentWithAttachments()
{
#if ENABLE(ATTACHMENT_ELEMENT)
    return DeprecatedGlobalSettings::attachmentElementEnabled();
#else
    return false;
#endif
}

#if ENABLE(ATTACHMENT_ELEMENT)

static String mimeTypeFromContentType(const String& contentType)
{
    if (contentType == String(UTTypeVCard.identifier)) {
        // CoreServices erroneously reports that "public.vcard" maps to "text/directory", rather
        // than either "text/vcard" or "text/x-vcard". Work around this by special casing the
        // "public.vcard" UTI type. See <rdar://problem/49478229> for more detail.
        return "text/vcard"_s;
    }

    return isDeclaredUTI(contentType) ? MIMETypeFromUTI(contentType) : contentType;
}

static bool contentTypeIsSuitableForInlineImageRepresentation(const String& contentType)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeTypeFromContentType(contentType));
}

static bool supportsClientSideAttachmentData(const LocalFrame& frame)
{
    if (auto* client = frame.editor().client())
        return client->supportsClientSideAttachmentData();

    return false;
}

#endif

static Ref<DocumentFragment> createFragmentForImageAttachment(LocalFrame& frame, Document& document, Ref<FragmentedSharedBuffer>&& buffer, const String& contentType, PresentationSize preferredSize)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
    // FIXME: This fallback image name needs to be a localized string.
    constexpr ASCIILiteral defaultImageAttachmentName { "image"_s };

    auto fragment = document.createDocumentFragment();
    if (supportsClientSideAttachmentData(frame)) {
        frame.editor().registerAttachmentIdentifier(attachment->ensureUniqueIdentifier(), contentType, defaultImageAttachmentName, buffer.copyRef());
        if (contentTypeIsSuitableForInlineImageRepresentation(contentType)) {
            auto image = HTMLImageElement::create(document);
            image->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { DOMURL::createObjectURL(document, Blob::create(&document, buffer->extractData(), contentType)) });
            image->setAttachmentElement(WTFMove(attachment));
            if (preferredSize.width)
                image->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(*preferredSize.width));
            if (preferredSize.height)
                image->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(*preferredSize.height));
            fragment->appendChild(WTFMove(image));
        } else {
            attachment->updateAttributes(buffer->size(), AtomString { contentType }, defaultImageAttachmentName);
            fragment->appendChild(WTFMove(attachment));
        }
    } else {
        attachment->setFile(File::create(&document, Blob::create(&document, buffer->extractData(), contentType), defaultImageAttachmentName), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
        fragment->appendChild(WTFMove(attachment));
    }
    return fragment;
#else
    UNUSED_PARAM(blob);
    return document.createDocumentFragment();
#endif
}

static void replaceRichContentWithAttachments(LocalFrame& frame, DocumentFragment& fragment, const Vector<Ref<ArchiveResource>>& subresources)
{
#if ENABLE(ATTACHMENT_ELEMENT)
    struct AttachmentInsertionInfo {
        String fileName;
        String contentType;
        Ref<SharedBuffer> data;
        Ref<Element> originalElement;
    };

    ASSERT(DeprecatedGlobalSettings::attachmentElementEnabled());
    if (subresources.isEmpty())
        return;

    // FIXME: Handle resources in subframe archives.
    UncheckedKeyHashMap<AtomString, Ref<ArchiveResource>> urlToResourceMap;
    for (auto& subresource : subresources)
        urlToResourceMap.set(AtomString { subresource->url().string() }, subresource.copyRef());

    Vector<SerializedAttachmentData> serializedAttachmentData;
    for (auto& attachment : descendantsOfType<HTMLAttachmentElement>(fragment)) {
        auto resourceURL = HTMLAttachmentElement::archiveResourceURL(attachment.uniqueIdentifier());
        auto resourceURLAtom = resourceURL.string().toExistingAtomString();
        if (resourceURLAtom.isNull())
            continue;
        auto resourceEntry = urlToResourceMap.find(resourceURLAtom);
        if (resourceEntry == urlToResourceMap.end())
            continue;

        auto& resource = resourceEntry->value;
        serializedAttachmentData.append({ attachment.uniqueIdentifier(), resource->mimeType(), resource->data().makeContiguous() });
    }

    if (!serializedAttachmentData.isEmpty())
        frame.editor().registerAttachments(WTFMove(serializedAttachmentData));

    Vector<Ref<Element>> elementsToRemove;
    Vector<AttachmentInsertionInfo> attachmentInsertionInfo;
    for (auto& image : descendantsOfType<HTMLImageElement>(fragment)) {
        auto resourceURLString = image.attributeWithoutSynchronization(HTMLNames::srcAttr);
        if (resourceURLString.isEmpty())
            continue;

        auto resource = urlToResourceMap.find(resourceURLString);
        if (resource == urlToResourceMap.end())
            continue;

        String name = image.attributeWithoutSynchronization(HTMLNames::altAttr);
        if (name.isEmpty())
            name = URL({ }, resourceURLString).lastPathComponent().toString();
        if (name.isEmpty())
            name = "media"_s;

        attachmentInsertionInfo.append({ WTFMove(name), resource->value->mimeType(), resource->value->data().makeContiguous(), image });
    }

    for (auto& object : descendantsOfType<HTMLObjectElement>(fragment)) {
        auto resourceURLString = object.attributeWithoutSynchronization(HTMLNames::dataAttr);
        if (resourceURLString.isEmpty()) {
            elementsToRemove.append(object);
            continue;
        }

        auto resource = urlToResourceMap.find(resourceURLString);
        if (resource == urlToResourceMap.end())
            continue;

        String name = URL({ }, resourceURLString).lastPathComponent().toString();
        if (name.isEmpty())
            name = "file"_s;

        attachmentInsertionInfo.append({ WTFMove(name), resource->value->mimeType(), resource->value->data().makeContiguous(), object });
    }

    for (auto& source : descendantsOfType<HTMLSourceElement>(fragment)) {
        auto resourceURLString = source.attributeWithoutSynchronization(HTMLNames::srcsetAttr);
        if (resourceURLString.isEmpty()) {
            elementsToRemove.append(source);
            continue;
        }

        auto resource = urlToResourceMap.find(resourceURLString);
        if (resource == urlToResourceMap.end())
            continue;

        String name = URL({ }, resourceURLString).lastPathComponent().toString();
        if (name.isEmpty())
            name = "media"_s;

        attachmentInsertionInfo.append({ WTFMove(name), resource->value->mimeType(), resource->value->data().makeContiguous(), source });
    }

    for (auto& info : attachmentInsertionInfo) {
        auto originalElement = WTFMove(info.originalElement);
        RefPtr parent { originalElement->parentNode() };
        if (!parent)
            continue;

        // If the filename begins with this sentinel value, this means that an existing attachment should be used.
        // See `HTMLConverter.mm` for more details.
        if (info.fileName.startsWith(WebContentReader::placeholderAttachmentFilenamePrefix)) {
            RefPtr document = frame.document();
            if (RefPtr existingAttachment = document->attachmentForIdentifier({ info.data->span() })) {
                parent->replaceChild(*existingAttachment.get(), WTFMove(originalElement));
                continue;
            }
        }

        auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, fragment.document());
        if (supportsClientSideAttachmentData(frame)) {
            if (RefPtr image = dynamicDowncast<HTMLImageElement>(originalElement); image && contentTypeIsSuitableForInlineImageRepresentation(info.contentType)) {
                RefPtr document = frame.document();
                image->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { DOMURL::createObjectURL(*document, Blob::create(document.get(), info.data->copyData(), info.contentType)) });
                image->setAttachmentElement(attachment.copyRef());
            } else if (RefPtr source = dynamicDowncast<HTMLSourceElement>(originalElement); source && contentTypeIsSuitableForInlineImageRepresentation(info.contentType)) {
                RefPtr document = frame.document();
                source->setAttributeWithoutSynchronization(HTMLNames::srcsetAttr, AtomString { DOMURL::createObjectURL(*document, Blob::create(document.get(), info.data->copyData(), info.contentType)) });
                source->setAttachmentElement(attachment.copyRef());
            } else {
                attachment->updateAttributes(info.data->size(), AtomString { info.contentType }, AtomString { info.fileName });
                parent->replaceChild(attachment, WTFMove(originalElement));
            }
            frame.editor().registerAttachmentIdentifier(attachment->ensureUniqueIdentifier(), WTFMove(info.contentType), WTFMove(info.fileName), WTFMove(info.data));
        } else {
            RefPtr document = frame.document();
            attachment->setFile(File::create(document.get(), Blob::create(document.get(), info.data->copyData(), WTFMove(info.contentType)), WTFMove(info.fileName)), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
            parent->replaceChild(WTFMove(attachment), WTFMove(originalElement));
        }
    }

    for (auto& elementToRemove : elementsToRemove)
        elementToRemove->remove();
#else
    UNUSED_PARAM(fragment);
    UNUSED_PARAM(subresources);
#endif
}

static void simplifyFragmentForSingleTextAttachment(NSAttributedString *string, DocumentFragment& fragment)
{
    auto stringLength = string.length;
    if (!stringLength || stringLength > 2)
        return;

    RetainPtr plainText = [string string];
    if ([plainText characterAtIndex:0] != objectReplacementCharacter)
        return;

    __block unsigned numberOfTextAttachments = 0;
    [string enumerateAttribute:NSAttachmentAttributeName inRange:NSMakeRange(0, 1) options:0 usingBlock:^(NSTextAttachment *attachment, NSRange, BOOL *) {
        if (attachment)
            numberOfTextAttachments++;
    }];

    if (numberOfTextAttachments != 1)
        return;

    if (stringLength == 2 && [plainText characterAtIndex:1] != newlineCharacter)
        return;

    RefPtr pictureOrImage = [&] -> RefPtr<HTMLElement> {
        for (Ref element : descendantsOfType<HTMLElement>(fragment)) {
            if (is<HTMLPictureElement>(element) || is<HTMLImageElement>(element))
                return WTFMove(element);
        }
        return { };
    }();

    if (!pictureOrImage)
        return;

    fragment.removeChildren();
    fragment.appendChild(pictureOrImage.releaseNonNull());
}

RefPtr<DocumentFragment> createFragment(LocalFrame& frame, NSAttributedString *string, OptionSet<FragmentCreationOptions> fragmentCreationOptions)
{
    if (!frame.page() || !frame.document())
        return nullptr;

    auto& document = *frame.document();
    if (!document.isHTMLDocument() || !string)
        return nullptr;

    DeferredLoadingScope scope(frame);
    auto fragmentAndResources = createFragmentInternal(frame, string, fragmentCreationOptions);
    if (!fragmentAndResources.fragment)
        return nullptr;

    if (fragmentCreationOptions.contains(FragmentCreationOptions::IgnoreResources))
        return fragmentAndResources.fragment;

    if (!DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
        if (DocumentLoader* loader = frame.loader().documentLoader()) {
            for (auto& resource : fragmentAndResources.resources)
                loader->addArchiveResource(resource.copyRef());
        }
        return WTFMove(fragmentAndResources.fragment);
    }

    if (shouldReplaceRichContentWithAttachments()) {
        replaceRichContentWithAttachments(frame, *fragmentAndResources.fragment, fragmentAndResources.resources);
        return WTFMove(fragmentAndResources.fragment);
    }

    UncheckedKeyHashMap<AtomString, AtomString> blobURLMap;
    for (auto& subresource : fragmentAndResources.resources) {
        auto blob = Blob::create(&document, subresource->data().copyData(), subresource->mimeType());
        String blobURL = DOMURL::createObjectURL(document, blob);
        blobURLMap.set(AtomString { subresource->url().string() }, AtomString { blobURL });
    }

    replaceSubresourceURLs(*fragmentAndResources.fragment, WTFMove(blobURLMap));
    simplifyFragmentForSingleTextAttachment(string, *fragmentAndResources.fragment);
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

    return MarkupAndArchive { String::fromUTF8(mainResource->data().makeContiguous()->span()), mainResource.releaseNonNull(), archive.releaseNonNull() };
}

static String sanitizeMarkupWithArchive(LocalFrame& frame, Document& destinationDocument, MarkupAndArchive& markupAndArchive, MSOListQuirks msoListQuirks, const std::function<bool(const String)>& canShowMIMETypeAsHTML)
{
    Ref page = createPageForSanitizingWebContent();
    auto* localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!localMainFrame)
        return String();

    Document* stagingDocument = localMainFrame->document();
    ASSERT(stagingDocument);
    auto fragment = createFragmentFromMarkup(*stagingDocument, markupAndArchive.markup, markupAndArchive.mainResource->url().string(), { });

    if (shouldReplaceRichContentWithAttachments()) {
        replaceRichContentWithAttachments(frame, fragment, markupAndArchive.archive->subresources());
        return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, markupAndArchive.markup);
    }

    UncheckedKeyHashMap<AtomString, AtomString> blobURLMap;
    for (const Ref<ArchiveResource>& subresource : markupAndArchive.archive->subresources()) {
        auto& subresourceURL = subresource->url();
        if (!shouldReplaceSubresourceURLWithBlobDuringSanitization(subresourceURL))
            continue;
        auto blob = Blob::create(&destinationDocument, subresource->data().copyData(), subresource->mimeType());
        String blobURL = DOMURL::createObjectURL(destinationDocument, blob);
        blobURLMap.set(AtomString { subresourceURL.string() }, AtomString { blobURL });
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
        if (!shouldReplaceSubresourceURLWithBlobDuringSanitization(subframeURL))
            continue;

        MarkupAndArchive subframeContent = { String::fromUTF8(subframeMainResource->data().makeContiguous()->span()),
            subframeMainResource.releaseNonNull(), subframeArchive.copyRef() };
        auto subframeMarkup = sanitizeMarkupWithArchive(frame, destinationDocument, subframeContent, MSOListQuirks::Disabled, canShowMIMETypeAsHTML);

        auto blob = Blob::create(&destinationDocument, Vector(subframeMarkup.utf8().span()), type);

        String subframeBlobURL = DOMURL::createObjectURL(destinationDocument, blob);
        blobURLMap.set(AtomString { subframeURL.string() }, AtomString { subframeBlobURL });
    }

    replaceSubresourceURLs(fragment.get(), WTFMove(blobURLMap));

    return sanitizedMarkupForFragmentInDocument(WTFMove(fragment), *stagingDocument, msoListQuirks, markupAndArchive.markup);
}

bool WebContentReader::readWebArchive(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (frame->settings().preferMIMETypeForImages() || !frame->document())
        return false;

    DeferredLoadingScope scope(frame);
    auto result = extractMarkupAndArchive(buffer, [&] (const String& type) {
        return frame->loader().client().canShowMIMETypeAsHTML(type);
    });
    if (!result)
        return false;
    
    Ref frameDocument = *frame->document();
    if (!DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
        m_fragment = createFragmentFromMarkup(frameDocument, result->markup, result->mainResource->url().string(), { });
        if (RefPtr loader = frame->loader().documentLoader())
            loader->addAllArchiveResources(result->archive.get());
        return true;
    }

    if (!shouldSanitize()) {
        m_fragment = createFragmentFromMarkup(frameDocument, result->markup, result->mainResource->url().string(), { });
        return true;
    }

    String sanitizedMarkup = sanitizeMarkupWithArchive(frame, frameDocument, *result, msoListQuirksForMarkup(), [&] (const String& type) {
        return frame->loader().client().canShowMIMETypeAsHTML(type);
    });
    m_fragment = createFragmentFromMarkup(frameDocument, sanitizedMarkup, aboutBlankURL().string(), { });

    return m_fragment;
}

bool WebContentMarkupReader::readWebArchive(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (!frame->document())
        return false;

    auto result = extractMarkupAndArchive(buffer, [&] (const String& type) {
        return frame->loader().client().canShowMIMETypeAsHTML(type);
    });
    if (!result)
        return false;

    if (!shouldSanitize()) {
        m_markup = result->markup;
        return true;
    }

    m_markup = sanitizeMarkupWithArchive(frame, *frame->protectedDocument(), *result, msoListQuirksForMarkup(), [&] (const String& type) {
        return frame->loader().client().canShowMIMETypeAsHTML(type);
    });

    return true;
}

static String stripMicrosoftPrefix(const String& string)
{
#if PLATFORM(MAC)
    // This code was added to make HTML paste from Microsoft Word on Mac work, back in 2004.
    // It's a simple-minded way to ignore the CF_HTML clipboard format, just skipping over the
    // description part and parsing the entire context plus fragment.
    if (string.startsWith("Version:"_s)) {
        size_t location = string.findIgnoringASCIICase("<html"_s);
        if (location != notFound)
            return string.substring(location);
    }
#endif
    return string;
}

bool WebContentReader::readHTML(const String& string)
{
    if (frame().settings().preferMIMETypeForImages() || !frame().document())
        return false;
    Ref document = *frame().document();

    String stringOmittingMicrosoftPrefix = stripMicrosoftPrefix(string);
    if (stringOmittingMicrosoftPrefix.isEmpty())
        return false;

    String markup;
    if (DeprecatedGlobalSettings::customPasteboardDataEnabled() && shouldSanitize()) {
        markup = sanitizeMarkup(stringOmittingMicrosoftPrefix, msoListQuirksForMarkup(), WTF::Function<void (DocumentFragment&)> { [] (DocumentFragment& fragment) {
            removeSubresourceURLAttributes(fragment, [](auto& url) {
                return url.protocolIsFile();
            });
        } });
    } else
        markup = stringOmittingMicrosoftPrefix;

    addFragment(createFragmentFromMarkup(document, markup, emptyString(), { }));
    return true;
}

bool WebContentMarkupReader::readHTML(const String& string)
{
    if (!frame().document())
        return false;

    String rawHTML = stripMicrosoftPrefix(string);
    if (shouldSanitize()) {
        m_markup = sanitizeMarkup(rawHTML, msoListQuirksForMarkup(), WTF::Function<void (DocumentFragment&)> { [] (DocumentFragment& fragment) {
            removeSubresourceURLAttributes(fragment, [](auto& url) {
                return url.protocolIsFile();
            });
        } });
    } else
        m_markup = rawHTML;

    return !m_markup.isEmpty();
}

bool WebContentReader::readRTFD(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (frame->settings().preferMIMETypeForImages() || !frame->document())
        return false;

    auto string = adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragment(frame, string.get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentMarkupReader::readRTFD(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (!frame->document())
        return false;
    auto string = adoptNS([[NSAttributedString alloc] initWithRTFD:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragment(frame, string.get());
    if (!fragment)
        return false;

    m_markup = serializeFragment(*fragment, SerializedNodes::SubtreeIncludingNode);
    return true;
}

bool WebContentReader::readRTF(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (frame->settings().preferMIMETypeForImages())
        return false;

    auto string = adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragment(frame, string.get());
    if (!fragment)
        return false;
    addFragment(fragment.releaseNonNull());

    return true;
}

bool WebContentMarkupReader::readRTF(SharedBuffer& buffer)
{
    Ref frame = this->frame();
    if (!frame->document())
        return false;
    auto string = adoptNS([[NSAttributedString alloc] initWithRTF:buffer.createNSData().get() documentAttributes:nullptr]);
    auto fragment = createFragment(frame, string.get());
    if (!fragment)
        return false;
    m_markup = serializeFragment(*fragment, SerializedNodes::SubtreeIncludingNode);
    return true;
}

bool WebContentReader::readPlainText(const String& text)
{
    if (!m_allowPlainText)
        return false;

    String precomposedString = [text precomposedStringWithCanonicalMapping];
    if (RefPtr page = frame().page())
        precomposedString = page->applyLinkDecorationFiltering(precomposedString, LinkDecorationFilteringTrigger::Paste);

    addFragment(createFragmentFromText(m_context, precomposedString));

    m_madeFragmentFromPlainText = true;
    return true;
}

bool WebContentReader::readImage(Ref<FragmentedSharedBuffer>&& buffer, const String& type, PresentationSize preferredPresentationSize)
{
    ASSERT(frame().document());
    Ref frame = this->frame();
    Ref document = *frame->document();
    if (document->quirks().shouldAvoidPastingImagesAsWebContent())
        return false;

    if (shouldReplaceRichContentWithAttachments())
        addFragment(createFragmentForImageAttachment(frame, document, WTFMove(buffer), type, preferredPresentationSize));
    else
        addFragment(createFragmentForImageAndURL(document, DOMURL::createObjectURL(document, Blob::create(document.ptr(), buffer->extractData(), type)), preferredPresentationSize));

    return m_fragment;
}

#if ENABLE(ATTACHMENT_ELEMENT)

static String typeForAttachmentElement(const String& contentType)
{
    if (contentType.isEmpty())
        return { };

    auto mimeType = mimeTypeFromContentType(contentType);
    return mimeType.isEmpty() ? contentType : mimeType;
}

static Ref<HTMLElement> attachmentForFilePath(LocalFrame& frame, const String& path, PresentationSize preferredSize, const String& explicitContentType)
{
    Ref document = *frame.document();
    auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
    if (!supportsClientSideAttachmentData(frame)) {
        attachment->setFile(File::create(document.ptr(), path), HTMLAttachmentElement::UpdateDisplayAttributes::Yes);
        return attachment;
    }

    auto fileType = FileSystem::fileTypeFollowingSymlinks(path);
    bool isDirectory = fileType == FileSystem::FileType::Directory;
    String contentType = typeForAttachmentElement(explicitContentType);
    if (contentType.isEmpty()) {
        if (isDirectory)
            contentType = UTTypeDirectory.identifier;
        else {
            contentType = File::contentTypeForFile(path);
            if (contentType.isEmpty())
                contentType = UTTypeData.identifier;
        }
    }

    std::optional<uint64_t> fileSizeForDisplay;
    if (fileType && !isDirectory)
        fileSizeForDisplay = FileSystem::fileSize(path).value_or(0);

    frame.editor().registerAttachmentIdentifier(attachment->ensureUniqueIdentifier(), contentType, path);

    if (!fileType)
        attachment->setAttributeWithoutSynchronization(HTMLNames::progressAttr, AtomString::number(0));
    else if (contentTypeIsSuitableForInlineImageRepresentation(contentType)) {
        auto image = HTMLImageElement::create(document);
        image->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { DOMURL::createObjectURL(document, File::create(document.ptr(), path)) });
        image->setAttachmentElement(WTFMove(attachment));
        if (preferredSize.width)
            image->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(*preferredSize.width));
        if (preferredSize.height)
            image->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(*preferredSize.height));
        return image;
    }

    attachment->updateAttributes(WTFMove(fileSizeForDisplay), AtomString { contentType }, AtomString { FileSystem::pathFileName(path) });
    return attachment;
}

static Ref<HTMLElement> attachmentForData(LocalFrame& frame, FragmentedSharedBuffer& buffer, const String& contentType, const AtomString& name, PresentationSize preferredSize)
{
    Ref document = *frame.document();
    auto attachment = HTMLAttachmentElement::create(HTMLNames::attachmentTag, document);
    auto attachmentType = typeForAttachmentElement(contentType);

    // FIXME: We should instead ask CoreServices for a preferred name corresponding to the given content type.
    static constexpr ASCIILiteral defaultAttachmentName = "file"_s;

    AtomString fileName;
    if (name.isEmpty())
        fileName = defaultAttachmentName;
    else
        fileName = name;

    if (!supportsClientSideAttachmentData(frame)) {
        attachment->setFile(File::create(document.ptr(), Blob::create(document.ptr(), buffer.copyData(), WTFMove(attachmentType)), fileName));
        return attachment;
    }

    frame.editor().registerAttachmentIdentifier(attachment->ensureUniqueIdentifier(), attachmentType, fileName, buffer);

    if (contentTypeIsSuitableForInlineImageRepresentation(attachmentType)) {
        auto image = HTMLImageElement::create(document);
        image->setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { DOMURL::createObjectURL(document, File::create(document.ptr(), Blob::create(document.ptr(), buffer.copyData(), WTFMove(attachmentType)), WTFMove(fileName))) });
        image->setAttachmentElement(WTFMove(attachment));
        if (preferredSize.width)
            image->setAttributeWithoutSynchronization(HTMLNames::widthAttr, AtomString::number(*preferredSize.width));
        if (preferredSize.height)
            image->setAttributeWithoutSynchronization(HTMLNames::heightAttr, AtomString::number(*preferredSize.height));
        return image;
    }

    attachment->updateAttributes({ buffer.size() }, AtomString { attachmentType }, WTFMove(fileName));
    return attachment;
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

bool WebContentReader::readFilePath(const String& path, PresentationSize preferredPresentationSize, const String& contentType)
{
    Ref frame = this->frame();
    if (path.isEmpty() || !frame->document())
        return false;

    Ref document = *frame->document();
    if (!m_fragment)
        m_fragment = document->createDocumentFragment();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (DeprecatedGlobalSettings::attachmentElementEnabled())
        m_fragment->appendChild(attachmentForFilePath(frame, path, preferredPresentationSize, contentType));
#endif

    return true;
}

bool WebContentReader::readURL(const URL& url, const String& title)
{
    if (url.isEmpty())
        return false;

    Ref frame = this->frame();
#if PLATFORM(IOS_FAMILY)
    // FIXME: This code shouldn't be accessing selection and changing the behavior.
    if (!frame->editor().client()->hasRichlyEditableSelection()) {
        if (readPlainText([(NSURL *)url absoluteString]))
            return true;
    }

    if ([(NSURL *)url isFileURL])
        return false;
#endif // PLATFORM(IOS_FAMILY)

    auto sanitizedURLString = [&] {
        if (RefPtr page = frame->page())
            return page->applyLinkDecorationFiltering(url, LinkDecorationFilteringTrigger::Paste);
        return url;
    }().string();

    Ref document = *frame->document();
    auto anchor = HTMLAnchorElement::create(document.get());
    anchor->setAttributeWithoutSynchronization(HTMLNames::hrefAttr, AtomString { sanitizedURLString });

    NSString *linkText = title.isEmpty() ? sanitizedURLString : title;
    anchor->appendChild(document->createTextNode([linkText precomposedStringWithCanonicalMapping]));

    auto newFragment = document->createDocumentFragment();
    if (m_fragment)
        newFragment->appendChild(HTMLBRElement::create(document.get()));
    newFragment->appendChild(anchor);
    addFragment(WTFMove(newFragment));
    return true;
}

bool WebContentReader::readDataBuffer(SharedBuffer& buffer, const String& type, const AtomString& name, PresentationSize preferredPresentationSize)
{
    if (buffer.isEmpty())
        return false;

    if (!shouldReplaceRichContentWithAttachments())
        return false;

    Ref frame = this->frame();
    RefPtr document { frame->document() };
    if (!document)
        return false;

    if (!m_fragment)
        m_fragment = document->createDocumentFragment();

#if ENABLE(ATTACHMENT_ELEMENT)
    protectedFragment()->appendChild(attachmentForData(frame, buffer, type, name, preferredPresentationSize));
#else
    UNUSED_PARAM(type);
    UNUSED_PARAM(name);
#endif
    return true;
}

}
