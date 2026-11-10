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

const ASCIILiteral WebSmartPastePboardType = "NeXT smart paste pasteboard type"_s;
const ASCIILiteral WebURLPboardType = "public.url"_s;

static const Vector<RefPtr<StringImpl>> writableTypesForURL()
{
    Vector<RefPtr<StringImpl>> types;
    
    types.append(WebURLsWithTitlesPboardType);
    types.append(StringImpl::create(legacyURLPasteboardTypeSingleton()));
    types.append(WebURLPboardType);
    types.append(WebURLNamePboardType);
    types.append(StringImpl::create(legacyStringPasteboardTypeSingleton()));
    return types;
}

static Vector<RefPtr<StringImpl>> writableTypesForImage()
{
    Vector<RefPtr<StringImpl>> types;
    types.append(StringImpl::create(legacyTIFFPasteboardTypeSingleton()));
    types.append(StringImpl::create(legacyRTFDPasteboardTypeSingleton()));
    return types;
}

NSArray *Pasteboard::supportedFileUploadPasteboardTypes()
{
    return @[ legacyFilesPromisePasteboardTypeSingleton(), legacyFilenamesPasteboardTypeSingleton() ];
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context)
    : m_context(WTF::move(context))
    , m_pasteboardName(emptyString())
    , m_changeCount(0)
{
}

Pasteboard::Pasteboard(std::unique_ptr<PasteboardContext>&& context, StringImpl* pasteboardName, const Vector<RefPtr<StringImpl>>& promisedFilePaths, const Vector<RefPtr<StringImpl>>& promisedFileMIMETypes)
    : m_context(WTF::move(context))
    , m_pasteboardName(pasteboardName)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, m_context.get()))
    , m_promisedFilePaths(promisedFilePaths)
    , m_promisedFileMIMETypes(promisedFileMIMETypes)
{
    ASSERT(pasteboardName);
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context), StringImpl::create(NSPasteboardNameGeneral).get());
}

#if ENABLE(DRAG_SUPPORT)
RefPtr<StringImpl> Pasteboard::nameOfDragPasteboard()
{
    return StringImpl::create(NSPasteboardNameDrag);
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(std::unique_ptr<PasteboardContext>&& context)
{
    return makeUnique<Pasteboard>(WTF::move(context), StringImpl::create(NSPasteboardNameDrag).get());
}

std::unique_ptr<Pasteboard> Pasteboard::create(const DragData& dragData)
{
    return makeUnique<Pasteboard>(dragData.createPasteboardContext(), dragData.pasteboardName(), dragData.fileNames(), dragData.promisedFileMIMETypes());
}
#endif

void Pasteboard::clear()
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(Vector<RefPtr<StringImpl>>(), m_pasteboardName, context());
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    Vector<RefPtr<StringImpl>> types;
    Vector<RefPtr<StringImpl>> clientTypes;
    Vector<RefPtr<WebCore::SharedBuffer>> clientData;
    for (size_t it = 0; it < content.clientTypesAndData.size(); ++it) {
        clientTypes.append(content.clientTypesAndData[it].first);
        clientData.append(content.clientTypesAndData[it].second);
    }

    if (content.canSmartCopyOrDelete)
        types.append(WebSmartPastePboardType);
    if (content.dataInWebArchiveFormat) {
        types.append(StringImpl::create(WebArchivePboardType));
        types.append(StringImpl::create(UTTypeWebArchive.identifier));
    }
    if (content.dataInRTFDFormat)
        types.append(StringImpl::create(legacyRTFDPasteboardTypeSingleton()));
    if (content.dataInRTFFormat)
        types.append(StringImpl::create(legacyRTFPasteboardTypeSingleton()));
    if (content.dataInHTMLFormat)
        types.append(StringImpl::create(legacyHTMLPasteboardTypeSingleton()));
    if (content.dataInStringFormat)
        types.append(StringImpl::create(legacyStringPasteboardTypeSingleton()));
    types.appendVector(clientTypes);
    types.append(PasteboardCustomData::cocoaType());

    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());

    // FIXME: The following code should be refactored, such that it only requires a single call out to the client layer.
    // In WebKit2, this currently results in many unnecessary synchronous round-trip IPC messages.

    ASSERT(clientTypes.size() == clientData.size());
    for (size_t i = 0, size = clientTypes.size(); i < size; ++i)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(clientData[i].get(), clientTypes[i], m_pasteboardName, context());
    if (content.canSmartCopyOrDelete)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(nullptr, StringImpl::create(WebSmartPastePboardType).ptr(), m_pasteboardName, context());

    bool didWriteWebArchive = false;
    if (RefPtr webArchive = content.webArchive) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->writeWebArchive(*webArchive, m_pasteboardName);
        didWriteWebArchive = !!m_changeCount;
    }
    if (!didWriteWebArchive && content.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), StringImpl::create(WebArchivePboardType).ptr(), m_pasteboardName, context());

        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), StringImpl::create(UTTypeWebArchive.identifier).get(), m_pasteboardName, context());
    }

    if (content.dataInRTFDFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFDFormat.get(), StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    if (content.dataInRTFFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFFormat.get(), StringImpl::create(legacyRTFPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    if (content.dataInHTMLFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInHTMLFormat, StringImpl::create(legacyHTMLPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    if (content.dataInStringFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInStringFormat, StringImpl::create(legacyStringPasteboardTypeSingleton()).get(), m_pasteboardName, context());

    PasteboardCustomData data;
    data.setOrigin(content.contentOrigin);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(data.createSharedBuffer().ptr(), StringImpl::create(PasteboardCustomData::cocoaType()).ptr(), m_pasteboardName, context());

}

void Pasteboard::writePlainText(StringImpl* text, SmartReplaceOption smartReplaceOption)
{
    Vector<RefPtr<StringImpl>> types;
    types.append(StringImpl::create(legacyStringPasteboardTypeSingleton()));
    if (smartReplaceOption == CanSmartReplace)
        types.append(WebSmartPastePboardType);

    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(text, StringImpl::create(legacyStringPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    if (smartReplaceOption == CanSmartReplace)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(0, StringImpl::create(WebSmartPastePboardType).ptr(), m_pasteboardName, context());
}

static long writeURLForTypes(const Vector<RefPtr<StringImpl>>& types, StringImpl* pasteboardName, const PasteboardURL& pasteboardURL, const PasteboardContext* context)
{
    auto newChangeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, pasteboardName, context);
    
    ASSERT(!pasteboardURL.url.isEmpty());
    
    RetainPtr nsURL = pasteboardURL.url.createNSURL();
    RetainPtr userVisibleString = createNSString(pasteboardURL.userVisibleForm);
    RetainPtr title = createNSString(pasteboardURL.title);
    if (![title length]) {
        title = [[nsURL path] lastPathComponent];
        // Very short titles are not useful, and we commonly get a "/" as the last path component.
        // Fall back to the full URL in this case.
        if ([title length] <= 1)
            title = userVisibleString;
    }

    if (types.containsIf([&](auto& item) { return equal(item, WebURLsWithTitlesPboardType); })) {
        PasteboardURL url = { pasteboardURL.url, trim(StringImpl::create(title.get()), deprecatedIsSpaceOrNewline), emptyString() };
        newChangeCount = platformStrategies()->pasteboardStrategy()->setURL(url, pasteboardName, context);
    }
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyURLPasteboardTypeSingleton()).get()); }))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(StringImpl::create([nsURL absoluteString]).get(), StringImpl::create(legacyURLPasteboardTypeSingleton()).get(), pasteboardName, context);
    if (types.containsIf([&](auto& item) { return equal(item, WebURLPboardType); }))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(StringImpl::create(userVisibleString.get()).get(), StringImpl::create(WebURLPboardType).ptr(), pasteboardName, context);
    if (types.containsIf([&](auto& item) { return equal(item, WebURLNamePboardType); }))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(StringImpl::create(title.get()).get(), StringImpl::create(WebURLNamePboardType).ptr(), pasteboardName, context);
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyStringPasteboardTypeSingleton()).get()); }))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(StringImpl::create(userVisibleString.get()).get(), StringImpl::create(legacyStringPasteboardTypeSingleton()).get(), pasteboardName, context);

    return newChangeCount;
}
    
void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    m_changeCount = writeURLForTypes(writableTypesForURL(), m_pasteboardName, pasteboardURL, context());
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL& pasteboardURL)
{
    PasteboardURL url = { pasteboardURL.url, trim(pasteboardURL.title, deprecatedIsSpaceOrNewline), emptyString() };
    m_changeCount = platformStrategies()->pasteboardStrategy()->setURL(url, m_pasteboardName, context());
}

void Pasteboard::write(const Color& color)
{
    Vector<RefPtr<StringImpl>> types = { StringImpl::create(legacyColorPasteboardTypeSingleton()) };
    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setColor(color, m_pasteboardName, context());
}

static NSFileWrapper* fileWrapper(const PasteboardImage& pasteboardImage)
{
    auto wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:Ref { *pasteboardImage.resourceData }->makeContiguous()->createNSData().get()]);
    [wrapper setPreferredFilename:RetainPtr { suggestedFilenameWithMIMEType(pasteboardImage.url.url.createNSURL().get(), pasteboardImage.resourceMIMEType) }.get()];
    return wrapper.autorelease();
}

static void writeFileWrapperAsRTFDAttachment(NSFileWrapper *wrapper, StringImpl* pasteboardName, int64_t& newChangeCount, const PasteboardContext* context)
{
    RetainPtr string = [NSAttributedString attributedStringWithAttachment:adoptNS([[NSTextAttachment alloc] initWithFileWrapper:wrapper]).get()];

    RetainPtr<NSData> RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];
    if (!RTFDData)
        return;

    newChangeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(SharedBuffer::create(RTFDData.get()).ptr(), StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get(), pasteboardName, context);
}

void Pasteboard::write(PasteboardImage&& pasteboardImage)
{
    RefPtr tiffData = WTF::move(pasteboardImage.dataInTIFFFormat);
    if (!tiffData)
        return;

    // FIXME: Why can we assert this? It doesn't seem like it's guaranteed.
    ASSERT(MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(pasteboardImage.resourceMIMEType));

    auto types = writableTypesForImage();
    if (pasteboardImage.dataInWebArchiveFormat) {
        types.append(StringImpl::create(WebArchivePboardType));
        types.append(StringImpl::create(UTTypeWebArchive.identifier));
    }

    m_changeCount = writeURLForTypes(types, m_pasteboardName, pasteboardImage.url, context());
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(tiffData.get(), StringImpl::create(legacyTIFFPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    if (auto archiveData = pasteboardImage.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), StringImpl::create(WebArchivePboardType).ptr(), m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), StringImpl::create(UTTypeWebArchive.identifier).get(), m_pasteboardName, context());
    }
    if (!isEmpty(pasteboardImage.dataInHTMLFormat))
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(pasteboardImage.dataInHTMLFormat, StringImpl::create(legacyHTMLPasteboardTypeSingleton()).get(), m_pasteboardName, context());
    writeFileWrapperAsRTFDAttachment(RetainPtr { fileWrapper(pasteboardImage) }.get(), m_pasteboardName, m_changeCount, context());
}

void Pasteboard::write(const PasteboardBuffer& pasteboardBuffer)
{
    ASSERT(!isEmpty(pasteboardBuffer.type));
    ASSERT(pasteboardBuffer.data);

    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes({ pasteboardBuffer.type, PasteboardCustomData::cocoaType() }, m_pasteboardName, context());

    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(pasteboardBuffer.data.get(), pasteboardBuffer.type, m_pasteboardName, context());

    PasteboardCustomData pasteboardCustomData;
    pasteboardCustomData.setOrigin(pasteboardBuffer.contentOrigin);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(pasteboardCustomData.createSharedBuffer().ptr(), StringImpl::create(PasteboardCustomData::cocoaType()).ptr(), m_pasteboardName, context());
}

bool Pasteboard::canSmartReplace()
{
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context());
    return types.containsIf([&](auto& item) {
        return equal(item, WebSmartPastePboardType);
    });
}

void Pasteboard::writeMarkup(StringImpl*)
{
}

// FIXME: This should be a general utility function for Vectors of Strings (or things that can be
// converted to Strings). It could also be faster by computing the total length and reserving that
// capacity in the StringBuilder.
static RefPtr<StringImpl> joinPathnames(const Vector<RefPtr<StringImpl>>& pathnames)
{
    StringBuilder builder;
    for (auto& path : pathnames) {
        if (!builder.isEmpty())
            builder.append('\n');
        builder.append(path);
    }
    return builder.toString();
}

static RefPtr<StringImpl> readStringAtPreferredItemIndex(StringImpl* type, std::optional<size_t> itemIndex, PasteboardStrategy& strategy, StringImpl* pasteboardName, const PasteboardContext* context)
{
    if (itemIndex)
        return strategy.readStringFromPasteboard(*itemIndex, type, pasteboardName, context);
    return strategy.stringForType(type, pasteboardName, context);
}

static RefPtr<SharedBuffer> readBufferAtPreferredItemIndex(StringImpl* type, std::optional<size_t> itemIndex, PasteboardStrategy& strategy, StringImpl* pasteboardName, const PasteboardContext* context)
{
    if (itemIndex)
        return strategy.readBufferFromPasteboard(*itemIndex, type, pasteboardName, context);
    return strategy.bufferForType(type, pasteboardName, context);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy allowURL, std::optional<size_t> itemIndex)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();

    Vector<RefPtr<StringImpl>> types;
    if (itemIndex) {
        if (auto itemInfo = strategy->informationForItemAtIndex(*itemIndex, m_pasteboardName, m_changeCount, context()))
            types = itemInfo->platformTypesByFidelity;
    } else
        strategy->getTypes(types, m_pasteboardName, context());

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeString).get()); })) {
        text.text = readStringAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeString).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = false;
        return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyStringPasteboardTypeSingleton()).get()); })) {
        text.text = readStringAtPreferredItemIndex(StringImpl::create(legacyStringPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = false;
        return;
    }
    
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get()); })) {
        if (auto data = readBufferAtPreferredItemIndex(StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->makeContiguous()->createNSData().get() documentAttributes:nil])) {
                text.text = StringImpl::create([attributedString string]);
                text.isURL = false;
                return;
            }
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeRTFD).get()); })) {
        if (auto data = readBufferAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeRTFD).get(), itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->createNSData().get() documentAttributes:nil])) {
                text.text = StringImpl::create([attributedString string]);
                text.isURL = false;
                return;
            }
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyRTFPasteboardTypeSingleton()).get()); })) {
        if (auto data = readBufferAtPreferredItemIndex(StringImpl::create(legacyRTFPasteboardTypeSingleton()).get(), itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = StringImpl::create([attributedString string]);
                text.isURL = false;
                return;
            }
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeRTF).get()); })) {
        if (auto data = readBufferAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeRTF).get(), itemIndex, strategy, m_pasteboardName, context())) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = StringImpl::create([attributedString string]);
                text.isURL = false;
                return;
            }
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); })) {
        text.text = joinPathnames(m_promisedFilePaths);
        text.isURL = false;
        return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); })) {
        Vector<RefPtr<StringImpl>> pathnames;
        strategy->getPathnamesForType(pathnames, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get(), m_pasteboardName, context());
        text.text = joinPathnames(pathnames);
        text.isURL = false;
        return;
    }

    // FIXME: The code above looks at the types vector first, but this just gets the string without checking. Why the difference?
    if (allowURL == PlainTextURLReadingPolicy::AllowURL) {
        text.text = readStringAtPreferredItemIndex(StringImpl::create(legacyURLPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        text.isURL = !!text.text;
    }
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, std::optional<size_t> itemIndex)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();
    auto platformTypesFromItems = [](const Vector<PasteboardItemInfo>& items) {
        HashSet<RefPtr<StringImpl>> types;
        for (auto& item : items) {
            for (auto& type : item.platformTypesByFidelity)
                types.add(type);
        }
        return types;
    };

    HashSet<RefPtr<StringImpl>> nonTranscodedTypes;
    Vector<RefPtr<StringImpl>> types;
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

    if (types.containsIf([&](auto& item) { return equal(item, WebArchivePboardType); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(WebArchivePboardType).ptr(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(UTTypeWebArchive.identifier).get()); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(UTTypeWebArchive.identifier).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::AnyType && types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); })) {
        if (m_changeCount != changeCount() || reader.readFilePaths(m_promisedFilePaths))
            return;
    }

    if (policy == WebContentReadingPolicy::AnyType && types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); })) {
        Vector<RefPtr<StringImpl>> paths;
        strategy->getPathnamesForType(paths, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || reader.readFilePaths(paths))
            return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyHTMLPasteboardTypeSingleton()).get()); })) {
        RefPtr<StringImpl> string = readStringAtPreferredItemIndex(StringImpl::create(legacyHTMLPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!!string && reader.readHTML(string)))
            return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeHTML).get()); })) {
        RefPtr<StringImpl> string = readStringAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeHTML).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!!string && reader.readHTML(string)))
            return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get()); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(legacyRTFDPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeRTFD).get()); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeRTFD).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyRTFPasteboardTypeSingleton()).get()); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(legacyRTFPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(NSPasteboardTypeRTF).get()); })) {
        if (auto buffer = readBufferAtPreferredItemIndex(StringImpl::create(NSPasteboardTypeRTF).get(), itemIndex, strategy.get(), m_pasteboardName, context())) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
        return;

    using ImageReadingInfo = std::tuple<RefPtr<StringImpl>, ASCIILiteral>;
    const std::array<ImageReadingInfo, 6> imageTypesToRead { {
        { StringImpl::create(legacyTIFFPasteboardTypeSingleton()), "image/tiff"_s },
        { StringImpl::create(NSPasteboardTypeTIFF), "image/tiff"_s },
        { StringImpl::create(legacyPDFPasteboardTypeSingleton()), "application/pdf"_s },
        { StringImpl::create(NSPasteboardTypePDF), "application/pdf"_s },
        { StringImpl::create(UTTypePNG.identifier), "image/png"_s },
        { StringImpl::create(UTTypeJPEG.identifier), "image/jpeg"_s }
    } };

    auto tryToReadImage = [&] (StringImpl* pasteboardType, ASCIILiteral mimeType) {
        if (!types.containsIf([&](auto& item) { return equal(item, pasteboardType); }))
            return false;

        auto buffer = readBufferAtPreferredItemIndex(pasteboardType, itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount())
            return true;

        if (!buffer)
            return false;

        return reader.readImage(buffer.releaseNonNull(), StringImpl::create(mimeType).ptr());
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

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyURLPasteboardTypeSingleton()).get()); })) {
        URL url = strategy->url(m_pasteboardName, context());
        RefPtr<StringImpl> title = readStringAtPreferredItemIndex(StringImpl::create(WebURLNamePboardType).ptr(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!url.isNull() && reader.readURL(url, title)))
            return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyStringPasteboardTypeSingleton()).get()); })) {
        RefPtr<StringImpl> string = readStringAtPreferredItemIndex(StringImpl::create(legacyStringPasteboardTypeSingleton()).get(), itemIndex, strategy.get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!!string && reader.readPlainText(string)))
            return;
    }

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(UTTypeUTF8PlainText.identifier).get()); })) {
        RefPtr<StringImpl> string = strategy->stringForType(StringImpl::create(UTTypeUTF8PlainText.identifier).get(), m_pasteboardName, context());
        if (m_changeCount != changeCount() || (!!string && reader.readPlainText(string)))
            return;
    }
}

bool Pasteboard::hasData()
{
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context());
    return !types.isEmpty();
}

static RefPtr<StringImpl> cocoaTypeFromHTMLClipboardType(StringImpl* type)
{
    if (RetainPtr platformType = createNSString(PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type)).get()) {
        if (platformType.get().length)
            return StringImpl::create(platformType.get());
    }

    // Reject types that might contain subframe information.
    if (equal(type, "text/rtf"_s) || equal(type, "public.rtf"_s) || equal(type, "com.apple.traditional-mac-plain-text"_s))
        return nullptr;

    auto utiType = UTIFromMIMEType(type);
    if (!isEmpty(utiType)) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (auto pbType = adoptCF(UTTypeCopyPreferredTagWithClass(createCFString(utiType).get(), kUTTagClassNSPboardType)))
            return StringImpl::create(pbType.get());
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    // No mapping, just pass the whole string though
    return type;
}

void Pasteboard::clear(StringImpl* type)
{
    RefPtr<StringImpl> cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (isEmpty(cocoaType))
        return;
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(emptyString(), cocoaType, m_pasteboardName, context());
}

Vector<RefPtr<StringImpl>> Pasteboard::readPlatformValuesAsStrings(StringImpl* domType, int64_t changeCount, StringImpl* pasteboardName)
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();
    auto cocoaType = cocoaTypeFromHTMLClipboardType(domType);
    if (isEmpty(cocoaType))
        return { };

    auto values = strategy->allStringsForType(cocoaType, pasteboardName, context());
    if (equal(cocoaType, StringImpl::create(legacyStringPasteboardTypeSingleton()).get())) {
        values = values.map([&] (auto& value) -> RefPtr<StringImpl> {
            return StringImpl::create([createNSString(value) precomposedStringWithCanonicalMapping]);
        });
    }

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (changeCount != platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName, context()))
        return { };

    return values;
}

static RefPtr<StringImpl> utiTypeFromCocoaType(StringImpl* type)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (RetainPtr<CFStringRef> utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, createCFString(type).get(), 0))) {
        if (RetainPtr<CFStringRef> mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType)))
            return StringImpl::create(mimeType.get());
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    return nullptr;
}

void Pasteboard::addHTMLClipboardTypesForCocoaType(OrderedHashSet<RefPtr<StringImpl>>& resultTypes, StringImpl* cocoaType)
{
    if (equal(cocoaType, "NeXT plain ascii pasteboard type"_s))
        return; // Skip this ancient type that gets auto-supplied by some system conversion.

    // UTI may not do these right, so make sure we get the right, predictable result
    if (equal(cocoaType, StringImpl::create(legacyStringPasteboardTypeSingleton()).get()) || equal(cocoaType, StringImpl::create(NSPasteboardTypeString).get())) {
        resultTypes.add(textPlainContentTypeAtom());
        return;
    }
    if (equal(cocoaType, StringImpl::create(legacyURLPasteboardTypeSingleton()).get())) {
        resultTypes.add("text/uri-list"_si);
        return;
    }
    if (equal(cocoaType, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()) || Pasteboard::shouldTreatCocoaTypeAsFile(cocoaType))
        return;
    RefPtr<StringImpl> utiType = utiTypeFromCocoaType(cocoaType);
    if (!isEmpty(utiType)) {
        resultTypes.add(utiType);
        return;
    }
    // No mapping, just pass the whole string through.
    resultTypes.add(cocoaType);
}

void Pasteboard::writeString(StringImpl* type, StringImpl* data)
{
    const RefPtr<StringImpl>& cocoaType = cocoaTypeFromHTMLClipboardType(type);
    RefPtr<StringImpl> cocoaData = data;

    if (equal(cocoaType, StringImpl::create(legacyURLPasteboardTypeSingleton()).get()) || equal(cocoaType, StringImpl::create(UTTypeFileURL.identifier).get())) {
        RetainPtr url = adoptNS([[NSURL alloc] initWithString:createNSString(cocoaData).get()]);
        if ([url isFileURL])
            return;
        platformStrategies()->pasteboardStrategy()->setTypes({ cocoaType }, m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName, context());

        return;
    }

    if (!isEmpty(cocoaType)) {
        // everything else we know of goes on the pboard as a string
        platformStrategies()->pasteboardStrategy()->addTypes({ cocoaType }, m_pasteboardName, context());
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName, context());
    }
}

Vector<RefPtr<StringImpl>> Pasteboard::readFilePaths()
{
    CheckedRef strategy = *platformStrategies()->pasteboardStrategy();

    Vector<RefPtr<StringImpl>> types;
    strategy->getTypes(types, m_pasteboardName, context());

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); }))
        return m_promisedFilePaths;

    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); })) {
        Vector<RefPtr<StringImpl>> filePaths;
        strategy->getPathnamesForType(filePaths, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get(), m_pasteboardName, context());
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
    RetainPtr<NSImageRep> imageRep = [image bestRepresentationForRect:convertedRect context:nil hints:nil];
    RetainPtr<NSBitmapImageRep> bitmapImage;
    if (!imageRep || ![imageRep isKindOfClass:[NSBitmapImageRep class]] || !NSEqualSizes(imageRep.get().size, imageSize)) {
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
        bitmapImage = checked_objc_cast<NSBitmapImageRep>(imageRep.get());
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

    // This is an out-parameter which is explicitly released below.
    SUPPRESS_UNRETAINED_LOCAL CGSRegionObj imageShape;
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

void Pasteboard::setDragImage(DragImage&& image, const IntPoint& location)
{
    // Don't allow setting the drag image if someone kept a pasteboard and is trying to set the image too late.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
        return;

    // Dashboard wants to be able to set the drag image during dragging, but Cocoa does not allow this.
    // Instead we must drop down to the CoreGraphics API.

    // FIXME: Do we still need this now Dashboard is gone?
    setDragImageImpl(image.platformImage().get(), location);

    // Hack: We must post an event to wake up the NSDragManager, which is sitting in a nextEvent call
    // up the stack from us because the CoreFoundation drag manager does not use the run loop by itself.
    // This is the most innocuous event to use, per Kristin Forster.
    // This is only relevant in WK1. Do not execute in the WebContent process, since it is now using
    // NSRunLoop, and not the NSApplication run loop.
    if ([NSApp isRunning]) {
        ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
        NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
        [NSApp postEvent:event atStart:YES];
    }
}
#endif

bool Pasteboard::canWriteTrustworthyWebURLsPboardType()
{
    return true;
}

RefPtr<WebCore::SharedBuffer> Pasteboard::bufferConvertedToPasteboardType(const PasteboardBuffer& pasteboardBuffer, StringImpl* pasteboardType)
{
    if (equal(pasteboardBuffer.type, pasteboardType))
        return pasteboardBuffer.data;

    if (!equal(pasteboardType, StringImpl::create(legacyTIFFPasteboardTypeSingleton()).get()))
        return pasteboardBuffer.data;

    if (equal(pasteboardBuffer.type, StringImpl::create(UTTypeTIFF.identifier).get()))
        return pasteboardBuffer.data;

    auto sourceData = Ref { *pasteboardBuffer.data }->createCFData();
    auto sourceType = createCFString(pasteboardBuffer.type);

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
