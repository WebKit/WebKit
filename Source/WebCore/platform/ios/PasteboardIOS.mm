/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Pasteboard.h"

#if PLATFORM(IOS_FAMILY)

#import "CommonAtomStrings.h"
#import "DeprecatedGlobalSettings.h"
#import "DragData.h"
#import "NotImplemented.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebNSAttributedStringExtras.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <pal/ios/UIKitSoftLink.h>
#import <wtf/URL.h>
#import <wtf/text/StringHash.h>

@interface NSAttributedString (NSAttributedStringInternal)
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

namespace WebCore {

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context, StringImpl* pasteboardName)
    : m_context(WTF::move(context))
    , m_pasteboardName(pasteboardName)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName, m_context.get()))
{
}

#if ENABLE(DRAG_SUPPORT)

void Pasteboard::setDragImage(DragImage&&, const IntPoint&)
{
    notImplemented();
}

RefPtr<StringImpl> Pasteboard::nameOfDragPasteboard()
{
    return "drag and drop pasteboard"_si;
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context), Pasteboard::nameOfDragPasteboard());
}

std::unique_ptr<Pasteboard> Pasteboard::create(const DragData& dragData)
{
    return makeUnique<Pasteboard>(dragData.createPasteboardContext(), dragData.pasteboardName());
}

#endif // ENABLE(DRAG_SUPPORT)

static int64_t changeCountForPasteboard(StringImpl* pasteboardName = nullptr, const PasteboardContext* context = nullptr)
{
    return platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName, context);
}

// FIXME: Does this need to be declared in the header file?
WEBCORE_EXPORT NSString *WebArchivePboardType = @"Apple Web Archive pasteboard type";
NSString *UIColorPboardType = @"com.apple.uikit.color";
NSString *UIImagePboardType = @"com.apple.uikit.image";

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTF::move(context))
    , m_changeCount(0)
{
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context, int64_t changeCount)
    : m_context(WTF::move(context))
    , m_changeCount(changeCount)
{
}

void Pasteboard::writeMarkup(StringImpl*)
{
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context), StringImpl::create(PAL::get_UIKit_UIPasteboardNameGeneralSingleton()).get());
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(content, m_pasteboardName, context());
}

RefPtr<StringImpl> Pasteboard::resourceMIMEType(NSString *mimeType)
{
    return UTIFromMIMEType(StringImpl::create(mimeType).get());
}

void Pasteboard::write(PasteboardImage&& pasteboardImage)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(pasteboardImage, m_pasteboardName, context());
}

void Pasteboard::write(const PasteboardBuffer&)
{
}

void Pasteboard::writePlainText(StringImpl* text, SmartReplaceOption)
{
    // FIXME: We vend "public.text" here for backwards compatibility with pre-iOS 11 apps. In the future, we should stop vending this UTI,
    // and instead set data for concrete plain text types. See <https://bugs.webkit.org/show_bug.cgi?id=173317>.
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(StringImpl::create(UTTypeText.identifier).get(), text, m_pasteboardName, context());
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(pasteboardURL, m_pasteboardName, context());
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    // A trustworthy URL pasteboard type needs to be decided on
    // before we allow calls to this function. A page data transfer
    // should not use the same pasteboard type as this function for
    // URLs.
    ASSERT_NOT_REACHED();
}

void Pasteboard::write(const Color& color)
{
    platformStrategies()->pasteboardStrategy()->setColor(color, m_pasteboardName, context());
}

bool Pasteboard::canSmartReplace()
{
    return true;
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy allowURL, std::optional<size_t> itemIndex)
{
    auto itemIndexToQuery = itemIndex.value_or(0);

    CheckedPtr strategy = platformStrategies()->pasteboardStrategy();

    if (allowURL == PlainTextURLReadingPolicy::AllowURL) {
        text.text = strategy->readStringFromPasteboard(itemIndexToQuery, StringImpl::create(UTTypeURL.identifier).get(), m_pasteboardName, context());
        if (!isEmpty(text.text)) {
            text.isURL = true;
            return;
        }
    }

    // We ask for the "public.text" representation only as a legacy fallback, in case apps still directly write
    // plain text for this abstract UTI. In almost all cases, the more correct choice would be to write to
    // one of the concrete "public.plain-text" representations (e.g. UTTypeUTF8PlainText). In the future, we
    // should consider removing support for reading plain text from "public.text".
    text.text = strategy->readStringFromPasteboard(itemIndexToQuery, StringImpl::create(UTTypePlainText.identifier).get(), m_pasteboardName, context());
    if (isEmpty(text.text))
        text.text = strategy->readStringFromPasteboard(itemIndexToQuery, StringImpl::create(UTTypeText.identifier).get(), m_pasteboardName, context());

    text.isURL = false;
}

static NSArray* supportedImageTypes()
{
    return @[ UTTypePNG.identifier, UTTypeTIFF.identifier, UTTypeJPEG.identifier, UTTypeGIF.identifier ];
}

static bool isTypeAllowedByReadingPolicy(NSString *type, WebContentReadingPolicy policy)
{
    return policy == WebContentReadingPolicy::AnyType
        || [type isEqualToString:protect(WebArchivePboardType)]
        || [type isEqualToString:UTTypeWebArchive.identifier]
        || [type isEqualToString:UTTypeHTML.identifier]
        || [type isEqualToString:UTTypeRTF.identifier]
        || [type isEqualToString:UTTypeFlatRTFD.identifier];
}

Pasteboard::ReaderResult Pasteboard::readPasteboardWebContentDataForType(PasteboardWebContentReader& reader, PasteboardStrategy& strategy, NSString *type, const PasteboardItemInfo& itemInfo, int itemIndex)
{
    if ([type isEqualToString:protect(WebArchivePboardType)] || [type isEqualToString:UTTypeWebArchive.identifier]) {
        auto buffer = strategy.readBufferFromPasteboard(itemIndex, StringImpl::create(type).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readWebArchive(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:UTTypeHTML.identifier]) {
        RefPtr<StringImpl> htmlString = strategy.readStringFromPasteboard(itemIndex, StringImpl::create(UTTypeHTML.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !!htmlString && reader.readHTML(htmlString) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:UTTypeVCard.identifier]) {
        // When dropping or pasting a virtual contact file in editable content, there's never a case where we
        // would want to dump the entire contents of the file as plain text. Instead, fall back on another
        // appropriate representation, such as a URL or plain text. For instance, in the case of an MKMapItem,
        // we would prefer to insert an Apple Maps link instead.
        return ReaderResult::DidNotReadType;
    }

#if !PLATFORM(MACCATALYST)
    if ([type isEqualToString:UTTypeFlatRTFD.identifier]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, StringImpl::create(UTTypeFlatRTFD.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readRTFD(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:UTTypeRTF.identifier]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, StringImpl::create(UTTypeRTF.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readRTF(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }
#endif // !PLATFORM(MACCATALYST)

    if ([protect(supportedImageTypes()) containsObject:type]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, StringImpl::create(type).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readImage(buffer.releaseNonNull(), StringImpl::create(type).get(), itemInfo.preferredPresentationSize) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:UTTypeURL.identifier]) {
        RefPtr<StringImpl> title;
        URL url = strategy.readURLFromPasteboard(itemIndex, m_pasteboardName, title, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !url.isNull() && reader.readURL(url, title) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    RetainPtr utType = [UTType typeWithIdentifier:type];
    if ([utType conformsToType:UTTypePlainText]) {
        RefPtr<StringImpl> string = strategy.readStringFromPasteboard(itemIndex, StringImpl::create(UTTypePlainText.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !!string && reader.readPlainText(string) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([utType conformsToType:UTTypeText]) {
        RefPtr<StringImpl> string = strategy.readStringFromPasteboard(itemIndex, StringImpl::create(UTTypeText.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !!string && reader.readPlainText(string) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    return ReaderResult::DidNotReadType;
}

static void readURLAlongsideAttachmentIfNecessary(PasteboardWebContentReader& reader, PasteboardStrategy& strategy, StringImpl* typeIdentifier, StringImpl* pasteboardName, int itemIndex, const PasteboardContext* context)
{
    if (RetainPtr type = [UTType typeWithIdentifier:createNSString(typeIdentifier).get()]) {
        if (![type conformsToType:UTTypeVCard])
            return;
    }

    RefPtr<StringImpl> title;
    auto url = strategy.readURLFromPasteboard(itemIndex, pasteboardName, title, context);
    if (!url.isEmpty())
        reader.readURL(url, title);
}

static bool shouldTreatAsAttachmentByDefault(StringImpl* typeIdentifier)
{
    if (RetainPtr type = [UTType typeWithIdentifier:createNSString(typeIdentifier).get()]) {
        for (RetainPtr attachmentType : std::array { UTTypeVCard, UTTypePDF, UTTypeCalendarEvent }) {
            if ([type conformsToType:attachmentType])
                return true;
        }
    }
    return false;
}

static bool prefersAttachmentRepresentation(const PasteboardItemInfo& info)
{
    auto contentTypeForHighestFidelityItem = info.contentTypeForHighestFidelityItem();
    if (isEmpty(contentTypeForHighestFidelityItem))
        return false;

    if (info.preferredPresentationStyle == PasteboardItemPresentationStyle::Inline)
        return false;

    return info.canBeTreatedAsAttachmentOrFile() || shouldTreatAsAttachmentByDefault(contentTypeForHighestFidelityItem);
}

static NSArray *supportedWebContentPasteboardTypesWhenCustomDataIsPresent()
{
    static NeverDestroyed<RetainPtr<NSArray>> types = @[
#if !PLATFORM(MACCATALYST)
        WebArchivePboardType,
#endif
        UTTypeWebArchive.identifier,
        UTTypeHTML.identifier,
        UTTypePNG.identifier,
        UTTypeTIFF.identifier,
        UTTypeJPEG.identifier,
        UTTypeGIF.identifier,
        UTTypeURL.identifier,
        UTTypeText.identifier,
#if !PLATFORM(MACCATALYST)
        UTTypeFlatRTFD.identifier,
        UTTypeRTF.identifier
#endif
    ];
    return types->get();
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t> itemIndex)
{
    reader.setContentOrigin(readOrigin());
    if (respectsUTIFidelities()) {
        readRespectingUTIFidelities(reader, policy, itemIndex);
        return;
    }

    CheckedPtr strategy = platformStrategies()->pasteboardStrategy();

    size_t numberOfItems = strategy->getPasteboardItemsCount(m_pasteboardName, context());

    if (!numberOfItems)
        return;

#if ENABLE(ATTACHMENT_ELEMENT)
    bool canReadAttachment = policy == WebContentReadingPolicy::AnyType && DeprecatedGlobalSettings::attachmentElementEnabled();
#else
    bool canReadAttachment = false;
#endif

    for (size_t i = 0; i < numberOfItems; i++) {
        if (itemIndex && i != *itemIndex)
            continue;

        auto info = strategy->informationForItemAtIndex(i, m_pasteboardName, m_changeCount, context());
        if (!info)
            return;

#if ENABLE(ATTACHMENT_ELEMENT)
        if (canReadAttachment && prefersAttachmentRepresentation(*info)) {
            auto typeForFileUpload = info->contentTypeForHighestFidelityItem();
            if (auto buffer = strategy->readBufferFromPasteboard(i, typeForFileUpload, m_pasteboardName, context())) {
                readURLAlongsideAttachmentIfNecessary(reader, *strategy, typeForFileUpload, m_pasteboardName, i, context());
                reader.readDataBuffer(*buffer, typeForFileUpload, AtomStringImpl::add(info->suggestedFileName).get(), info->preferredPresentationSize);
                continue;
            }
        }
#endif

        RetainPtr<NSArray> typesToRead;
        if (info->platformTypesByFidelity.containsIf([&](auto& item) { return equal(item, StringImpl::create(PasteboardCustomData::cocoaType()).ptr()); }))
            typesToRead = supportedWebContentPasteboardTypesWhenCustomDataIsPresent();
        else
            typesToRead = supportedWebContentPasteboardTypes();

        for (NSString *type in typesToRead.get()) {
            if (!isTypeAllowedByReadingPolicy(type, policy))
                continue;

            auto itemResult = readPasteboardWebContentDataForType(reader, *strategy, type, *info, i);
            if (itemResult == ReaderResult::PasteboardWasChangedExternally)
                return;

            if (itemResult == ReaderResult::ReadType)
                break;
        }
    }
}

bool Pasteboard::respectsUTIFidelities() const
{
#if ENABLE(DRAG_SUPPORT)
    // FIXME: We should respect UTI fidelity for normal UIPasteboard-backed pasteboards as well.
    return equal(m_pasteboardName, Pasteboard::nameOfDragPasteboard());
#else
    return false;
#endif
}

void Pasteboard::readRespectingUTIFidelities(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t> itemIndex)
{
    ASSERT(respectsUTIFidelities());
    CheckedPtr strategy = platformStrategies()->pasteboardStrategy();
    for (NSUInteger index = 0, numberOfItems = strategy->getPasteboardItemsCount(m_pasteboardName, context()); index < numberOfItems; ++index) {
        if (itemIndex && index != *itemIndex)
            continue;

#if ENABLE(ATTACHMENT_ELEMENT)
        auto info = strategy->informationForItemAtIndex(index, m_pasteboardName, m_changeCount, context());
        if (!info)
            return;

        auto attachmentFilePath = info->pathForHighestFidelityItem();
        bool canReadAttachment = policy == WebContentReadingPolicy::AnyType && DeprecatedGlobalSettings::attachmentElementEnabled() && !isEmpty(attachmentFilePath);
        auto contentType = info->contentTypeForHighestFidelityItem();
        if (canReadAttachment && prefersAttachmentRepresentation(*info)) {
            readURLAlongsideAttachmentIfNecessary(reader, *strategy, contentType, m_pasteboardName, index, context());
            reader.readFilePath(WTF::move(attachmentFilePath), info->preferredPresentationSize, contentType);
            continue;
        }
#endif
        // Try to read data from each type identifier that this pasteboard item supports, and WebKit also recognizes. Type identifiers are
        // read in order of fidelity, as specified by each pasteboard item.
        ReaderResult result = ReaderResult::DidNotReadType;
        for (auto& type : info->platformTypesByFidelity) {
            RetainPtr nsType = createNSString(type);
            if (!isTypeAllowedByReadingPolicy(nsType.get(), policy))
                continue;

            result = readPasteboardWebContentDataForType(reader, *strategy, nsType.get(), *info, index);
            if (result == ReaderResult::PasteboardWasChangedExternally)
                return;
            if (result == ReaderResult::ReadType)
                break;
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        if (canReadAttachment && result == ReaderResult::DidNotReadType)
            reader.readFilePath(WTF::move(attachmentFilePath), info->preferredPresentationSize, contentType);
#endif
    }
}

NSArray *Pasteboard::supportedWebContentPasteboardTypes()
{
    static NeverDestroyed<RetainPtr<NSArray>> types = @[
#if !PLATFORM(MACCATALYST)
        WebArchivePboardType,
#endif
        UTTypeWebArchive.identifier,
#if !PLATFORM(MACCATALYST)
        UTTypeFlatRTFD.identifier,
        UTTypeRTF.identifier,
#endif
        UTTypeHTML.identifier,
        UTTypePNG.identifier,
        UTTypeTIFF.identifier,
        UTTypeJPEG.identifier,
        UTTypeGIF.identifier,
        UTTypeURL.identifier,
        UTTypeText.identifier
    ];
    return types->get();
}

NSArray *Pasteboard::supportedFileUploadPasteboardTypes()
{
    return @[ UTTypeItem.identifier, UTTypeContent.identifier, UTTypeZIP.identifier ];
}

bool Pasteboard::hasData()
{
    return !!platformStrategies()->pasteboardStrategy()->getPasteboardItemsCount(m_pasteboardName, context());
}

static RefPtr<StringImpl> utiTypeFromCocoaType(NSString *type)
{
    RetainPtr utiType = [UTType typeWithIdentifier:type];
    if (!utiType)
        utiType = [UTType typeWithMIMEType:type];
    if (!utiType)
        return nullptr;
    return StringImpl::create(utiType.get().identifier);
}

static RetainPtr<NSString> cocoaTypeFromHTMLClipboardType(StringImpl* type)
{
    RetainPtr nsType = createNSString(type);
    if (RetainPtr platformType = createNSString(PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(StringImpl::create(nsType.get()).get()))) {
        if (platformType.get().length)
            return platformType;
    }

    // Try UTI now.
    if (RetainPtr utiType = createNSString(utiTypeFromCocoaType(createNSString(type).get())))
        return utiType;

    // No mapping, just pass the whole string though.
    return nsType;
}

void Pasteboard::clear(StringImpl* type)
{
    // Since UIPasteboard enforces changeCount itself on writing, we don't check it here.

    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(StringImpl::create(cocoaType.get()).get(), nullString(), m_pasteboardName, context());
}

void Pasteboard::clear()
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(nullString(), nullString(), m_pasteboardName, context());
}

Vector<RefPtr<StringImpl>> Pasteboard::readPlatformValuesAsStrings(StringImpl* domType, int64_t changeCount, StringImpl* pasteboardName)
{
    CheckedPtr strategy = platformStrategies()->pasteboardStrategy();

    // Grab the value off the pasteboard corresponding to the cocoaType.
    auto cocoaType = cocoaTypeFromHTMLClipboardType(domType);
    if (!cocoaType)
        return { };

    auto values = strategy->allStringsForType(StringImpl::create(cocoaType.get()).get(), pasteboardName, context());
    if ([cocoaType isEqualToString:UTTypePlainText.identifier]) {
        values = values.map([&] (auto& value) -> RefPtr<StringImpl> {
            return StringImpl::create([createNSString(value) precomposedStringWithCanonicalMapping]);
        });
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (changeCount != changeCountForPasteboard(pasteboardName))
        return { };

    return values;
}

void Pasteboard::addHTMLClipboardTypesForCocoaType(OrderedHashSet<RefPtr<StringImpl>>& resultTypes, StringImpl* cocoaType)
{
    RetainPtr nsType = createNSString(cocoaType);
    // UTI may not do these right, so make sure we get the right, predictable result.
    if ([nsType isEqualToString:UTTypePlainText.identifier]
        || [nsType isEqualToString:UTTypeUTF8PlainText.identifier]
        || [nsType isEqualToString:UTTypeUTF16PlainText.identifier]) {
        resultTypes.add(textPlainContentTypeAtom());
        return;
    }
    if ([nsType isEqualToString:UTTypeURL.identifier]) {
        resultTypes.add("text/uri-list"_si);
        return;
    }
    if ([nsType isEqualToString:UTTypeHTML.identifier]) {
        resultTypes.add(textHTMLContentTypeAtom());
        // We don't return here for App compatibility.
    }

    if (Pasteboard::shouldTreatCocoaTypeAsFile(cocoaType))
        return;
    RefPtr<StringImpl> utiType = utiTypeFromCocoaType(nsType.get());
    if (!isEmpty(utiType)) {
        resultTypes.add(utiType);
        return;
    }
    // No mapping, just pass the whole string though.
    resultTypes.add(cocoaType);
}

void Pasteboard::writeString(StringImpl* type, StringImpl* data)
{
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(StringImpl::create(cocoaType.get()).get(), data, m_pasteboardName, context());
}

Vector<RefPtr<StringImpl>> Pasteboard::readFilePaths()
{
    Vector<RefPtr<StringImpl>> filePaths;
    CheckedPtr strategy = platformStrategies()->pasteboardStrategy();
    for (NSUInteger index = 0, numberOfItems = strategy->getPasteboardItemsCount(m_pasteboardName, context()); index < numberOfItems; ++index) {
        // Currently, drag and drop is the only case on iOS where the "pasteboard" may contain file paths.
        auto info = strategy->informationForItemAtIndex(index, m_pasteboardName, m_changeCount, context());
        if (!info)
            return { };

        auto filePath = info->pathForHighestFidelityItem();
        if (!isEmpty(filePath))
            filePaths.append(WTF::move(filePath));
    }
    return filePaths;
}

}

#endif // PLATFORM(IOS_FAMILY)
