/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#import "LegacyNSPasteboardTypes.h"
#import "Pasteboard.h"
#import "SharedBuffer.h"
#import <wtf/HashCountedSet.h>
#import <wtf/ListHashSet.h>
#import <wtf/URL.h>
#import <wtf/text/StringHash.h>

namespace WebCore {

PlatformPasteboard::PlatformPasteboard(const String& pasteboardName)
    : m_pasteboard([NSPasteboard pasteboardWithName:pasteboardName])
{
    ASSERT(pasteboardName);
}

void PlatformPasteboard::getTypes(Vector<String>& types)
{
    NSArray *pasteboardTypes = [m_pasteboard.get() types];

    for (NSUInteger i = 0; i < [pasteboardTypes count]; i++)
        types.append([pasteboardTypes objectAtIndex:i]);
}

RefPtr<SharedBuffer> PlatformPasteboard::bufferForType(const String& pasteboardType)
{
    NSData *data = [m_pasteboard.get() dataForType:pasteboardType];
    if (!data)
        return nullptr;
    return SharedBuffer::create(adoptNS([data copy]).get());
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
    NSArray* paths = [m_pasteboard.get() propertyListForType:pasteboardType];
    if ([paths isKindOfClass:[NSString class]]) {
        pathnames.append((NSString *)paths);
        return;
    }
    for (NSUInteger i = 0; i < [paths count]; i++)
        pathnames.append([paths objectAtIndex:i]);
}

static bool pasteboardMayContainFilePaths(NSPasteboard *pasteboard)
{
    for (NSString *type in pasteboard.types) {
        if ([type isEqualToString:(NSString *)legacyFilenamesPasteboardType()] || [type isEqualToString:(NSString *)legacyFilesPromisePasteboardType()] || Pasteboard::shouldTreatCocoaTypeAsFile(type))
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

    return [m_pasteboard stringForType:pasteboardType];
}

static Vector<String> urlStringsFromPasteboard(NSPasteboard *pasteboard)
{
    NSArray<NSPasteboardItem *> *items = pasteboard.pasteboardItems;
    Vector<String> urlStrings;
    urlStrings.reserveInitialCapacity(items.count);
    if (items.count > 1) {
        for (NSPasteboardItem *item in items) {
            if (id propertyList = [item propertyListForType:(__bridge NSString *)kUTTypeURL]) {
                if (auto urlFromItem = adoptNS([[NSURL alloc] initWithPasteboardPropertyList:propertyList ofType:(__bridge NSString *)kUTTypeURL]))
                    urlStrings.uncheckedAppend([urlFromItem absoluteString]);
            }
        }
    } else if (NSURL *urlFromPasteboard = [NSURL URLFromPasteboard:pasteboard])
        urlStrings.uncheckedAppend(urlFromPasteboard.absoluteString);
    else if (NSString *urlStringFromPasteboard = [pasteboard stringForType:legacyURLPasteboardType()])
        urlStrings.uncheckedAppend(urlStringFromPasteboard);

    bool mayContainFiles = pasteboardMayContainFilePaths(pasteboard);
    urlStrings.removeAllMatching([&] (auto& urlString) {
        return urlString.isEmpty() || (mayContainFiles && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString));
    });

    return urlStrings;
}

static String typeIdentifierForPasteboardType(const String& pasteboardType)
{
    if (UTTypeIsDeclared(pasteboardType.createCFString().get()))
        return pasteboardType;

    if (pasteboardType == String(legacyStringPasteboardType()))
        return kUTTypeUTF8PlainText;

    if (pasteboardType == String(legacyHTMLPasteboardType()))
        return kUTTypeHTML;

    if (pasteboardType == String(legacyURLPasteboardType()))
        return kUTTypeURL;

    return { };
}

Vector<String> PlatformPasteboard::allStringsForType(const String& pasteboardType) const
{
    auto typeIdentifier = typeIdentifierForPasteboardType(pasteboardType);
    if (typeIdentifier == String(kUTTypeURL))
        return urlStringsFromPasteboard(m_pasteboard.get());

    NSArray<NSPasteboardItem *> *items = [m_pasteboard pasteboardItems];
    Vector<String> strings;
    strings.reserveInitialCapacity(items.count);
    if (items.count > 1 && !typeIdentifier.isNull()) {
        for (NSPasteboardItem *item in items) {
            if (NSString *stringFromItem = [item stringForType:typeIdentifier])
                strings.append(stringFromItem);
        }
    } else if (NSString *stringFromPasteboard = [m_pasteboard stringForType:pasteboardType])
        strings.append(stringFromPasteboard);

    return strings;
}

static const char* safeTypeForDOMToReadAndWriteForPlatformType(const String& platformType)
{
    if (platformType == String(legacyStringPasteboardType()) || platformType == String(NSPasteboardTypeString))
        return "text/plain"_s;

    if (platformType == String(legacyURLPasteboardType()))
        return "text/uri-list"_s;

    if (platformType == String(legacyHTMLPasteboardType()) || platformType == String(WebArchivePboardType)
        || platformType == String(legacyRTFDPasteboardType()) || platformType == String(legacyRTFPasteboardType()))
        return "text/html"_s;

    return nullptr;
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String& origin) const
{
    ListHashSet<String> domPasteboardTypes;
    if (NSData *serializedCustomData = [m_pasteboard dataForType:@(PasteboardCustomData::cocoaType())]) {
        auto data = PasteboardCustomData::fromSharedBuffer(SharedBuffer::create(serializedCustomData).get());
        if (data.origin == origin) {
            for (auto& type : data.orderedTypes)
                domPasteboardTypes.add(type);
        }
    }

    NSArray<NSString *> *allTypes = [m_pasteboard types];
    for (NSString *type in allTypes) {
        if ([type isEqualToString:@(PasteboardCustomData::cocoaType())])
            continue;

        if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type))
            domPasteboardTypes.add(type);
        else if (auto* domType = safeTypeForDOMToReadAndWriteForPlatformType(type)) {
            auto domTypeAsString = String::fromUTF8(domType);
            if (domTypeAsString == "text/uri-list" && stringForType(legacyURLPasteboardType()).isEmpty())
                continue;
            domPasteboardTypes.add(WTFMove(domTypeAsString));
        }
    }

    return copyToVector(domPasteboardTypes);
}

long PlatformPasteboard::write(const PasteboardCustomData& data)
{
    NSMutableArray *types = [NSMutableArray array];
    for (auto& entry : data.platformData)
        [types addObject:platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(entry.key)];
    if (data.sameOriginCustomData.size())
        [types addObject:@(PasteboardCustomData::cocoaType())];

    [m_pasteboard declareTypes:types owner:nil];

    for (auto& entry : data.platformData) {
        auto platformType = platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(entry.key);
        ASSERT(!platformType.isEmpty());
        if (!platformType.isEmpty())
            [m_pasteboard setString:entry.value forType:platformType];
    }

    if (data.sameOriginCustomData.size()) {
        if (auto serializedCustomData = data.createSharedBuffer()->createNSData())
            [m_pasteboard setData:serializedCustomData.get() forType:@(PasteboardCustomData::cocoaType())];
    }

    return changeCount();
}

long PlatformPasteboard::changeCount() const
{
    return [m_pasteboard.get() changeCount];
}

String PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(const String& domType)
{
    if (domType == "text/plain")
        return legacyStringPasteboardType();

    if (domType == "text/html")
        return legacyHTMLPasteboardType();

    if (domType == "text/uri-list")
        return legacyURLPasteboardType();

    return { };
}

String PlatformPasteboard::uniqueName()
{
    return [[NSPasteboard pasteboardWithUniqueName] name];
}

Color PlatformPasteboard::color()
{
    return colorFromNSColor([NSColor colorFromPasteboard:m_pasteboard.get()]);
}

URL PlatformPasteboard::url()
{
    return [NSURL URLFromPasteboard:m_pasteboard.get()];
}

long PlatformPasteboard::copy(const String& fromPasteboard)
{
    NSPasteboard* pasteboard = [NSPasteboard pasteboardWithName:fromPasteboard];
    NSArray* types = [pasteboard types];

    [m_pasteboard.get() addTypes:types owner:nil];
    for (NSUInteger i = 0; i < [types count]; i++) {
        NSString* type = [types objectAtIndex:i];
        if (![m_pasteboard.get() setData:[pasteboard dataForType:type] forType:type])
            return 0;
    }
    return changeCount();
}

long PlatformPasteboard::addTypes(const Vector<String>& pasteboardTypes)
{
    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] init]);
    for (size_t i = 0; i < pasteboardTypes.size(); ++i)
        [types.get() addObject:pasteboardTypes[i]];

    return [m_pasteboard.get() addTypes:types.get() owner:nil];
}

long PlatformPasteboard::setTypes(const Vector<String>& pasteboardTypes)
{
    if (pasteboardTypes.isEmpty())
        return [m_pasteboard declareTypes:@[] owner:nil];

    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] init]);
    for (size_t i = 0; i < pasteboardTypes.size(); ++i)
        [types.get() addObject:pasteboardTypes[i]];

    return [m_pasteboard.get() declareTypes:types.get() owner:nil];
}

long PlatformPasteboard::setBufferForType(SharedBuffer* buffer, const String& pasteboardType)
{
    BOOL didWriteData = [m_pasteboard setData:buffer ? buffer->createNSData().get() : nil forType:pasteboardType];
    if (!didWriteData)
        return 0;
    return changeCount();
}

long PlatformPasteboard::setURL(const PasteboardURL& pasteboardURL)
{
    NSURL *cocoaURL = pasteboardURL.url;
    NSArray *urlWithTitle = @[ @[ cocoaURL.absoluteString ], @[ pasteboardURL.title ] ];
    NSString *pasteboardType = [NSString stringWithUTF8String:WebURLsWithTitlesPboardType];
    BOOL didWriteData = [m_pasteboard.get() setPropertyList:urlWithTitle forType:pasteboardType];
    if (!didWriteData)
        return 0;

    return changeCount();
}

long PlatformPasteboard::setColor(const Color& color)
{
    NSColor *pasteboardColor = nsColor(color);
    [pasteboardColor writeToPasteboard:m_pasteboard.get()];
    return changeCount();
}

long PlatformPasteboard::setStringForType(const String& string, const String& pasteboardType)
{
    BOOL didWriteData;

    if (pasteboardType == String(legacyURLPasteboardType())) {
        // We cannot just use -NSPasteboard writeObjects:], because -declareTypes has been already called, implicitly creating an item.
        NSURL *url = [NSURL URLWithString:string];
        if ([[m_pasteboard.get() types] containsObject:legacyURLPasteboardType()]) {
            NSURL *base = [url baseURL];
            if (base)
                didWriteData = [m_pasteboard.get() setPropertyList:@[[url relativeString], [base absoluteString]] forType:legacyURLPasteboardType()];
            else if (url)
                didWriteData = [m_pasteboard.get() setPropertyList:@[[url absoluteString], @""] forType:legacyURLPasteboardType()];
            else
                didWriteData = [m_pasteboard.get() setPropertyList:@[@"", @""] forType:legacyURLPasteboardType()];

            if (!didWriteData)
                return 0;
        }

        if ([[m_pasteboard.get() types] containsObject:(NSString *)kUTTypeURL]) {
            didWriteData = [m_pasteboard.get() setString:[url absoluteString] forType:(NSString *)kUTTypeURL];
            if (!didWriteData)
                return 0;
        }

        if ([[m_pasteboard.get() types] containsObject:(NSString *)kUTTypeFileURL] && [url isFileURL]) {
            didWriteData = [m_pasteboard.get() setString:[url absoluteString] forType:(NSString *)kUTTypeFileURL];
            if (!didWriteData)
                return 0;
        }

    } else {
        didWriteData = [m_pasteboard.get() setString:string forType:pasteboardType];
        if (!didWriteData)
            return 0;
    }

    return changeCount();
}

}

#endif // PLATFORM(MAC)
