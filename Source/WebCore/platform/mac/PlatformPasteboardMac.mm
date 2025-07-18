/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
    return [legacyFilenamesPasteboardType() isEqualToString:nsType.get()]
        || [legacyFilesPromisePasteboardType() isEqualToString:nsType.get()]
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        || [bridge_cast(kUTTypeFileURL) isEqualToString:nsType.get()];
ALLOW_DEPRECATED_DECLARATIONS_END
}

static bool canWritePasteboardType(const String& type)
{
    if (isFilePasteboardType(type))
        return false;
    auto cfString = type.createCFString();
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (UTTypeIsDeclared(cfString.get()) || UTTypeIsDynamic(cfString.get()))
        return true;
ALLOW_DEPRECATED_DECLARATIONS_END

    return [(__bridge NSString *)cfString.get() lengthOfBytesUsingEncoding:NSString.defaultCStringEncoding];
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
    types = makeVector<String>([m_pasteboard types]);
}

PasteboardBuffer PlatformPasteboard::bufferForType(const String& pasteboardType) const
{
    NSData *data = nil;
    String bufferType = pasteboardType;

    if (pasteboardType == String(legacyTIFFPasteboardType())) {
        data = [m_pasteboard _dataWithoutConversionForType:pasteboardType.createNSString().get() securityScoped:NO];
        if (!data) {
            static NeverDestroyed<RetainPtr<NSArray>> sourceTypes;
            static std::once_flag onceFlag;
            std::call_once(onceFlag, [] {
                auto originalSourceTypes = adoptCF(CGImageSourceCopyTypeIdentifiers());
                if (originalSourceTypes) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                    sourceTypes.get() = [(__bridge NSArray *)originalSourceTypes.get() arrayByExcludingObjectsInArray:@[(__bridge NSString *)kUTTypePDF]];
ALLOW_DEPRECATED_DECLARATIONS_END
                } else
                    sourceTypes.get() = nil;
            });

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

    NSArray *pasteboardTypes = [m_pasteboard types];
    if ([pasteboardTypes containsObject:legacyFilesPromisePasteboardType()]) {
        // FIXME: legacyFilesPromisePasteboardType() contains file types, not path names, but in
        // this case we are only concerned with the count of them. The count of types should equal
        // the count of files, but this isn't guaranteed as some legacy providers might only write
        // unique file types.
        getPathnamesForType(files, String(legacyFilesPromisePasteboardType()));
        return files.size();
    }

    if ([pasteboardTypes containsObject:legacyFilenamesPasteboardType()]) {
        getPathnamesForType(files, String(legacyFilenamesPasteboardType()));
        return files.size();
    }

    return 0;
}

void PlatformPasteboard::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType) const
{
    if (!isFilePasteboardType(pasteboardType))
        return;
    id paths = [m_pasteboard propertyListForType:pasteboardType.createNSString().get()];
    if (auto *pathsString = dynamic_objc_cast<NSString>(paths)) {
        pathnames.append(pathsString);
        return;
    }
    pathnames = makeVector<String>(paths);
}

static bool pasteboardMayContainFilePaths(NSPasteboard *pasteboard)
{
    for (NSString *type in pasteboard.types) {
        if ([type isEqualToString:legacyFilenamesPasteboardType()] || [type isEqualToString:legacyFilesPromisePasteboardType()] || Pasteboard::shouldTreatCocoaTypeAsFile(type))
            return true;
    }
    return false;
}

String PlatformPasteboard::stringForType(const String& pasteboardType) const
{
    if (pasteboardType == String { legacyURLPasteboardType() }) {
        String urlString = ([NSURL URLFromPasteboard:m_pasteboard.get()] ?: [NSURL URLWithString:[m_pasteboard stringForType:legacyURLPasteboardType()]]).absoluteString;
        if (pasteboardMayContainFilePaths(m_pasteboard.get()) && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString))
            return { };
        return urlString;
    }

    return [m_pasteboard stringForType:pasteboardType.createNSString().get()];
}

static Vector<String> urlStringsFromPasteboard(NSPasteboard *pasteboard)
{
    NSArray<NSPasteboardItem *> *items = pasteboard.pasteboardItems;
    Vector<String> urlStrings;
    urlStrings.reserveInitialCapacity(items.count);
    if (items.count > 1) {
        for (NSPasteboardItem *item in items) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (id propertyList = [item propertyListForType:(__bridge NSString *)kUTTypeURL]) {
                if (auto urlFromItem = adoptNS([[NSURL alloc] initWithPasteboardPropertyList:propertyList ofType:(__bridge NSString *)kUTTypeURL]))
                    urlStrings.append([urlFromItem absoluteString]);
            }
ALLOW_DEPRECATED_DECLARATIONS_END
        }
    } else if (NSURL *urlFromPasteboard = [NSURL URLFromPasteboard:pasteboard])
        urlStrings.append(urlFromPasteboard.absoluteString);
    else if (NSString *urlStringFromPasteboard = [pasteboard stringForType:legacyURLPasteboardType()])
        urlStrings.append(urlStringFromPasteboard);

    bool mayContainFiles = pasteboardMayContainFilePaths(pasteboard);
    urlStrings.removeAllMatching([&] (auto& urlString) {
        return urlString.isEmpty() || (mayContainFiles && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString));
    });

    return urlStrings;
}

static String typeIdentifierForPasteboardType(const String& pasteboardType)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (UTTypeIsDeclared(pasteboardType.createCFString().get()))
        return pasteboardType;

    if (pasteboardType == String(legacyStringPasteboardType()))
        return kUTTypeUTF8PlainText;

    if (pasteboardType == String(legacyHTMLPasteboardType()))
        return kUTTypeHTML;

    if (pasteboardType == String(legacyURLPasteboardType()))
        return kUTTypeURL;
ALLOW_DEPRECATED_DECLARATIONS_END

    return { };
}

Vector<String> PlatformPasteboard::allStringsForType(const String& pasteboardType) const
{
    auto typeIdentifier = typeIdentifierForPasteboardType(pasteboardType);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (typeIdentifier == String(kUTTypeURL))
        return urlStringsFromPasteboard(m_pasteboard.get());
ALLOW_DEPRECATED_DECLARATIONS_END

    NSArray<NSPasteboardItem *> *items = [m_pasteboard pasteboardItems];
    Vector<String> strings;
    strings.reserveInitialCapacity(items.count);
    if (items.count > 1 && !typeIdentifier.isNull()) {
        for (NSPasteboardItem *item in items) {
            if (NSString *stringFromItem = [item stringForType:typeIdentifier.createNSString().get()])
                strings.append(stringFromItem);
        }
    } else if (NSString *stringFromPasteboard = [m_pasteboard stringForType:pasteboardType.createNSString().get()])
        strings.append(stringFromPasteboard);

    return strings;
}

static ASCIILiteral safeTypeForDOMToReadAndWriteForPlatformType(const String& platformType)
{
    if (platformType == String(legacyStringPasteboardType()) || platformType == String(NSPasteboardTypeString))
        return "text/plain"_s;

    if (platformType == String(legacyURLPasteboardType()))
        return "text/uri-list"_s;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (platformType == String(legacyHTMLPasteboardType()) || platformType == String(WebArchivePboardType) || platformType == String(kUTTypeWebArchive)
        || platformType == String(legacyRTFDPasteboardType()) || platformType == String(legacyRTFPasteboardType()))
        return "text/html"_s;
ALLOW_DEPRECATED_DECLARATIONS_END

    return { };
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String& origin) const
{
    ListHashSet<String> domPasteboardTypes;
    if (NSData *serializedCustomData = [m_pasteboard dataForType:@(PasteboardCustomData::cocoaType().characters())]) {
        auto data = PasteboardCustomData::fromSharedBuffer(SharedBuffer::create(serializedCustomData).get());
        if (data.origin() == origin) {
            for (auto& type : data.orderedTypes())
                domPasteboardTypes.add(type);
        }
    }

    NSArray<NSString *> *allTypes = [m_pasteboard types];
    for (NSString *type in allTypes) {
        if ([type isEqualToString:@(PasteboardCustomData::cocoaType().characters())])
            continue;

        if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type))
            domPasteboardTypes.add(type);
        else if (auto domType = safeTypeForDOMToReadAndWriteForPlatformType(type)) {
            if (domType == "text/uri-list"_s && stringForType(legacyURLPasteboardType()).isEmpty())
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
        [types addObject:@(PasteboardCustomData::cocoaType().characters())];

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
            [m_pasteboard setData:serializedCustomData.get() forType:@(PasteboardCustomData::cocoaType().characters())];
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
        return legacyStringPasteboardType();

    if (domType == textHTMLContentTypeAtom())
        return legacyHTMLPasteboardType();

    if (domType == "text/uri-list"_s)
        return legacyURLPasteboardType();

    if (includeImageTypes == IncludeImageTypes::Yes && domType == "image/png"_s)
        return legacyPNGPasteboardType();

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
    NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:fromPasteboard.createNSString().get()];
    NSArray* types = [pasteboard types];

    [m_pasteboard addTypes:types owner:nil];
    for (NSUInteger i = 0; i < [types count]; i++) {
        NSString* type = [types objectAtIndex:i];
        if (![m_pasteboard setData:[pasteboard dataForType:type] forType:type])
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

    NSArray *urlWithTitle = @[ @[ urlString.get() ], @[ pasteboardURL.title.createNSString().get() ] ];
    NSString *pasteboardType = [NSString stringWithUTF8String:WebURLsWithTitlesPboardType];
    BOOL didWriteData = [m_pasteboard setPropertyList:urlWithTitle forType:pasteboardType];
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

    if (pasteboardType == String(legacyURLPasteboardType())) {
        // We cannot just use -NSPasteboard writeObjects:], because -declareTypes has been already called, implicitly creating an item.
        RetainPtr url = adoptNS([[NSURL alloc] initWithString:string.createNSString().get()]);
        if ([[m_pasteboard types] containsObject:legacyURLPasteboardType()]) {
            NSURL *base = [url baseURL];
            if (base)
                didWriteData = [m_pasteboard setPropertyList:@[[url relativeString], [base absoluteString]] forType:legacyURLPasteboardType()];
            else if (url)
                didWriteData = [m_pasteboard setPropertyList:@[[url absoluteString], @""] forType:legacyURLPasteboardType()];
            else
                didWriteData = [m_pasteboard setPropertyList:@[@"", @""] forType:legacyURLPasteboardType()];

            if (!didWriteData)
                return 0;
        }

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([[m_pasteboard types] containsObject:bridge_cast(kUTTypeURL)]) {
            didWriteData = [m_pasteboard setString:[url absoluteString] forType:bridge_cast(kUTTypeURL)];
            if (!didWriteData)
                return 0;
        }

        if ([[m_pasteboard types] containsObject:bridge_cast(kUTTypeFileURL)] && [url isFileURL]) {
            didWriteData = [m_pasteboard setString:[url absoluteString] forType:bridge_cast(kUTTypeFileURL)];
            if (!didWriteData)
                return 0;
        }
ALLOW_DEPRECATED_DECLARATIONS_END

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

    NSPasteboardItem *item = itemAtIndex(*index);
    if (!item)
        return { };

    if (NSData *data = [item dataForType:type.createNSString().get()]) {
        auto nsData = adoptNS(data.copy);
        return SharedBuffer::create(nsData.get());
    }

    return nullptr;
}

String PlatformPasteboard::readString(size_t index, const String& type) const
{
    NSPasteboardItem *item = itemAtIndex(index);
    if (!item)
        return { };

    return [item stringForType:type.createNSString().get()];
}

URL PlatformPasteboard::readURL(size_t index, String& title) const
{
    title = emptyString();

    NSPasteboardItem *item = itemAtIndex(index);
    if (!item)
        return { };

    RetainPtr<NSURL> url;
    if (id propertyList = [item propertyListForType:NSPasteboardTypeURL])
        url = adoptNS([[NSURL alloc] initWithPasteboardPropertyList:propertyList ofType:NSPasteboardTypeURL]);
    else if (NSString *absoluteString = [item stringForType:NSPasteboardTypeURL])
        url = [NSURL URLWithString:absoluteString];
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
            [item setData:serializedCustomData.get() forType:@(PasteboardCustomData::cocoaType().characters())];
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

    NSPasteboardItem *item = itemAtIndex(index);
    if (!item)
        return std::nullopt;

    PasteboardItemInfo info;
    NSArray<NSPasteboardType> *platformTypes = [item types];
    auto containsFileURL = [platformTypes containsObject:NSPasteboardTypeFileURL] ? ContainsFileURL::Yes : ContainsFileURL::No;
    ListHashSet<String> webSafeTypes;
    info.platformTypesByFidelity.reserveInitialCapacity(platformTypes.count);
    for (NSPasteboardType type in platformTypes) {
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
