/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#import "DragData.h"
#import "Image.h"
#import "LegacyNSPasteboardTypes.h"
#import "LoaderNSURLExtras.h"
#import "MIMETypeRegistry.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "SharedBuffer.h"
#import "UTIUtilities.h"
#import "WebNSAttributedStringExtras.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/URL.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/unicode/CharacterNames.h>

namespace WebCore {

const char* const WebArchivePboardType = "Apple Web Archive pasteboard type";
const char* const WebURLNamePboardType = "public.url-name";
const char* const WebURLsWithTitlesPboardType = "WebURLsWithTitlesPboardType";

const char WebSmartPastePboardType[] = "NeXT smart paste pasteboard type";
const char WebURLPboardType[] = "public.url";

static const Vector<String> writableTypesForURL()
{
    Vector<String> types;
    
    types.append(WebURLsWithTitlesPboardType);
    types.append(String(legacyURLPasteboardType()));
    types.append(WebURLPboardType);
    types.append(WebURLNamePboardType);
    types.append(String(legacyStringPasteboardType()));
    return types;
}

static Vector<String> writableTypesForImage()
{
    Vector<String> types;
    types.append(String(legacyTIFFPasteboardType()));
    types.appendVector(writableTypesForURL());
    types.append(String(legacyRTFDPasteboardType()));
    return types;
}

NSArray *Pasteboard::supportedFileUploadPasteboardTypes()
{
    return @[ (NSString *)legacyFilesPromisePasteboardType(), (NSString *)legacyFilenamesPasteboardType() ];
}

Pasteboard::Pasteboard()
    : m_pasteboardName(emptyString())
    , m_changeCount(0)
{
}

Pasteboard::Pasteboard(const String& pasteboardName, const Vector<String>& promisedFilePaths)
    : m_pasteboardName(pasteboardName)
    , m_changeCount(platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
    , m_promisedFilePaths(promisedFilePaths)
{
    ASSERT(pasteboardName);
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return makeUnique<Pasteboard>(NSGeneralPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

#if ENABLE(DRAG_SUPPORT)
String Pasteboard::nameOfDragPasteboard()
{
    return NSPasteboardNameDrag;
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return makeUnique<Pasteboard>(NSDragPboard);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return makeUnique<Pasteboard>(dragData.pasteboardName(), dragData.fileNames());
}
#endif

void Pasteboard::clear()
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(Vector<String>(), m_pasteboardName);
}

void Pasteboard::write(const PasteboardWebContent& content)
{
    Vector<String> types;

    if (content.canSmartCopyOrDelete)
        types.append(WebSmartPastePboardType);
    if (content.dataInWebArchiveFormat) {
        types.append(WebArchivePboardType);
        types.append(kUTTypeWebArchive);
    }
    if (content.dataInRTFDFormat)
        types.append(String(legacyRTFDPasteboardType()));
    if (content.dataInRTFFormat)
        types.append(String(legacyRTFPasteboardType()));
    if (!content.dataInHTMLFormat.isNull())
        types.append(String(legacyHTMLPasteboardType()));
    if (!content.dataInStringFormat.isNull())
        types.append(String(legacyStringPasteboardType()));
    types.appendVector(content.clientTypes);
    types.append(PasteboardCustomData::cocoaType());

    m_changeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName);

    // FIXME: The following code should be refactored, such that it only requires a single call out to the client layer.
    // In WebKit2, this currently results in many unnecessary synchronous round-trip IPC messages.

    ASSERT(content.clientTypes.size() == content.clientData.size());
    for (size_t i = 0, size = content.clientTypes.size(); i < size; ++i)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.clientData[i].get(), content.clientTypes[i], m_pasteboardName);
    if (content.canSmartCopyOrDelete)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(nullptr, WebSmartPastePboardType, m_pasteboardName);
    if (content.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), WebArchivePboardType, m_pasteboardName);
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInWebArchiveFormat.get(), kUTTypeWebArchive, m_pasteboardName);
    }
    if (content.dataInRTFDFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFDFormat.get(), legacyRTFDPasteboardType(), m_pasteboardName);
    if (content.dataInRTFFormat)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(content.dataInRTFFormat.get(), legacyRTFPasteboardType(), m_pasteboardName);
    if (!content.dataInHTMLFormat.isNull())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInHTMLFormat, legacyHTMLPasteboardType(), m_pasteboardName);
    if (!content.dataInStringFormat.isNull())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(content.dataInStringFormat, legacyStringPasteboardType(), m_pasteboardName);

    PasteboardCustomData data;
    data.setOrigin(content.contentOrigin);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(data.createSharedBuffer().ptr(), PasteboardCustomData::cocoaType(), m_pasteboardName);

}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    Vector<String> types;
    types.append(legacyStringPasteboardType());
    if (smartReplaceOption == CanSmartReplace)
        types.append(WebSmartPastePboardType);

    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(text, legacyStringPasteboardType(), m_pasteboardName);
    if (smartReplaceOption == CanSmartReplace)
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(0, WebSmartPastePboardType, m_pasteboardName);
}

static long writeURLForTypes(const Vector<String>& types, const String& pasteboardName, const PasteboardURL& pasteboardURL)
{
    auto newChangeCount = platformStrategies()->pasteboardStrategy()->setTypes(types, pasteboardName);
    
    ASSERT(!pasteboardURL.url.isEmpty());
    
    NSURL *cocoaURL = pasteboardURL.url;
    NSString *userVisibleString = pasteboardURL.userVisibleForm;
    NSString *title = (NSString *)pasteboardURL.title;
    if (![title length]) {
        title = [[cocoaURL path] lastPathComponent];
        if (![title length])
            title = userVisibleString;
    }

    if (types.contains(WebURLsWithTitlesPboardType)) {
        PasteboardURL url = { pasteboardURL.url, String(title).stripWhiteSpace(), emptyString() };
        newChangeCount = platformStrategies()->pasteboardStrategy()->setURL(url, pasteboardName);
    }
    if (types.contains(String(legacyURLPasteboardType())))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType([cocoaURL absoluteString], legacyURLPasteboardType(), pasteboardName);
    if (types.contains(WebURLPboardType))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(userVisibleString, WebURLPboardType, pasteboardName);
    if (types.contains(WebURLNamePboardType))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(title, WebURLNamePboardType, pasteboardName);
    if (types.contains(String(legacyStringPasteboardType())))
        newChangeCount = platformStrategies()->pasteboardStrategy()->setStringForType(userVisibleString, legacyStringPasteboardType(), pasteboardName);

    return newChangeCount;
}
    
void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    m_changeCount = writeURLForTypes(writableTypesForURL(), m_pasteboardName, pasteboardURL);
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL& pasteboardURL)
{
    PasteboardURL url = { pasteboardURL.url, pasteboardURL.title.stripWhiteSpace(), emptyString() };
    m_changeCount = platformStrategies()->pasteboardStrategy()->setURL(url, m_pasteboardName);
}

void Pasteboard::write(const Color& color)
{
    Vector<String> types = { legacyColorPasteboardType() };
    platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setColor(color, m_pasteboardName);
}

static NSFileWrapper* fileWrapper(const PasteboardImage& pasteboardImage)
{
    NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:pasteboardImage.resourceData->createNSData().get()] autorelease];
    [wrapper setPreferredFilename:suggestedFilenameWithMIMEType(pasteboardImage.url.url, pasteboardImage.resourceMIMEType)];
    return wrapper;
}

static void writeFileWrapperAsRTFDAttachment(NSFileWrapper *wrapper, const String& pasteboardName, int64_t& newChangeCount)
{
    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment];
    [attachment release];

    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];
    if (!RTFDData)
        return;

    newChangeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(SharedBuffer::create(RTFDData).ptr(), legacyRTFDPasteboardType(), pasteboardName);
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    CFDataRef imageData = pasteboardImage.image->tiffRepresentation();
    if (!imageData)
        return;

    // FIXME: Why can we assert this? It doesn't seem like it's guaranteed.
    ASSERT(MIMETypeRegistry::isSupportedImageMIMEType(pasteboardImage.resourceMIMEType));

    auto types = writableTypesForImage();
    if (pasteboardImage.dataInWebArchiveFormat) {
        types.append(WebArchivePboardType);
        types.append(kUTTypeWebArchive);
    }

    m_changeCount = writeURLForTypes(types, m_pasteboardName, pasteboardImage.url);
    m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(SharedBuffer::create(imageData).ptr(), legacyTIFFPasteboardType(), m_pasteboardName);
    if (auto archiveData = pasteboardImage.dataInWebArchiveFormat) {
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), WebArchivePboardType, m_pasteboardName);
        m_changeCount = platformStrategies()->pasteboardStrategy()->setBufferForType(archiveData.get(), kUTTypeWebArchive, m_pasteboardName);
    }
    if (!pasteboardImage.dataInHTMLFormat.isEmpty())
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(pasteboardImage.dataInHTMLFormat, legacyHTMLPasteboardType(), m_pasteboardName);
    writeFileWrapperAsRTFDAttachment(fileWrapper(pasteboardImage), m_pasteboardName, m_changeCount);
}

bool Pasteboard::canSmartReplace()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
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

static String readStringAtPreferredItemIndex(const String& type, Optional<size_t> itemIndex, PasteboardStrategy& strategy, const String& pasteboardName)
{
    if (itemIndex)
        return strategy.readStringFromPasteboard(*itemIndex, type, pasteboardName);
    return strategy.stringForType(type, pasteboardName);
}

static RefPtr<SharedBuffer> readBufferAtPreferredItemIndex(const String& type, Optional<size_t> itemIndex, PasteboardStrategy& strategy, const String& pasteboardName)
{
    if (itemIndex)
        return strategy.readBufferFromPasteboard(*itemIndex, type, pasteboardName);
    return strategy.bufferForType(type, pasteboardName);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy allowURL, Optional<size_t> itemIndex)
{
    auto& strategy = *platformStrategies()->pasteboardStrategy();

    Vector<String> types;
    if (itemIndex) {
        if (auto itemInfo = strategy.informationForItemAtIndex(*itemIndex, m_pasteboardName, m_changeCount))
            types = itemInfo->platformTypesByFidelity;
    } else
        strategy.getTypes(types, m_pasteboardName);

    if (types.contains(String(NSPasteboardTypeString))) {
        text.text = readStringAtPreferredItemIndex(NSPasteboardTypeString, itemIndex, strategy, m_pasteboardName);
        text.isURL = false;
        return;
    }

    if (types.contains(String(legacyStringPasteboardType()))) {
        text.text = readStringAtPreferredItemIndex(legacyStringPasteboardType(), itemIndex, strategy, m_pasteboardName);
        text.isURL = false;
        return;
    }
    
    if (types.contains(String(legacyRTFDPasteboardType()))) {
        if (auto data = readBufferAtPreferredItemIndex(legacyRTFDPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(NSPasteboardTypeRTFD))) {
        if (auto data = readBufferAtPreferredItemIndex(NSPasteboardTypeRTFD, itemIndex, strategy, m_pasteboardName)) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTFD:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(legacyRTFPasteboardType()))) {
        if (auto data = readBufferAtPreferredItemIndex(legacyRTFPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(NSPasteboardTypeRTF))) {
        if (auto data = readBufferAtPreferredItemIndex(NSPasteboardTypeRTF, itemIndex, strategy, m_pasteboardName)) {
            if (auto attributedString = adoptNS([[NSAttributedString alloc] initWithRTF:data->createNSData().get() documentAttributes:nil])) {
                text.text = [attributedString string];
                text.isURL = false;
                return;
            }
        }
    }

    if (types.contains(String(legacyFilesPromisePasteboardType()))) {
        text.text = joinPathnames(m_promisedFilePaths);
        text.isURL = false;
        return;
    }

    if (types.contains(String(legacyFilenamesPasteboardType()))) {
        Vector<String> pathnames;
        strategy.getPathnamesForType(pathnames, legacyFilenamesPasteboardType(), m_pasteboardName);
        text.text = joinPathnames(pathnames);
        text.isURL = false;
        return;
    }

    // FIXME: The code above looks at the types vector first, but this just gets the string without checking. Why the difference?
    if (allowURL == PlainTextURLReadingPolicy::AllowURL) {
        text.text = readStringAtPreferredItemIndex(legacyURLPasteboardType(), itemIndex, strategy, m_pasteboardName);
        text.isURL = !text.text.isNull();
    }
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, Optional<size_t> itemIndex)
{
    auto& strategy = *platformStrategies()->pasteboardStrategy();

    Vector<String> types;
    if (itemIndex) {
        if (auto itemInfo = strategy.informationForItemAtIndex(*itemIndex, m_pasteboardName, m_changeCount))
            types = itemInfo->platformTypesByFidelity;
    } else
        strategy.getTypes(types, m_pasteboardName);

    reader.contentOrigin = readOrigin();

    if (types.contains(WebArchivePboardType)) {
        if (auto buffer = readBufferAtPreferredItemIndex(WebArchivePboardType, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (types.contains(String(kUTTypeWebArchive))) {
        if (auto buffer = readBufferAtPreferredItemIndex(kUTTypeWebArchive, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readWebArchive(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::AnyType && types.contains(String(legacyFilesPromisePasteboardType()))) {
        if (m_changeCount != changeCount() || reader.readFilePaths(m_promisedFilePaths))
            return;
    }

    if (policy == WebContentReadingPolicy::AnyType && types.contains(String(legacyFilenamesPasteboardType()))) {
        Vector<String> paths;
        strategy.getPathnamesForType(paths, legacyFilenamesPasteboardType(), m_pasteboardName);
        if (m_changeCount != changeCount() || reader.readFilePaths(paths))
            return;
    }

    if (types.contains(String(legacyHTMLPasteboardType()))) {
        String string = readStringAtPreferredItemIndex(legacyHTMLPasteboardType(), itemIndex, strategy, m_pasteboardName);
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readHTML(string)))
            return;
    }

    if (types.contains(String(NSPasteboardTypeHTML))) {
        String string = readStringAtPreferredItemIndex(NSPasteboardTypeHTML, itemIndex, strategy, m_pasteboardName);
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readHTML(string)))
            return;
    }

    if (types.contains(String(legacyRTFDPasteboardType()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyRTFDPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypeRTFD))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypeRTFD, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readRTFD(*buffer))
                return;
        }
    }

    if (types.contains(String(legacyRTFPasteboardType()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyRTFPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypeRTF))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypeRTF, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readRTF(*buffer))
                return;
        }
    }

    if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
        return;

    if (types.contains(String(legacyTIFFPasteboardType()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyTIFFPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "image/tiff"_s))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypeTIFF))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypeTIFF, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "image/tiff"_s))
                return;
        }
    }

    if (types.contains(String(legacyPDFPasteboardType()))) {
        if (auto buffer = readBufferAtPreferredItemIndex(legacyPDFPasteboardType(), itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "application/pdf"_s))
                return;
        }
    }

    if (types.contains(String(NSPasteboardTypePDF))) {
        if (auto buffer = readBufferAtPreferredItemIndex(NSPasteboardTypePDF, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "application/pdf"_s))
                return;
        }
    }

    if (types.contains(String(kUTTypePNG))) {
        if (auto buffer = readBufferAtPreferredItemIndex(kUTTypePNG, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "image/png"_s))
                return;
        }
    }

    if (types.contains(String(kUTTypeJPEG))) {
        if (auto buffer = readBufferAtPreferredItemIndex(kUTTypeJPEG, itemIndex, strategy, m_pasteboardName)) {
            if (m_changeCount != changeCount() || reader.readImage(buffer.releaseNonNull(), "image/jpeg"_s))
                return;
        }
    }

    if (types.contains(String(legacyURLPasteboardType()))) {
        URL url = strategy.url(m_pasteboardName);
        String title = readStringAtPreferredItemIndex(WebURLNamePboardType, itemIndex, strategy, m_pasteboardName);
        if (m_changeCount != changeCount() || (!url.isNull() && reader.readURL(url, title)))
            return;
    }

    if (types.contains(String(legacyStringPasteboardType()))) {
        String string = readStringAtPreferredItemIndex(legacyStringPasteboardType(), itemIndex, strategy, m_pasteboardName);
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readPlainText(string)))
            return;
    }

    if (types.contains(String(kUTTypeUTF8PlainText))) {
        String string = strategy.stringForType(kUTTypeUTF8PlainText, m_pasteboardName);
        if (m_changeCount != changeCount() || (!string.isNull() && reader.readPlainText(string)))
            return;
    }
}

bool Pasteboard::hasData()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    return !types.isEmpty();
}

static String cocoaTypeFromHTMLClipboardType(const String& type)
{
    if (NSString *platformType = PlatformPasteboard::platformPasteboardTypeForSafeTypeForDOMToReadAndWrite(type)) {
        if (platformType.length)
            return platformType;
    }

    // Blocklist types that might contain subframe information.
    if (type == "text/rtf" || type == "public.rtf" || type == "com.apple.traditional-mac-plain-text")
        return String();

    auto utiType = UTIFromMIMEType(type);
    if (!utiType.isEmpty()) {
        if (auto pbType = adoptCF(UTTypeCopyPreferredTagWithClass(utiType.createCFString().get(), kUTTagClassNSPboardType)))
            return pbType.get();
    }

    // No mapping, just pass the whole string though
    return type;
}

void Pasteboard::clear(const String& type)
{
    String cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (cocoaType.isEmpty())
        return;
    m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(emptyString(), cocoaType, m_pasteboardName);
}

Vector<String> Pasteboard::readPlatformValuesAsStrings(const String& domType, int64_t changeCount, const String& pasteboardName)
{
    auto& strategy = *platformStrategies()->pasteboardStrategy();
    auto cocoaType = cocoaTypeFromHTMLClipboardType(domType);
    if (cocoaType.isEmpty())
        return { };

    auto values = strategy.allStringsForType(cocoaType, pasteboardName);
    if (cocoaType == String(legacyStringPasteboardType())) {
        values = values.map([&] (auto& value) -> String {
            return [value precomposedStringWithCanonicalMapping];
        });
    }

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (changeCount != platformStrategies()->pasteboardStrategy()->changeCount(pasteboardName))
        return { };

    return values;
}

static String utiTypeFromCocoaType(const String& type)
{
    if (RetainPtr<CFStringRef> utiType = adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, type.createCFString().get(), 0))) {
        if (RetainPtr<CFStringRef> mimeType = adoptCF(UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType)))
            return String(mimeType.get());
    }
    return String();
}

void Pasteboard::addHTMLClipboardTypesForCocoaType(ListHashSet<String>& resultTypes, const String& cocoaType)
{
    if (cocoaType == "NeXT plain ascii pasteboard type")
        return; // Skip this ancient type that gets auto-supplied by some system conversion.

    // UTI may not do these right, so make sure we get the right, predictable result
    if (cocoaType == String(legacyStringPasteboardType()) || cocoaType == String(NSPasteboardTypeString)) {
        resultTypes.add("text/plain"_s);
        return;
    }
    if (cocoaType == String(legacyURLPasteboardType())) {
        resultTypes.add("text/uri-list"_s);
        return;
    }
    if (cocoaType == String(legacyFilenamesPasteboardType()) || Pasteboard::shouldTreatCocoaTypeAsFile(cocoaType))
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

    if (cocoaType == String(legacyURLPasteboardType()) || cocoaType == String(kUTTypeFileURL)) {
        NSURL *url = [NSURL URLWithString:cocoaData];
        if ([url isFileURL])
            return;

        Vector<String> types;
        types.append(cocoaType);
        platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName);
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName);

        return;
    }

    if (!cocoaType.isEmpty()) {
        // everything else we know of goes on the pboard as a string
        Vector<String> types;
        types.append(cocoaType);
        platformStrategies()->pasteboardStrategy()->addTypes(types, m_pasteboardName);
        m_changeCount = platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName);
    }
}

Vector<String> Pasteboard::readFilePaths()
{
    auto& strategy = *platformStrategies()->pasteboardStrategy();

    Vector<String> types;
    strategy.getTypes(types, m_pasteboardName);

    if (types.contains(String(legacyFilesPromisePasteboardType())))
        return m_promisedFilePaths;

    if (types.contains(String(legacyFilenamesPasteboardType()))) {
        Vector<String> filePaths;
        strategy.getPathnamesForType(filePaths, legacyFilenamesPasteboardType(), m_pasteboardName);
        return filePaths;
    }

    return { };
}

#if ENABLE(DRAG_SUPPORT)
static void flipImageSpec(CoreDragImageSpec* imageSpec)
{
    unsigned char* tempRow = (unsigned char*)fastMalloc(imageSpec->bytesPerRow);
    int planes = imageSpec->isPlanar ? imageSpec->samplesPerPixel : 1;

    for (int p = 0; p < planes; ++p) {
        unsigned char* topRow = const_cast<unsigned char*>(imageSpec->data[p]);
        unsigned char* botRow = topRow + (imageSpec->pixelsHigh - 1) * imageSpec->bytesPerRow;
        for (int i = 0; i < imageSpec->pixelsHigh / 2; ++i, topRow += imageSpec->bytesPerRow, botRow -= imageSpec->bytesPerRow) {
            bcopy(topRow, tempRow, imageSpec->bytesPerRow);
            bcopy(botRow, topRow, imageSpec->bytesPerRow);
            bcopy(tempRow, botRow, imageSpec->bytesPerRow);
        }
    }

    fastFree(tempRow);
}

static void setDragImageImpl(NSImage *image, NSPoint offset)
{
    bool flipImage;
    NSSize imageSize = image.size;
    CGRect imageRect = CGRectMake(0, 0, imageSize.width, imageSize.height);
    NSImageRep *imageRep = [image bestRepresentationForRect:NSRectFromCGRect(imageRect) context:nil hints:nil];
    RetainPtr<NSBitmapImageRep> bitmapImage;
    if (!imageRep || ![imageRep isKindOfClass:[NSBitmapImageRep class]] || !NSEqualSizes(imageRep.size, imageSize)) {
        [image lockFocus];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        bitmapImage = adoptNS([[NSBitmapImageRep alloc] initWithFocusedViewRect:*(NSRect*)&imageRect]);
        ALLOW_DEPRECATED_DECLARATIONS_END
        [image unlockFocus];
        
        // we may have to flip the bits we just read if the image was flipped since it means the cache was also
        // and CoreDragSetImage can't take a transform for rendering.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        flipImage = image.isFlipped;
        ALLOW_DEPRECATED_DECLARATIONS_END
    } else {
        flipImage = false;
        bitmapImage = (NSBitmapImageRep *)imageRep;
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
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return;

    // Dashboard wants to be able to set the drag image during dragging, but Cocoa does not allow this.
    // Instead we must drop down to the CoreGraphics API.
    setDragImageImpl(image.get().get(), location);

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

}

#endif // PLATFORM(MAC)
