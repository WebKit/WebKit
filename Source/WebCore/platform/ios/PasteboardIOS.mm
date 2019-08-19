/*
 * Copyright (C) 2007, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#import "DragData.h"
#import "Image.h"
#import "NotImplemented.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "RuntimeEnabledFeatures.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebNSAttributedStringExtras.h"
#import <MobileCoreServices/MobileCoreServices.h>
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

#if ENABLE(DRAG_SUPPORT)

Pasteboard::Pasteboard(const String& pasteboardName)
    : m_pasteboardName(pasteboardName)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName))
{
}

void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
    notImplemented();
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return makeUnique<Pasteboard>("data interaction pasteboard");
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return makeUnique<Pasteboard>(dragData.pasteboardName());
}

#endif

static long changeCountForPasteboard(const String& pasteboardName = { })
{
    return platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName);
}

// FIXME: Does this need to be declared in the header file?
WEBCORE_EXPORT NSString *WebArchivePboardType = @"Apple Web Archive pasteboard type";
NSString *UIColorPboardType = @"com.apple.uikit.color";

Pasteboard::Pasteboard()
    : m_changeCount(0)
{
}

Pasteboard::Pasteboard(long changeCount)
    : m_changeCount(changeCount)
{
}

void Pasteboard::writeMarkup(const String&)
{
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return makeUnique<Pasteboard>(changeCountForPasteboard());
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(content, m_pasteboardName);
}

String Pasteboard::resourceMIMEType(NSString *mimeType)
{
    return UTIFromMIMEType(mimeType);
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(pasteboardImage, m_pasteboardName);
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    // FIXME: We vend "public.text" here for backwards compatibility with pre-iOS 11 apps. In the future, we should stop vending this UTI,
    // and instead set data for concrete plain text types. See <https://bugs.webkit.org/show_bug.cgi?id=173317>.
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(kUTTypeText, text, m_pasteboardName);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(pasteboardURL, m_pasteboardName);
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
    platformStrategies()->pasteboardStrategy()->setColor(color, m_pasteboardName);
}

bool Pasteboard::canSmartReplace()
{
    return true;
}

void Pasteboard::read(PasteboardPlainText& text)
{
    PasteboardStrategy& strategy = *platformStrategies()->pasteboardStrategy();
    text.text = strategy.readStringFromPasteboard(0, kUTTypeURL, m_pasteboardName);
    if (!text.text.isEmpty()) {
        text.isURL = true;
        return;
    }

    text.text = strategy.readStringFromPasteboard(0, kUTTypePlainText, m_pasteboardName);
    if (text.text.isEmpty())
        text.text = strategy.readStringFromPasteboard(0, kUTTypeText, m_pasteboardName);

    text.isURL = false;
}

static NSArray* supportedImageTypes()
{
    return @[(__bridge NSString *)kUTTypePNG, (__bridge NSString *)kUTTypeTIFF, (__bridge NSString *)kUTTypeJPEG, (__bridge NSString *)kUTTypeGIF];
}

static bool isTypeAllowedByReadingPolicy(NSString *type, WebContentReadingPolicy policy)
{
    return policy == WebContentReadingPolicy::AnyType
        || [type isEqualToString:WebArchivePboardType]
        || [type isEqualToString:(__bridge NSString *)kUTTypeWebArchive]
        || [type isEqualToString:(__bridge NSString *)kUTTypeHTML]
        || [type isEqualToString:(__bridge NSString *)kUTTypeRTF]
        || [type isEqualToString:(__bridge NSString *)kUTTypeFlatRTFD];
}

Pasteboard::ReaderResult Pasteboard::readPasteboardWebContentDataForType(PasteboardWebContentReader& reader, PasteboardStrategy& strategy, NSString *type, const PasteboardItemInfo& itemInfo, int itemIndex)
{
    if ([type isEqualToString:WebArchivePboardType] || [type isEqualToString:(__bridge NSString *)kUTTypeWebArchive]) {
        auto buffer = strategy.readBufferFromPasteboard(itemIndex, type, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readWebArchive(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:(__bridge NSString *)kUTTypeHTML]) {
        String htmlString = strategy.readStringFromPasteboard(itemIndex, kUTTypeHTML, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !htmlString.isNull() && reader.readHTML(htmlString) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:(__bridge NSString *)kUTTypeVCard]) {
        // When dropping or pasting a virtual contact file in editable content, there's never a case where we
        // would want to dump the entire contents of the file as plain text. Instead, fall back on another
        // appropriate representation, such as a URL or plain text. For instance, in the case of an MKMapItem,
        // we would prefer to insert an Apple Maps link instead.
        return ReaderResult::DidNotReadType;
    }

#if !PLATFORM(MACCATALYST)
    if ([type isEqualToString:(__bridge NSString *)kUTTypeFlatRTFD]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, kUTTypeFlatRTFD, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readRTFD(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:(__bridge NSString *)kUTTypeRTF]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, kUTTypeRTF, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readRTF(*buffer) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }
#endif // !PLATFORM(MACCATALYST)

    if ([supportedImageTypes() containsObject:type]) {
        RefPtr<SharedBuffer> buffer = strategy.readBufferFromPasteboard(itemIndex, type, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return buffer && reader.readImage(buffer.releaseNonNull(), type, itemInfo.preferredPresentationSize) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if ([type isEqualToString:(__bridge NSString *)kUTTypeURL]) {
        String title;
        URL url = strategy.readURLFromPasteboard(itemIndex, m_pasteboardName, title);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !url.isNull() && reader.readURL(url, title) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if (UTTypeConformsTo((__bridge CFStringRef)type, kUTTypePlainText)) {
        String string = strategy.readStringFromPasteboard(itemIndex, kUTTypePlainText, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !string.isNull() && reader.readPlainText(string) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    if (UTTypeConformsTo((__bridge CFStringRef)type, kUTTypeText)) {
        String string = strategy.readStringFromPasteboard(itemIndex, kUTTypeText, m_pasteboardName);
        if (m_changeCount != changeCount())
            return ReaderResult::PasteboardWasChangedExternally;
        return !string.isNull() && reader.readPlainText(string) ? ReaderResult::ReadType : ReaderResult::DidNotReadType;
    }

    return ReaderResult::DidNotReadType;
}

static void readURLAlongsideAttachmentIfNecessary(PasteboardWebContentReader& reader, PasteboardStrategy& strategy, const String& typeIdentifier, const String& pasteboardName, int itemIndex)
{
    if (!UTTypeConformsTo(typeIdentifier.createCFString().get(), kUTTypeVCard))
        return;

    String title;
    auto url = strategy.readURLFromPasteboard(itemIndex, pasteboardName, title);
    if (!url.isEmpty())
        reader.readURL(url, title);
}

static bool prefersAttachmentRepresentation(const PasteboardItemInfo& info)
{
    auto contentTypeForHighestFidelityItem = info.contentTypeForHighestFidelityItem();
    if (contentTypeForHighestFidelityItem.isEmpty())
        return false;

    if (info.preferredPresentationStyle == PasteboardItemPresentationStyle::Inline)
        return false;

    return info.canBeTreatedAsAttachmentOrFile() || UTTypeConformsTo(contentTypeForHighestFidelityItem.createCFString().get(), kUTTypeVCard);
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy)
{
    reader.contentOrigin = readOrigin();
    if (respectsUTIFidelities()) {
        readRespectingUTIFidelities(reader, policy);
        return;
    }

    PasteboardStrategy& strategy = *platformStrategies()->pasteboardStrategy();

    int numberOfItems = strategy.getPasteboardItemsCount(m_pasteboardName);

    if (!numberOfItems)
        return;

    NSArray *types = supportedWebContentPasteboardTypes();
    int numberOfTypes = [types count];

#if ENABLE(ATTACHMENT_ELEMENT)
    bool canReadAttachment = policy == WebContentReadingPolicy::AnyType && RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled();
#else
    bool canReadAttachment = false;
#endif

    for (int i = 0; i < numberOfItems; i++) {
        auto info = strategy.informationForItemAtIndex(i, m_pasteboardName);
#if ENABLE(ATTACHMENT_ELEMENT)
        if (canReadAttachment && prefersAttachmentRepresentation(info)) {
            auto typeForFileUpload = info.contentTypeForHighestFidelityItem();
            if (auto buffer = strategy.readBufferFromPasteboard(i, typeForFileUpload, m_pasteboardName)) {
                readURLAlongsideAttachmentIfNecessary(reader, strategy, typeForFileUpload, m_pasteboardName, i);
                reader.readDataBuffer(*buffer, typeForFileUpload, info.suggestedFileName, info.preferredPresentationSize);
                continue;
            }
        }
#endif

        for (int typeIndex = 0; typeIndex < numberOfTypes; typeIndex++) {
            NSString *type = [types objectAtIndex:typeIndex];
            if (!isTypeAllowedByReadingPolicy(type, policy))
                continue;

            auto itemResult = readPasteboardWebContentDataForType(reader, strategy, type, info, i);
            if (itemResult == ReaderResult::PasteboardWasChangedExternally)
                return;

            if (itemResult == ReaderResult::ReadType)
                break;
        }
    }
}

bool Pasteboard::respectsUTIFidelities() const
{
    // For now, data interaction is the only feature that uses item-provider-based pasteboard representations.
    // In the future, we may need to consult the client layer to determine whether or not the pasteboard supports
    // item types ranked by fidelity.
    return m_pasteboardName == "data interaction pasteboard";
}

void Pasteboard::readRespectingUTIFidelities(PasteboardWebContentReader& reader, WebContentReadingPolicy policy)
{
    ASSERT(respectsUTIFidelities());
    auto& strategy = *platformStrategies()->pasteboardStrategy();
    for (NSUInteger index = 0, numberOfItems = strategy.getPasteboardItemsCount(m_pasteboardName); index < numberOfItems; ++index) {
#if ENABLE(ATTACHMENT_ELEMENT)
        auto info = strategy.informationForItemAtIndex(index, m_pasteboardName);
        auto attachmentFilePath = info.pathForHighestFidelityItem();
        bool canReadAttachment = policy == WebContentReadingPolicy::AnyType && RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled() && !attachmentFilePath.isEmpty();
        auto contentType = info.contentTypeForHighestFidelityItem();
        if (canReadAttachment && prefersAttachmentRepresentation(info)) {
            readURLAlongsideAttachmentIfNecessary(reader, strategy, contentType, m_pasteboardName, index);
            reader.readFilePath(WTFMove(attachmentFilePath), info.preferredPresentationSize, contentType);
            continue;
        }
#endif
        // Try to read data from each type identifier that this pasteboard item supports, and WebKit also recognizes. Type identifiers are
        // read in order of fidelity, as specified by each pasteboard item.
        ReaderResult result = ReaderResult::DidNotReadType;
        for (auto& type : info.contentTypesByFidelity) {
            if (!isTypeAllowedByReadingPolicy(type, policy))
                continue;

            result = readPasteboardWebContentDataForType(reader, strategy, type, info, index);
            if (result == ReaderResult::PasteboardWasChangedExternally)
                return;
            if (result == ReaderResult::ReadType)
                break;
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        if (canReadAttachment && result == ReaderResult::DidNotReadType)
            reader.readFilePath(WTFMove(attachmentFilePath), info.preferredPresentationSize, contentType);
#endif
    }
}

NSArray *Pasteboard::supportedWebContentPasteboardTypes()
{
    return @[
#if !PLATFORM(MACCATALYST)
        WebArchivePboardType,
#endif
        (__bridge NSString *)kUTTypeWebArchive,
#if !PLATFORM(MACCATALYST)
        (__bridge NSString *)kUTTypeFlatRTFD,
        (__bridge NSString *)kUTTypeRTF,
#endif
        (__bridge NSString *)kUTTypeHTML,
        (__bridge NSString *)kUTTypePNG,
        (__bridge NSString *)kUTTypeTIFF,
        (__bridge NSString *)kUTTypeJPEG,
        (__bridge NSString *)kUTTypeGIF,
        (__bridge NSString *)kUTTypeURL,
        (__bridge NSString *)kUTTypeText
    ];
}

NSArray *Pasteboard::supportedFileUploadPasteboardTypes()
{
    return @[ (__bridge NSString *)kUTTypeItem, (__bridge NSString *)kUTTypeContent, (__bridge NSString *)kUTTypeZipArchive ];
}

bool Pasteboard::hasData()
{
    return !!platformStrategies()->pasteboardStrategy()->getPasteboardItemsCount(m_pasteboardName);
}

static String utiTypeFromCocoaType(NSString *type)
{
    RetainPtr<CFStringRef> utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)type, NULL));
    if (!utiType)
        return String();
    return String(adoptCF(UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType)).get());
}

static RetainPtr<NSString> cocoaTypeFromHTMLClipboardType(const String& type)
{
    if (NSString *platformType = PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type)) {
        if (platformType.length)
            return platformType;
    }

    // Try UTI now.
    if (NSString *utiType = utiTypeFromCocoaType(type))
        return utiType;

    // No mapping, just pass the whole string though.
    return (NSString *)type;
}

void Pasteboard::clear(const String& type)
{
    // Since UIPasteboard enforces changeCount itself on writing, we don't check it here.

    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(cocoaType.get(), String(), m_pasteboardName);
}

void Pasteboard::clear()
{
    platformStrategies()->pasteboardStrategy()->writeToPasteboard(String(), String(), m_pasteboardName);
}

Vector<String> Pasteboard::readPlatformValuesAsStrings(const String& domType, long changeCount, const String& pasteboardName)
{
    auto& strategy = *platformStrategies()->pasteboardStrategy();

    // Grab the value off the pasteboard corresponding to the cocoaType.
    auto cocoaType = cocoaTypeFromHTMLClipboardType(domType);
    if (!cocoaType)
        return { };

    auto values = strategy.allStringsForType(cocoaType.get(), pasteboardName);
    if ([cocoaType isEqualToString:(__bridge NSString *)kUTTypePlainText]) {
        values = values.map([&] (auto& value) -> String {
            return [value precomposedStringWithCanonicalMapping];
        });
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (changeCount != changeCountForPasteboard(pasteboardName))
        return { };

    return values;
}

void Pasteboard::addHTMLClipboardTypesForCocoaType(ListHashSet<String>& resultTypes, const String& cocoaType)
{
    // UTI may not do these right, so make sure we get the right, predictable result.
    if ([cocoaType isEqualToString:(NSString *)kUTTypePlainText]
        || [cocoaType isEqualToString:(NSString *)kUTTypeUTF8PlainText]
        || [cocoaType isEqualToString:(NSString *)kUTTypeUTF16PlainText]) {
        resultTypes.add("text/plain"_s);
        return;
    }
    if ([cocoaType isEqualToString:(NSString *)kUTTypeURL]) {
        resultTypes.add("text/uri-list"_s);
        return;
    }
    if ([cocoaType isEqualToString:(NSString *)kUTTypeHTML]) {
        resultTypes.add("text/html"_s);
        // We don't return here for App compatibility.
    }
    if (Pasteboard::shouldTreatCocoaTypeAsFile(cocoaType))
        return;
    String utiType = utiTypeFromCocoaType(cocoaType);
    if (!utiType.isEmpty()) {
        resultTypes.add(utiType);
        return;
    }
    // No mapping, just pass the whole string though.
    resultTypes.add(cocoaType);
}

void Pasteboard::writeString(const String& type, const String& data)
{
    RetainPtr<NSString> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType)
        return;

    platformStrategies()->pasteboardStrategy()->writeToPasteboard(cocoaType.get(), data, m_pasteboardName);
}

Vector<String> Pasteboard::readFilePaths()
{
    Vector<String> filePaths;
    auto& strategy = *platformStrategies()->pasteboardStrategy();
    for (NSUInteger index = 0, numberOfItems = strategy.getPasteboardItemsCount(m_pasteboardName); index < numberOfItems; ++index) {
        // Currently, drag and drop is the only case on iOS where the "pasteboard" may contain file paths.
        auto filePath = strategy.informationForItemAtIndex(index, m_pasteboardName).pathForHighestFidelityItem();
        if (!filePath.isEmpty())
            filePaths.append(WTFMove(filePath));
    }
    return filePaths;
}

}

#endif // PLATFORM(IOS_FAMILY)
