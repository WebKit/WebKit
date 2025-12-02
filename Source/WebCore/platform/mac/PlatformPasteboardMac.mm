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
#import "PlatformPasteboard.h"

#if PLATFORM(MAC)

#import "Color.h"
#import "ColorMac.h"
#import "CommonAtomStrings.h"
#import "LegacyNSPasteboardTypes.h"
#import "Pasteboard.h"
#import "SharedBuffer.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <pal/spi/cocoa/FoundationSPI.h>
#import <pal/spi/mac/NSPasteboardSPI.h>
#import <wtf/HashCountedSet.h>
#import <wtf/ListHashSet.h>
#import <wtf/URL.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringHash.h>

namespace WebCore {

static bool isFilePasteboardType(const String& type)
{
    RetainPtr nsType = type.createNSString();
    return [legacyFilenamesPasteboardTypeSingleton() isEqualToString:nsType.get()]
        || [legacyFilesPromisePasteboardTypeSingleton() isEqualToString:nsType.get()]
        || [UTTypeFileURL.identifier isEqualToString:nsType.get()];
}

static bool canWritePasteboardType(const String& type)
{
    if (isFilePasteboardType(type))
        return false;

    RetainPtr nsString = type.createNSString();
    RetainPtr utType = [UTType typeWithIdentifier:nsString.get()];
    if ([utType isDeclared] || [utType isDynamic])
        return true;

    return [nsString lengthOfBytesUsingEncoding:NSString.defaultCStringEncoding];
}

static bool canWriteAllPasteboardTypes(const Vector<String>& types)
{
    return !types.containsIf([](auto& type) {
        return !canWritePasteboardType(type);
    });
}

void PlatformPasteboard::performAsDataOwner(DataOwnerType, NOESCAPE Function<void()>&& actions)
{
    actions();
}

PlatformPasteboard::PlatformPasteboard(const String& pasteboardName)
    : m_pasteboard([NSPasteboard pasteboardWithName:pasteboardName.createNSString().get()])
{
    ASSERT(pasteboardName);
}

void PlatformPasteboard::getTypes(Vector<String>& types) const
{
    types = makeVector<String>(retainPtr([m_pasteboard types]).get());
}

PasteboardBuffer PlatformPasteboard::bufferForType(const String& pasteboardType) const
{
    RetainPtr<NSData> data;
    String bufferType = pasteboardType;

    if (pasteboardType == String(legacyTIFFPasteboardTypeSingleton())) {
        data = [m_pasteboard _dataWithoutConversionForType:pasteboardType.createNSString().get() securityScoped:NO];
        if (!data) {
            static NeverDestroyed<RetainPtr<NSArray>> sourceTypes = [] -> NSArray * {
                RetainPtr originalSourceTypes = adoptCF(CGImageSourceCopyTypeIdentifiers());
                if (originalSourceTypes)
                    return [(__bridge NSArray *)originalSourceTypes.get() arrayByExcludingObjectsInArray:@[UTTypePDF.identifier]];
                return nil;
            }();

            for (NSString *sourceType in sourceTypes.get().get()) {
                data = [m_pasteboard _dataWithoutConversionForType:sourceType securityScoped:NO];
                if (data) {
                    bufferType = sourceType;
                    break;
                }
            }
        }
    } else
        data = [m_pasteboard dataForType:pasteboardType.createNSString().get()];

    PasteboardBuffer pasteboardBuffer;
    pasteboardBuffer.type = bufferType;

    if (!data)
        return pasteboardBuffer;

    pasteboardBuffer.data = SharedBuffer::create(adoptNS([data copy]).get());
    return pasteboardBuffer;
}

int PlatformPasteboard::numberOfFiles() const
{
    Vector<String> files;

    RetainPtr<NSArray> pasteboardTypes = [m_pasteboard types];
    if ([pasteboardTypes containsObject:legacyFilesPromisePasteboardTypeSingleton()]) {
        // FIXME: legacyFilesPromisePasteboardTypeSingleton() contains file types, not path names, but in
        // this case we are only concerned with the count of them. The count of types should equal
        // the count of files, but this isn't guaranteed as some legacy providers might only write
        // unique file types.
        getPathnamesForType(files, String(legacyFilesPromisePasteboardTypeSingleton()));
        return files.size();
    }

    if ([pasteboardTypes containsObject:legacyFilenamesPasteboardTypeSingleton()]) {
        getPathnamesForType(files, String(legacyFilenamesPasteboardTypeSingleton()));
        return files.size();
    }

    return 0;
}

void PlatformPasteboard::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType) const
{
    if (!isFilePasteboardType(pasteboardType))
        return;
    RetainPtr paths = [m_pasteboard propertyListForType:pasteboardType.createNSString().get()];
    if (RetainPtr pathsString = dynamic_objc_cast<NSString>(paths.get())) {
        pathnames.append(pathsString.get());
        return;
    }
    pathnames = makeVector<String>(paths.get());
}

static bool pasteboardMayContainFilePaths(NSPasteboard *pasteboard)
{
    for (NSString *type in pasteboard.types) {
        if ([type isEqualToString:legacyFilenamesPasteboardTypeSingleton()] || [type isEqualToString:legacyFilesPromisePasteboardTypeSingleton()] || Pasteboard::shouldTreatCocoaTypeAsFile(type))
            return true;
    }
    return false;
}

String PlatformPasteboard::stringForType(const String& pasteboardType) const
{
    if (pasteboardType == String { legacyURLPasteboardTypeSingleton() }) {
        RetainPtr url = [NSURL URLFromPasteboard:m_pasteboard.get()];
        String urlString = (url ?: RetainPtr { [NSURL URLWithString:retainPtr([m_pasteboard stringForType:legacyURLPasteboardTypeSingleton()]).get()] }).get().absoluteString;
        if (pasteboardMayContainFilePaths(m_pasteboard.get()) && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString))
            return { };
        return urlString;
    }

    return [m_pasteboard stringForType:pasteboardType.createNSString().get()];
}

static Vector<String> urlStringsFromPasteboard(NSPasteboard *pasteboard)
{
    RetainPtr<NSArray<NSPasteboardItem *>> items = pasteboard.pasteboardItems;
    Vector<String> urlStrings;
    urlStrings.reserveInitialCapacity(items.get().count);
    if (items.get().count > 1) {
        for (NSPasteboardItem *item in items.get()) {
            if (RetainPtr<id> propertyList = [item propertyListForType:UTTypeURL.identifier]) {
                if (auto urlFromItem = adoptNS([[NSURL alloc] initWithPasteboardPropertyList:propertyList.get() ofType:UTTypeURL.identifier]))
                    urlStrings.append([urlFromItem absoluteString]);
            }
        }
    } else if (NSURL *urlFromPasteboard = [NSURL URLFromPasteboard:pasteboard])
        urlStrings.append(urlFromPasteboard.absoluteString);
    else if (RetainPtr<NSString> urlStringFromPasteboard = [pasteboard stringForType:legacyURLPasteboardTypeSingleton()])
        urlStrings.append(urlStringFromPasteboard.get());

    bool mayContainFiles = pasteboardMayContainFilePaths(pasteboard);
    urlStrings.removeAllMatching([&] (auto& urlString) {
        return urlString.isEmpty() || (mayContainFiles && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString));
    });

    return urlStrings;
}

static String typeIdentifierForPasteboardType(const String& pasteboardType)
{
    RetainPtr utType = [UTType typeWithIdentifier:pasteboardType.createNSString().get()];
    if ([utType isDeclared])
        return pasteboardType;

    if (pasteboardType == String(legacyStringPasteboardTypeSingleton()))
        return UTTypeUTF8PlainText.identifier;

    if (pasteboardType == String(legacyHTMLPasteboardTypeSingleton()))
        return UTTypeHTML.identifier;

    if (pasteboardType == String(legacyURLPasteboardTypeSingleton()))
        return UTTypeURL.identifier;

    return { };
}

Vector<String> PlatformPasteboard::allStringsForType(const String& pasteboardType) const
{
    auto typeIdentifier = typeIdentifierForPasteboardType(pasteboardType);
    if (typeIdentifier == String(UTTypeURL.identifier))
        return urlStringsFromPasteboard(m_pasteboard.get());

    RetainPtr<NSArray<NSPasteboardItem *>> items = [m_pasteboard pasteboardItems];
    Vector<String> strings;
    strings.reserveInitialCapacity(items.get().count);
    if (items.get().count > 1 && !typeIdentifier.isNull()) {
        for (NSPasteboardItem *item in items.get()) {
            if (RetainPtr stringFromItem = [item stringForType:typeIdentifier.createNSString().get()])
                strings.append(stringFromItem.get());
        }
    } else if (RetainPtr stringFromPasteboard = [m_pasteboard stringForType:pasteboardType.createNSString().get()])
        strings.append(stringFromPasteboard.get());

    return strings;
}

static ASCIILiteral safeTypeForDOMToReadAndWriteForPlatformType(NSString *platformType)
{
    if ([platformType isEqualToString:legacyStringPasteboardTypeSingleton()] || [platformType isEqualToString:NSPasteboardTypeString])
        return "text/plain"_s;

    if ([platformType isEqualToString:legacyURLPasteboardTypeSingleton()])
        return "text/uri-list"_s;

    if ([platformType isEqualToString:legacyHTMLPasteboardTypeSingleton()] || [platformType isEqualToString:UTTypeWebArchive.identifier]
        || [platformType  isEqualToString:legacyRTFDPasteboardTypeSingleton()] || [platformType isEqualToString:legacyRTFPasteboardTypeSingleton()])
        return "text/html"_s;

    RetainPtr nsWebArchivePboardType = String(WebArchivePboardType).createNSString();
    if ([platformType isEqualToString:nsWebArchivePboardType.get()])
        return "text/html"_s;

    return { };
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String& origin) const
{
    ListHashSet<String> domPasteboardTypes;
    if (RetainPtr serializedCustomData = [m_pasteboard dataForType:RetainPtr { @(PasteboardCustomData::cocoaType().characters()) }.get()]) {
        auto data = PasteboardCustomData::fromSharedBuffer(SharedBuffer::create(serializedCustomData.get()).get());
        if (data.origin() == origin) {
            for (auto& type : data.orderedTypes())
                domPasteboardTypes.add(type);
        }
    }

    RetainPtr<NSArray<NSString *>> allTypes = [m_pasteboard types];
    for (NSString *type in allTypes.get()) {
        if ([type isEqualToString:RetainPtr { @(PasteboardCustomData::cocoaType().characters()) }.get()])
            continue;

        if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type))
            domPasteboardTypes.add(type);
        else if (auto domType = safeTypeForDOMToReadAndWriteForPlatformType(type)) {
            if (domType == "text/uri-list"_s && stringForType(legacyURLPasteboardTypeSingleton()).isEmpty())
                continue;
            domPasteboardTypes.add(domType);
        }
    }

    return copyToVector(domPasteboardTypes);
}

int64_t PlatformPasteboard::write(const PasteboardCustomData& data, PasteboardDataLifetime pasteboardDataLifetime)
{
    NSMutableArray *types = [NSMutableArray array];
    data.forEachType([&] (auto& type) {
        RetainPtr platformType = platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type, IncludeImageTypes::Yes).createNSString();
        if (platformType.get().length)
            [types addObject:platformType.get()];
    });

    bool shouldWriteCustomData = data.hasSameOriginCustomData() || !data.origin().isEmpty();
    if (shouldWriteCustomData)
        [types addObject:RetainPtr { @(PasteboardCustomData::cocoaType().characters()) }.get()];

    [m_pasteboard declareTypes:types owner:nil];
    if (pasteboardDataLifetime == PasteboardDataLifetime::Ephemeral)
        [m_pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    data.forEachPlatformStringOrBuffer([&] (auto& type, auto& stringOrBuffer) {
        auto platformType = platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type, IncludeImageTypes::Yes);
        if (platformType.isEmpty())
            return;

        if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
            if (auto platformData = std::get<Ref<SharedBuffer>>(stringOrBuffer)->createNSData())
                [m_pasteboard setData:platformData.get() forType:platformType.createNSString().get()];
        } else if (std::holds_alternative<String>(stringOrBuffer)) {
            auto string = std::get<String>(stringOrBuffer);
            if (!!string)
                [m_pasteboard setString:string.createNSString().get() forType:platformType.createNSString().get()];
        }
    });

    if (shouldWriteCustomData) {
        if (auto serializedCustomData = data.createSharedBuffer()->createNSData())
            [m_pasteboard setData:serializedCustomData.get() forType:RetainPtr { @(PasteboardCustomData::cocoaType().characters()) }.get()];
    }

    return changeCount();
}

int64_t PlatformPasteboard::changeCount() const
{
    return [m_pasteboard changeCount];
}

String PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(const String& domType, IncludeImageTypes includeImageTypes)
{
    if (domType == textPlainContentTypeAtom())
        return legacyStringPasteboardTypeSingleton();

    if (domType == textHTMLContentTypeAtom())
        return legacyHTMLPasteboardTypeSingleton();

    if (domType == "text/uri-list"_s)
        return legacyURLPasteboardTypeSingleton();

    if (includeImageTypes == IncludeImageTypes::Yes && domType == "image/png"_s)
        return legacyPNGPasteboardTypeSingleton();

    return { };
}

Color PlatformPasteboard::color()
{
    return colorFromCocoaColor([NSColor colorFromPasteboard:m_pasteboard.get()]);
}

URL PlatformPasteboard::url()
{
    return [NSURL URLFromPasteboard:m_pasteboard.get()];
}

int64_t PlatformPasteboard::copy(const String& fromPasteboard)
{
    RetainPtr pasteboard = [NSPasteboard pasteboardWithName:fromPasteboard.createNSString().get()];
    RetainPtr<NSArray> types = [pasteboard types];

    [m_pasteboard addTypes:types.get() owner:nil];
    for (NSUInteger i = 0; i < [types count]; i++) {
        RetainPtr<NSString> type = [types objectAtIndex:i];
        if (![m_pasteboard setData:retainPtr([pasteboard dataForType:type.get()]).get() forType:type.get()])
            return 0;
    }
    return changeCount();
}

int64_t PlatformPasteboard::addTypes(const Vector<String>& pasteboardTypes)
{
    if (!canWriteAllPasteboardTypes(pasteboardTypes))
        return 0;
    return [m_pasteboard addTypes:createNSArray(pasteboardTypes).get() owner:nil];
}

int64_t PlatformPasteboard::setTypes(const Vector<String>& pasteboardTypes, PasteboardDataLifetime pasteboardDataLifetime)
{
    auto didClearContents = [m_pasteboard clearContents];

    if (pasteboardDataLifetime == PasteboardDataLifetime::Ephemeral)
        [m_pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    if (!canWriteAllPasteboardTypes(pasteboardTypes))
        return didClearContents;
    return [m_pasteboard addTypes:createNSArray(pasteboardTypes).get() owner:nil];
}

int64_t PlatformPasteboard::setBufferForType(SharedBuffer* buffer, const String& pasteboardType)
{
    if (!canWritePasteboardType(pasteboardType))
        return 0;

    BOOL didWriteData = [m_pasteboard setData:buffer ? buffer->createNSData().get() : nil forType:pasteboardType.createNSString().get()];
    if (!didWriteData)
        return 0;
    return changeCount();
}

int64_t PlatformPasteboard::setURL(const PasteboardURL& pasteboardURL)
{
    RetainPtr urlString = [pasteboardURL.url.createNSURL() absoluteString];
    if (!urlString)
        return 0;

    RetainPtr urlWithTitle = @[ @[ urlString.get() ], @[ pasteboardURL.title.createNSString().get() ] ];
    RetainPtr pasteboardType = adoptNS([[NSString alloc] initWithUTF8String:WebURLsWithTitlesPboardType]);
    BOOL didWriteData = [m_pasteboard setPropertyList:urlWithTitle.get() forType:pasteboardType.get()];
    if (!didWriteData)
        return 0;

    return changeCount();
}

int64_t PlatformPasteboard::setColor(const Color& color)
{
    [cocoaColor(color) writeToPasteboard:m_pasteboard.get()];
    return changeCount();
}

int64_t PlatformPasteboard::setStringForType(const String& string, const String& pasteboardType)
{
    if (!canWritePasteboardType(pasteboardType))
        return 0;

    BOOL didWriteData;

    if (pasteboardType == String(legacyURLPasteboardTypeSingleton())) {
        // We cannot just use -NSPasteboard writeObjects:], because -declareTypes has been already called, implicitly creating an item.
        RetainPtr url = adoptNS([[NSURL alloc] initWithString:string.createNSString().get()]);
        if ([retainPtr([m_pasteboard types]) containsObject:legacyURLPasteboardTypeSingleton()]) {
            RetainPtr<NSURL> base = [url baseURL];
            if (base)
                didWriteData = [m_pasteboard setPropertyList:@[[url relativeString], [base absoluteString]] forType:legacyURLPasteboardTypeSingleton()];
            else if (url)
                didWriteData = [m_pasteboard setPropertyList:@[[url absoluteString], @""] forType:legacyURLPasteboardTypeSingleton()];
            else
                didWriteData = [m_pasteboard setPropertyList:@[@"", @""] forType:legacyURLPasteboardTypeSingleton()];

            if (!didWriteData)
                return 0;
        }

        if ([retainPtr([m_pasteboard types]) containsObject:UTTypeURL.identifier]) {
            didWriteData = [m_pasteboard setString:retainPtr([url absoluteString]).get() forType:UTTypeURL.identifier];
            if (!didWriteData)
                return 0;
        }

        if ([retainPtr([m_pasteboard types]) containsObject:UTTypeFileURL.identifier] && [url isFileURL]) {
            didWriteData = [m_pasteboard setString:retainPtr([url absoluteString]).get() forType:UTTypeFileURL.identifier];
            if (!didWriteData)
                return 0;
        }

    } else {
        didWriteData = [m_pasteboard setString:string.createNSString().get() forType:pasteboardType.createNSString().get()];
        if (!didWriteData)
            return 0;
    }

    return changeCount();
}

static NSPasteboardType modernPasteboardTypeForWebSafeMIMEType(const String& webSafeType)
{
    if (webSafeType == textPlainContentTypeAtom())
        return NSPasteboardTypeString;
    if (webSafeType == textHTMLContentTypeAtom())
        return NSPasteboardTypeHTML;
    if (webSafeType == "text/uri-list"_s)
        return NSPasteboardTypeURL;
    if (webSafeType == "image/png"_s)
        return NSPasteboardTypePNG;
    return nil;
}

enum class ContainsFileURL : bool { No, Yes };
static String webSafeMIMETypeForModernPasteboardType(NSPasteboardType platformType, ContainsFileURL containsFileURL)
{
    if ([platformType isEqual:NSPasteboardTypeString] && containsFileURL == ContainsFileURL::No)
        return textPlainContentTypeAtom();
    if ([platformType isEqual:NSPasteboardTypeHTML] || [platformType isEqual:NSPasteboardTypeRTF] || [platformType isEqual:NSPasteboardTypeRTFD])
        return textHTMLContentTypeAtom();
    if ([platformType isEqual:NSPasteboardTypeURL] && containsFileURL == ContainsFileURL::No)
        return "text/uri-list"_s;
    if ([platformType isEqual:NSPasteboardTypePNG] || [platformType isEqual:NSPasteboardTypeTIFF])
        return "image/png"_s;
    return { };
}

RefPtr<SharedBuffer> PlatformPasteboard::readBuffer(std::optional<size_t> index, const String& type) const
{
    if (!index)
        return bufferForType(type).data;

    RetainPtr item = itemAtIndex(*index);
    if (!item)
        return { };

    if (RetainPtr data = [item dataForType:type.createNSString().get()]) {
        RetainPtr nsData = adoptNS([data copy]);
        return SharedBuffer::create(nsData.get());
    }

    return nullptr;
}

String PlatformPasteboard::readString(size_t index, const String& type) const
{
    RetainPtr item = itemAtIndex(index);
    if (!item)
        return { };

    return [item stringForType:type.createNSString().get()];
}

URL PlatformPasteboard::readURL(size_t index, String& title) const
{
    title = emptyString();

    RetainPtr item = itemAtIndex(index);
    if (!item)
        return { };

    RetainPtr<NSURL> url;
    if (RetainPtr<id> propertyList = [item propertyListForType:NSPasteboardTypeURL])
        url = adoptNS([[NSURL alloc] initWithPasteboardPropertyList:propertyList.get() ofType:NSPasteboardTypeURL]);
    else if (RetainPtr<NSString> absoluteString = [item stringForType:NSPasteboardTypeURL])
        url = [NSURL URLWithString:absoluteString.get()];
    return { [url isFileURL] ? nil : url.get() };
}

int PlatformPasteboard::count() const
{
    return [m_pasteboard pasteboardItems].count;
}

static RetainPtr<NSPasteboardItem> createPasteboardItem(const PasteboardCustomData& data)
{
    auto item = adoptNS([[NSPasteboardItem alloc] init]);

    if (data.hasSameOriginCustomData() || !data.origin().isEmpty()) {
        if (auto serializedCustomData = data.createSharedBuffer()->createNSData())
            [item setData:serializedCustomData.get() forType:RetainPtr { @(PasteboardCustomData::cocoaType().characters()) }.get()];
    }

    data.forEachPlatformStringOrBuffer([&] (auto& type, auto& stringOrBuffer) {
        RetainPtr platformType = modernPasteboardTypeForWebSafeMIMEType(type);
        if (!platformType)
            return;

        if (std::holds_alternative<Ref<SharedBuffer>>(stringOrBuffer)) {
            if (auto platformData = std::get<Ref<SharedBuffer>>(stringOrBuffer)->createNSData())
                [item setData:platformData.get() forType:platformType.get()];
        } else if (std::holds_alternative<String>(stringOrBuffer)) {
            auto string = std::get<String>(stringOrBuffer);
            if (!!string)
                [item setString:string.createNSString().get() forType:platformType.get()];
        }
    });

    return item;
}

int64_t PlatformPasteboard::write(const Vector<PasteboardCustomData>& itemData, PasteboardDataLifetime pasteboardDataLifetime)
{
    if (itemData.size() == 1)
        return write(itemData.first(), pasteboardDataLifetime);

    [m_pasteboard clearContents];
    if (pasteboardDataLifetime == PasteboardDataLifetime::Ephemeral)
        [m_pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    [m_pasteboard writeObjects:createNSArray(itemData, [] (auto& data) {
        return createPasteboardItem(data);
    }).get()];
    return [m_pasteboard changeCount];
}

std::optional<PasteboardItemInfo> PlatformPasteboard::informationForItemAtIndex(size_t index, int64_t changeCount)
{
    if (changeCount != [m_pasteboard changeCount])
        return std::nullopt;

    RetainPtr item = itemAtIndex(index);
    if (!item)
        return std::nullopt;

    PasteboardItemInfo info;
    RetainPtr<NSArray<NSPasteboardType>> platformTypes = [item types];
    auto containsFileURL = [platformTypes containsObject:NSPasteboardTypeFileURL] ? ContainsFileURL::Yes : ContainsFileURL::No;
    ListHashSet<String> webSafeTypes;
    info.platformTypesByFidelity.reserveInitialCapacity(platformTypes.get().count);
    for (NSPasteboardType type in platformTypes.get()) {
        info.platformTypesByFidelity.append(type);
        auto webSafeType = webSafeMIMETypeForModernPasteboardType(type, containsFileURL);
        if (webSafeType.isEmpty())
            continue;

        webSafeTypes.add(WTFMove(webSafeType));
    }
    info.containsFileURLAndFileUploadContent = containsFileURL == ContainsFileURL::Yes;
    info.webSafeTypesByFidelity = copyToVector(webSafeTypes);
    return info;
}

NSPasteboardItem *PlatformPasteboard::itemAtIndex(size_t index) const
{
    NSArray<NSPasteboardItem *> *items = [m_pasteboard pasteboardItems];
    return index >= items.count ? nil : items[index];
}

bool PlatformPasteboard::containsURLStringSuitableForLoading()
{
    String unusedTitle;
    return !urlStringSuitableForLoading(unusedTitle).isEmpty();
}

}

#endif // PLATFORM(MAC)
