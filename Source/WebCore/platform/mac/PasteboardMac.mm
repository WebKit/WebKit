/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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
#import "Pasteboard.h"

#if PLATFORM(MAC)

#import "CommonAtomStrings.h"
#import "DragData.h"
#import "Image.h"
#import "ImageAdapter.h"
#import "LegacyNSPasteboardTypes.h"
#import "LoaderNSURLExtras.h"
#import "MIMETypeRegistry.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebNSAttributedStringExtras.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/MallocSpan.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/URL.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/unicode/CharacterNames.h>

namespace WebCore {

const ASCIILiteral WebArchivePboardType = "Apple Web Archive pasteboard type"_s;
const ASCIILiteral WebURLNamePboardType = "public.url-name"_s;
const ASCIILiteral WebURLsWithTitlesPboardType = "WebURLsWithTitlesPboardType"_s;

const ASCIILiteral WebSmartPastePboardType = "NeXT smart paste pasteboard type"_s;
const ASCIILiteral WebURLPboardType = "public.url"_s;

static const Vector<String> writableTypesForURL()
{
    Vector<String> types;
    
    types.append(WebURLsWithTitlesPboardType);
    types.append(String(legacyURLPasteboardTypeSingleton()));
    types.append(WebURLPboardType);
    types.append(WebURLNamePboardType);
    types.append(String(legacyStringPasteboardTypeSingleton()));
    return types;
}

static Vector<String> writableTypesForImage()
{
    Vector<String> types;
    types.append(String(legacyTIFFPasteboardTypeSingleton()));
    types.appendVector(writableTypesForURL());
    types.append(String(legacyRTFDPasteboardTypeSingleton()));
    return types;
}

NSArray *Pasteboard::supportedFileUploadPasteboardTypes()
{
    return @[ legacyFilesPromisePasteboardTypeSingleton(), legacyFilenamesPasteboardTypeSingleton() ];
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTFMove(context))
    , m_pasteboardName(emptyString())
    , m_changeCount(0)
{
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context, const String& pasteboardName, const Vector<String>& promisedFilePaths)
    : m_context(WTFMove(context))
    , m_pasteboardName(pasteboardName)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, m_context.get()))
    , m_promisedFilePaths(promisedFilePaths)
{
    ASSERT(pasteboardName);
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTFMove(context), NSPasteboardNameGeneral);
}

#if ENABLE(DRAG_SUPPORT)
String Pasteboard::nameOfDragPasteboard()
{
    return NSPasteboardNameDrag;
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTFMove(context), NSPasteboardNameDrag);
}

std::unique_ptr<Pasteboard> Pasteboard::create(const DragData& dragData)
{
    return makeUnique<Pasteboard>(dragData.createPasteboardContext(), dragData.pasteboardName(), dragData.fileNames());
}
#endif

void Pasteboard::clear()
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(Vector<String>(), m_pasteboardName, context());
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    Vector<String> types;
    Vector<String> clientTypes;
    Vector<RefPtr<WebCore::SharedBuffer>> clientData;
    for (size_t it = 0; it < content.clientTypesAndData.size(); ++it) {
        clientTypes.append(content.clientTypesAndData[it].first);
        clientData.append(content.clientTypesAndData[it].second);
    }

    if (content.canSmartCopyOrDelete)
        types.append(WebSmartPastePboardType);
    if (content.dataInWebArchiveFormat) {
        types.append(WebArchivePboardType);
        types.append(UTTypeWebArchive.identifier);
    }
    if (content.dataInRTFDFormat)
        types.append(String(legacyRTFDPasteboardTypeSingleton()));
    if (content.dataInRTFFormat)
        types.append(String(legacyRTFPasteboardTypeSingleton()));
    if (!content.dataInHTMLFormat.isNull())
        types.append(String(legacyHTMLPasteboardTypeSingleton()));
    if (!content.dataInStringFormat.isNull())
        types.append(String(legacyStringPasteboardTypeSingleton()));
    types.appendVector(clientTypes);
    types.append(PasteboardCustomData::cocoaType());

    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());

    // FIXME: The following code should be refactored, such that it only requires a single call out to the client layer.
    // In WebKit2, this currently results in many unnecessary synchronous round-trip IPC messages.

    ASSERT(clientTypes.size() == clientData.size());
    for (size_t i = 0, size = clientTypes.size(); i < size; ++i)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(clientData[i].get(), clientTypes[i], m_pasteboardName, context());
    if (content.canSmartCopyOrDelete)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(nullptr, WebSmartPastePboardType, m_pasteboardName, context());

    bool didWriteWebArchive = false;
    if (RefPtr webArchive = content.webArchive) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->writeWebArchive(*webArchive, m_pasteboardName);
        didWriteWebArchive = !!m_changeCount;
    }
    if (!didWriteWebArchive && content.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), WebArchivePboardType, m_pasteboardName, context());

        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), UTTypeWebArchive.identifier, m_pasteboardName, context());
    }

    if (content.dataInRTFDFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFDFormat.get(), legacyRTFDPasteboardTypeSingleton(), m_pasteboardName, context());
    if (content.dataInRTFFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFFormat.get(), legacyRTFPasteboardTypeSingleton(), m_pasteboardName, context());
    if (!content.dataInHTMLFormat.isNull())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInHTMLFormat, legacyHTMLPasteboardTypeSingleton(), m_pasteboardName, context());
    if (!content.dataInStringFormat.isNull())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInStringFormat, legacyStringPasteboardTypeSingleton(), m_pasteboardName, context());

    PasteboardCustomData data;
    data.setOrigin(content.contentOrigin);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(data.createSharedBuffer().ptr(), PasteboardCustomData::cocoaType(), m_pasteboardName, context());

}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    Vector<String> types;
    types.append(legacyStringPasteboardTypeSingleton());
    if (smartReplaceOption == CanSmartReplace)
        types.append(WebSmartPastePboardType);

    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(text, legacyStringPasteboardTypeSingleton(), m_pasteboardName, context());
    if (smartReplaceOption == CanSmartReplace)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(0, WebSmartPastePboardType, m_pasteboardName, context());
}

static long writeURLForTypes(const Vector<String>& types, const String& pasteboardName, const PasteboardURL& pasteboardURL, const PasteboardContext* context)
{
    auto newChangeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, pasteboardName, context);
    
    ASSERT(!pasteboardURL.url.isEmpty());
    
    RetainPtr nsURL = pasteboardURL.url.createNSURL();
    RetainPtr userVisibleString = pasteboardURL.userVisibleForm.createNSString();
    RetainPtr title = pasteboardURL.title.createNSString();
    if (![title length]) {
        title = [[nsURL path] lastPathComponent];
        if (![title length])
            title = userVisibleString;
    }

    if (types.contains(WebURLsWithTitlesPboardType)) {
        PasteboardURL url = { pasteboardURL.url, String(title.get()).trim(deprecatedIsSpaceOrNewline), emptyString() };
        newChangeCount = platformStrategies()->pasteboardStrategy()->setURL(url, pasteboardName, context);
    }
    if (types.contains(String(legacyURLPasteboardTypeSingleton())))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType([nsURL absoluteString], legacyURLPasteboardTypeSingleton(), pasteboardName, context);
    if (types.contains(WebURLPboardType))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(userVisibleString.get(), WebURLPboardType, pasteboardName, context);
    if (types.contains(WebURLNamePboardType))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(title.get(), WebURLNamePboardType, pasteboardName, context);
    if (types.contains(String(legacyStringPasteboardTypeSingleton())))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(userVisibleString.get(), legacyStringPasteboardTypeSingleton(), pasteboardName, context);

    return newChangeCount;
}
    
void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    m_changeCount = writeURLForTypes(writableTypesForURL(), m_pasteboardName, pasteboardURL, context());
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL& pasteboardURL)
{
    PasteboardURL url = { pasteboardURL.url, pasteboardURL.title.trim(deprecatedIsSpaceOrNewline), emptyString() };
    m_changeCount = platformStrategies()->pasteboardStrategy()->setURL(url, m_pasteboardName, context());
}

void Pasteboard::write(const Color& color)
{
    Vector<String> types = { legacyColorPasteboardTypeSingleton() };
    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setColor(color, m_pasteboardName, context());
}

static NSFileWrapper* fileWrapper(const PasteboardImage& pasteboardImage)
{
    auto wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:Ref { *pasteboardImage.resourceData }->makeContiguous()->createNSData().get()]);
    [wrapper setPreferredFilename:RetainPtr { suggestedFilenameWithMIMEType(pasteboardImage.url.url.createNSURL().get(), pasteboardImage.resourceMIMEType) }.get()];
    return wrapper.autorelease();
}

static void writeFileWrapperAsRTFDAttachment(NSFileWrapper *wrapper, const String& pasteboardName, int64_t& newChangeCount, const PasteboardContext* context)
{
    RetainPtr string = [NSAttributedString attributedStringWithAttachment:adoptNS([[NSTextAttachment alloc] initWithFileWrapper:wrapper]).get()];

    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];
    if (!RTFDData)
        return;

    newChangeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(SharedBuffer::create(RTFDData).ptr(), legacyRTFDPasteboardTypeSingleton(), pasteboardName, context);
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    RetainPtr imageData = Ref { *pasteboardImage.image }->adapter().tiffRepresentation();
    if (!imageData)
        return;

    // FIXME: Why can we assert this? It doesn't seem like it's guaranteed.
    ASSERT(MIMETypeRegistry::isSupportedImageMIMEType(pasteboardImage.resourceMIMEType));

    auto types = writableTypesForImage();
    if (pasteboardImage.dataInWebArchiveFormat) {
        types.append(WebArchivePboardType);
        types.append(UTTypeWebArchive.identifier);
    }

    m_changeCount = writeURLForTypes(types, m_pasteboardName, pasteboardImage.url, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(SharedBuffer::create(imageData.get()).ptr(), legacyTIFFPasteboardTypeSingleton(), m_pasteboardName, context());
    if (auto archiveData = pasteboardImage.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), WebArchivePboardType, m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), UTTypeWebArchive.identifier, m_pasteboardName, context());
    }
    if (!pasteboardImage.dataInHTMLFormat.isEmpty())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(pasteboardImage.dataInHTMLFormat, legacyHTMLPasteboardTypeSingleton(), m_pasteboardName, context());
    writeFileWrapperAsRTFDAttachment(RetainPtr { fileWrapper(pasteboardImage) }.get(), m_pasteboardName, m_changeCount, context());
}

void Pasteboard::write(const PasteboardBuffer& pasteboardBuffer)
{
    ASSERT(!pasteboardBuffer.type.isEmpty());
    ASSERT(pasteboardBuffer.data);

    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes({ pasteboardBuffer.type, PasteboardCustomData::cocoaType() }, m_pasteboardName, context());

    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(pasteboardBuffer.data.get(), pasteboardBuffer.type, m_pasteboardName, context());

    PasteboardCustomData pasteboardCustomData;
    pasteboardCustomData.setOrigin(pasteboardBuffer.contentOrigin);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(pasteboardCustomData.createSharedBuffer().ptr(), PasteboardCustomData::cocoaType(), m_pasteboardName, context());
}

bool Pasteboard::canSmartReplace()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context());
    return types.contains(WebSmartPastePboardType);
}

void Pasteboard::writeMarkup(const String&)
{
}

// FIXME: This should be a general utility function for Vectors of Strings (or things that can be
// converted to Strings). It could also be faster by computing the total length and reserving that
// capacity in the StringBuilder.
static String joinPathnames(const Vector<String>& pathnames)
{
    StringBuilder builder;
    for (auto& path : pathnames) {
        if (!builder.isEmpty())
            builder.append('\n');
        builder.append(path);
    }
    return builder.toString();
}

static String readStringAtPreferredItemIndex(const String& type, std::optional<size_t> itemIndex, PasteboardStrategy& strategy, const String& pasteboardName, const PasteboardContext* context)
{
    if (itemIndex)
        return strategy.readStringFromPasteboard(*itemIndex, type, pasteboardName, context);
    return strategy.stringForType(type, pasteboardName, context);
}

static RefPtr<SharedBuffer> readBufferAtPreferredItemIndex(const String& type, std::optional<size_t> itemIndex, PasteboardStrategy& strategy, const String& pasteboardName, const PasteboardContext* context)
{
    if (itemIndex)
        return strategy.readBufferFromPasteboard(*itemIndex, type, pasteboardName, context);
    return strategy.bufferForType(type, pasteboardName, context);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy allowURL, std::optional<size_t> itemIndex)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();

    Vector<String> types;
    if (itemIndex) {
        if (auto itemInfo = strategy->informationForItemAtIndex(*itemIndex, m_pasteboardName, m_changeCount, context()))
            types = itemInfo->platformTypesByFidelity;
    } else
        strategy->getTypes(types, m_pasteboardName, context());

    if (types.contains(String(NSPasteboardTypeString))) {
        text.text = readStringAtPreferredItemIndex(NSPasteboardTypeString, itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = false;
        return;
    }

    if (types.contains(String(legacyStringPasteboardTypeSingleton()))) {
        text.text = readStringAtPreferredItemIndex(legacyStringPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = false;
        return;
    }
    
    if (types.contains(String(legacyRTFDPasteboardTypeSingleton()))) {
        if (auto data = readBufferAtPreferredItemIndex(legacyRTFDPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->makeContiguous()->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(NSPasteboardTypeRTFD))) {
        if (auto data = readBufferAtPreferredItemIndex(NSPasteboardTypeRTFD, itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(legacyRTFPasteboardTypeSingleton()))) {
        if (auto data = readBufferAtPreferredItemIndex(legacyRTFPasteboardTypeSingleton(), itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(NSPasteboardTypeRTF))) {
        if (auto data = readBufferAtPreferredItemIndex(NSPasteboardTypeRTF, itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(legacyFilesPromisePasteboardTypeSingleton()))) {
        text.text = joinPathnames(m_promisedFilePaths);
        text.isURL = false;
        return;
    }

    if (types.contains(String(legacyFilenamesPasteboardTypeSingleton()))) {
        Vector<String> pathnames;
        strategy->getPathnamesForType(pathnames, legacyFilenamesPasteboardTypeSingleton(), m_pasteboardName, context());
        text.text = joinPathnames(pathnames);
        text.isURL = false;
        return;
    }

    // FIXME: The code above looks at the types vector first, but this just gets the string without checking. Why the difference?
    if (allowURL == PlainTextURLReadingPolicy::AllowURL) {
        text.text = readStringAtPreferredItemIndex(legacyURLPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = !text.text.isNull();
    }
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t> itemIndex)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();
    auto platformTypesFromItems = [](const Vector<PasteboardItemInfo>& items) {
        HashSet<String> types;
        for (auto& item : items) {
            for (auto& type : item.platformTypesByFidelity)
                types.add(type);
        }
        return types;
    };

    HashSet<String> nonTranscodedTypes;
    Vector<String> types;
    if (itemIndex) {
        if (auto itemInfo = strategy->informationForItemAtIndex(*itemIndex, m_pasteboardName, m_changeCount, context())) {
            types = itemInfo->platformTypesByFidelity;
            nonTranscodedTypes = platformTypesFromItems({ *itemInfo });
        }
    } else {
        strategy->getTypes(types, m_pasteboardName, context());
        if (auto allItems = strategy->allPasteboardItemInfo(m_pasteboardName, m_changeCount, context()))
            nonTranscodedTypes = platformTypesFromItems(*allItems);
    }

    reader.setContentOrigin(readOrigin());

    if (types.contains(WebArchivePboardType)) {
        if (auto buffer = readBufferAtPreferredItemIndex(WebArchivePboardType, itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (types.contains(String(UTTypeWebArchive.identifier))) {
        if (auto buffer = readBufferAtPreferredItemIndex(UTTypeWebArchive.identifier, itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::AnyType && types.contains(String(legacyFilesPromisePasteboardTypeSingleton()))) {
        if (m_changeCount != changeCount() || reader.readFilePaths(m_promisedFilePaths))
            return;
    }

    if (policy == WebContentReadingPolicy::AnyType && types.contains(String(legacyFilenamesPasteboardTypeSingleton()))) {
        Vector<String> paths;
        strategy->getPathnamesForType(paths, legacyFilenamesPasteboardTypeSingleton(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || reader.readFilePaths(paths))
            return;
    }

    if (types.contains(String(legacyHTMLPasteboardTypeSingleton()))) {
        String string = readStringAtPreferredItemIndex(legacyHTMLPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readHTML(string)))
            return;
    }

    if (types.contains(String(NSPasteboardTypeHTML))) {
        String string = readStringAtPreferredItemIndex(NSPasteboardTypeHTML, itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readHTML(string)))
            return;
    }

    if (types.contains(String(legacyRTFDPasteboardTypeSingleton()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyRTFDPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypeRTFD))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypeRTFD, itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.contains(String(legacyRTFPasteboardTypeSingleton()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyRTFPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypeRTF))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypeRTF, itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
        return;

    using ImageReadingInfo = std::tuple<String, ASCIILiteral>;
    const std::array<ImageReadingInfo, 6> imageTypesToRead { {
        { String(legacyTIFFPasteboardTypeSingleton()), "image/tiff"_s },
        { String(NSPasteboardTypeTIFF), "image/tiff"_s },
        { String(legacyPDFPasteboardTypeSingleton()), "application/pdf"_s },
        { String(NSPasteboardTypePDF), "application/pdf"_s },
        { String(UTTypePNG.identifier), "image/png"_s },
        { String(UTTypeJPEG.identifier), "image/jpeg"_s }
    } };

    auto tryToReadImage = [&] (const String& pasteboardType, ASCIILiteral mimeType) {
        if (!types.contains(pasteboardType))
            return false;

        auto buffer = readBufferAtPreferredItemIndex(pasteboardType, itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return true;

        if (!buffer)
            return false;

        return reader.readImage(buffer.releaseNonNull(), mimeType);
    };

    Vector<ImageReadingInfo, 6> transcodedImageTypesToRead;
    for (auto& [pasteboardType, mimeType] : imageTypesToRead) {
        if (!nonTranscodedTypes.contains(pasteboardType)) {
            transcodedImageTypesToRead.append({ pasteboardType, mimeType });
            continue;
        }
        if (tryToReadImage(pasteboardType, mimeType))
            return;
    }

    for (auto& [pasteboardType, mimeType] : transcodedImageTypesToRead) {
        if (tryToReadImage(pasteboardType, mimeType))
            return;
    }

    if (types.contains(String(legacyURLPasteboardTypeSingleton()))) {
        URL url = strategy->url(m_pasteboardName, context());
        String title = readStringAtPreferredItemIndex(WebURLNamePboardType, itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!url.isNull() && reader.readURL(url, title)))
            return;
    }

    if (types.contains(String(legacyStringPasteboardTypeSingleton()))) {
        String string = readStringAtPreferredItemIndex(legacyStringPasteboardTypeSingleton(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readPlainText(string)))
            return;
    }

    if (types.contains(String(UTTypeUTF8PlainText.identifier))) {
        String string = strategy->stringForType(UTTypeUTF8PlainText.identifier, m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readPlainText(string)))
            return;
    }
}

bool Pasteboard::hasData()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context());
    return !types.isEmpty();
}

static String cocoaTypeFromHTMLClipboardType(const String& type)
{
    if (RetainPtr platformType = PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type).createNSString().get()) {
        if (platformType.get().length)
            return platformType.get();
    }

    // Reject types that might contain subframe information.
    if (type == "text/rtf"_s || type == "public.rtf"_s || type == "com.apple.traditional-mac-plain-text"_s)
        return String();

    auto utiType = UTIFromMIMEType(type);
    if (!utiType.isEmpty()) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (auto pbType = adoptCF(UTTypeCopyPreferredTagWithClass(utiType.createCFString().get(), kUTTagClassNSPboardType)))
            return pbType.get();
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    // No mapping, just pass the whole string though
    return type;
}

void Pasteboard::clear(const String& type)
{
    String cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (cocoaType.isEmpty())
        return;
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(emptyString(), cocoaType, m_pasteboardName, context());
}

Vector<String> Pasteboard::readPlatformValuesAsStrings(const String& domType, int64_t changeCount, const String& pasteboardName)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();
    auto cocoaType = cocoaTypeFromHTMLClipboardType(domType);
    if (cocoaType.isEmpty())
        return { };

    auto values = strategy->allStringsForType(cocoaType, pasteboardName, context());
    if (cocoaType == String(legacyStringPasteboardTypeSingleton())) {
        values = values.map([&] (auto& value) -> String {
            return [value.createNSString() precomposedStringWithCanonicalMapping];
        });
    }

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (changeCount != platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName, context()))
        return { };

    return values;
}

static String utiTypeFromCocoaType(const String& type)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (RetainPtr<CFStringRef> utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, type.createCFString().get(), 0))) {
        if (RetainPtr<CFStringRef> mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType)))
            return String(mimeType.get());
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    return String();
}

void Pasteboard::addHTMLClipboardTypesForCocoaType(ListHashSet<String>& resultTypes, const String& cocoaType)
{
    if (cocoaType == "NeXT plain ascii pasteboard type"_s)
        return; // Skip this ancient type that gets auto-supplied by some system conversion.

    // UTI may not do these right, so make sure we get the right, predictable result
    if (cocoaType == String(legacyStringPasteboardTypeSingleton()) || cocoaType == String(NSPasteboardTypeString)) {
        resultTypes.add(textPlainContentTypeAtom());
        return;
    }
    if (cocoaType == String(legacyURLPasteboardTypeSingleton())) {
        resultTypes.add("text/uri-list"_s);
        return;
    }
    if (cocoaType == String(legacyFilenamesPasteboardTypeSingleton()) || Pasteboard::shouldTreatCocoaTypeAsFile(cocoaType))
        return;
    String utiType = utiTypeFromCocoaType(cocoaType);
    if (!utiType.isEmpty()) {
        resultTypes.add(utiType);
        return;
    }
    // No mapping, just pass the whole string through.
    resultTypes.add(cocoaType);
}

void Pasteboard::writeString(const String& type, const String& data)
{
    const String& cocoaType = cocoaTypeFromHTMLClipboardType(type);
    String cocoaData = data;

    if (cocoaType == String(legacyURLPasteboardTypeSingleton()) || cocoaType == String(UTTypeFileURL.identifier)) {
        RetainPtr url = adoptNS([[NSURL alloc] initWithString:cocoaData.createNSString().get()]);
        if ([url isFileURL])
            return;
        platformStrategies()->pasteboardStrategy()->setTypes({ cocoaType }, m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName, context());

        return;
    }

    if (!cocoaType.isEmpty()) {
        // everything else we know of goes on the pboard as a string
        platformStrategies()->pasteboardStrategy()->addTypes({ cocoaType }, m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName, context());
    }
}

Vector<String> Pasteboard::readFilePaths()
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();

    Vector<String> types;
    strategy->getTypes(types, m_pasteboardName, context());

    if (types.contains(String(legacyFilesPromisePasteboardTypeSingleton())))
        return m_promisedFilePaths;

    if (types.contains(String(legacyFilenamesPasteboardTypeSingleton()))) {
        Vector<String> filePaths;
        strategy->getPathnamesForType(filePaths, legacyFilenamesPasteboardTypeSingleton(), m_pasteboardName, context());
        return filePaths;
    }

    return { };
}

#if ENABLE(DRAG_SUPPORT)
static void flipImageSpec(CoreDragImageSpec* imageSpec)
{
    auto tempRow = MallocSpan<uint8_t>::malloc(imageSpec->bytesPerRow);
    int planes = imageSpec->isPlanar ? imageSpec->samplesPerPixel : 1;
    for (auto* plane : std::span { imageSpec->data }.first(planes)) {
        auto planeSpan = unsafeMakeSpan(const_cast<uint8_t*>(plane), imageSpec->bytesPerRow * imageSpec->pixelsHigh);
        for (int i = 0; i < imageSpec->pixelsHigh / 2; ++i) {
            auto topRow = planeSpan.first(imageSpec->bytesPerRow);
            auto bottomRow = planeSpan.last(imageSpec->bytesPerRow);
            memmoveSpan(tempRow.mutableSpan(), topRow);
            memmoveSpan(topRow, bottomRow);
            memmoveSpan(bottomRow, tempRow.span());
            planeSpan = planeSpan.subspan(imageSpec->bytesPerRow, planeSpan.size() - 2 * imageSpec->bytesPerRow);
        }
    }
}

static void setDragImageImpl(NSImage *image, NSPoint offset)
{
    bool flipImage;
    NSSize imageSize = image.size;
    CGRect imageRect = CGRectMake(0, 0, imageSize.width, imageSize.height);
    NSRect convertedRect = NSRectFromCGRect(imageRect);
    NSImageRep *imageRep = [image bestRepresentationForRect:convertedRect context:nil hints:nil];
    RetainPtr<NSBitmapImageRep> bitmapImage;
    if (!imageRep || ![imageRep isKindOfClass:[NSBitmapImageRep class]] || !NSEqualSizes(imageRep.size, imageSize)) {
        [image lockFocus];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        bitmapImage = adoptNS([[NSBitmapImageRep alloc] initWithFocusedViewRect:convertedRect]);
ALLOW_DEPRECATED_DECLARATIONS_END
        [image unlockFocus];

        // We may have to flip the bits we just read if the image was flipped since it means the cache was also
        // and CoreDragSetImage can't take a transform for rendering.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        flipImage = image.isFlipped;
ALLOW_DEPRECATED_DECLARATIONS_END
    } else {
        flipImage = false;
        bitmapImage = checked_objc_cast<NSBitmapImageRep>(imageRep);
    }
    ASSERT(bitmapImage);

    CoreDragImageSpec imageSpec;
    imageSpec.version = kCoreDragImageSpecVersionOne;
    imageSpec.pixelsWide = [bitmapImage pixelsWide];
    imageSpec.pixelsHigh = [bitmapImage pixelsHigh];
    imageSpec.bitsPerSample = [bitmapImage bitsPerSample];
    imageSpec.samplesPerPixel = [bitmapImage samplesPerPixel];
    imageSpec.bitsPerPixel = [bitmapImage bitsPerPixel];
    imageSpec.bytesPerRow = [bitmapImage bytesPerRow];
    imageSpec.isPlanar = [bitmapImage isPlanar];
    imageSpec.hasAlpha = [bitmapImage hasAlpha];
    [bitmapImage getBitmapDataPlanes:const_cast<unsigned char**>(imageSpec.data)];

    // if image was flipped, we have an upside down bitmap since the cache is rendered flipped
    if (flipImage)
        flipImageSpec(&imageSpec);

    CGSRegionObj imageShape;
    OSStatus error = CGSNewRegionWithRect(&imageRect, &imageShape);
    ASSERT(error == kCGErrorSuccess);
    if (error != kCGErrorSuccess)
        return;

    // make sure image has integer offset
    CGPoint imageOffset = { -offset.x, -(imageSize.height - offset.y) };
    imageOffset.x = floor(imageOffset.x + 0.5);
    imageOffset.y = floor(imageOffset.y + 0.5);

    error = CoreDragSetImage(CoreDragGetCurrentDrag(), imageOffset, &imageSpec, imageShape, 1.0);
    CGSReleaseRegion(imageShape);
    ASSERT(error == kCGErrorSuccess);
}

void Pasteboard::setDragImage(DragImage image, const IntPoint& location)
{
    // Don't allow setting the drag image if someone kept a pasteboard and is trying to set the image too late.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
        return;

    // Dashboard wants to be able to set the drag image during dragging, but Cocoa does not allow this.
    // Instead we must drop down to the CoreGraphics API.

    // FIXME: Do we still need this now Dashboard is gone?
    setDragImageImpl(image.get().get(), location);

    // Hack: We must post an event to wake up the NSDragManager, which is sitting in a nextEvent call
    // up the stack from us because the CoreFoundation drag manager does not use the run loop by itself.
    // This is the most innocuous event to use, per Kristin Forster.
    // This is only relevant in WK1. Do not execute in the WebContent process, since it is now using
    // NSRunLoop, and not the NSApplication run loop.
    // This is a safer cpp false positive (rdar://161068288).
    SUPPRESS_UNRETAINED_ARG if ([NSApp isRunning]) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
        NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
        // This is a safer cpp false positive (rdar://161068288).
        SUPPRESS_UNRETAINED_ARG [NSApp postEvent:event atStart:YES];
    }
}
#endif

bool Pasteboard::canWriteTrustworthyWebURLsPboardType()
{
    return true;
}

RefPtr<WebCore::SharedBuffer> Pasteboard::bufferConvertedToPasteboardType(const PasteboardBuffer& pasteboardBuffer, const String& pasteboardType)
{
    if (pasteboardBuffer.type == pasteboardType)
        return pasteboardBuffer.data;

    if (pasteboardType != String(legacyTIFFPasteboardTypeSingleton()))
        return pasteboardBuffer.data;

    if (pasteboardBuffer.type == String(UTTypeTIFF.identifier))
        return pasteboardBuffer.data;

    auto sourceData = Ref { *pasteboardBuffer.data }->createCFData();
    auto sourceType = pasteboardBuffer.type.createCFString();

    const void* key = kCGImageSourceTypeIdentifierHint;
    const void* value = sourceType.get();
    auto options = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    auto source = adoptCF(CGImageSourceCreateWithData(sourceData.get(), options.get()));
    if (!source)
        return nullptr;

    auto data = adoptCF(CFDataCreateMutable(0, 0));
    auto destination = adoptCF(CGImageDestinationCreateWithData(data.get(), bridge_cast(UTTypeTIFF.identifier), 1, NULL));
    if (!destination)
        return nullptr;

    CGImageDestinationAddImageFromSource(destination.get(), source.get(), 0, NULL);
    if (!CGImageDestinationFinalize(destination.get()))
        return nullptr;

    return SharedBuffer::create(data.get());
}

}

#endif // PLATFORM(MAC)
