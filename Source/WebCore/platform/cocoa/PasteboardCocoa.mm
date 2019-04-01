/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Pasteboard.h"

#include "LegacyNSPasteboardTypes.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SharedBuffer.h"
#include <ImageIO/ImageIO.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

#if PLATFORM(MAC)
static NSBitmapImageFileType bitmapPNGFileType()
{
    return NSBitmapImageFileTypePNG;
}
#endif // PLATFORM(MAC)

// Making this non-inline so that WebKit 2's decoding doesn't have to include SharedBuffer.h.
PasteboardWebContent::PasteboardWebContent() = default;
PasteboardWebContent::~PasteboardWebContent() = default;

const char* PasteboardCustomData::cocoaType()
{
    return "com.apple.WebKit.custom-pasteboard-data";
}

enum class ImageType {
    Invalid = 0,
    TIFF,
    PNG,
    JPEG,
    GIF,
};

static ImageType cocoaTypeToImageType(const String& cocoaType)
{
#if PLATFORM(MAC)
    if (cocoaType == String(legacyTIFFPasteboardType()))
        return ImageType::TIFF;
#endif
    if (cocoaType == String(kUTTypeTIFF))
        return ImageType::TIFF;
#if PLATFORM(MAC)
    if (cocoaType == "Apple PNG pasteboard type") // NSPNGPboardType
        return ImageType::PNG;
#endif
    if (cocoaType == String(kUTTypePNG))
        return ImageType::PNG;
    if (cocoaType == String(kUTTypeJPEG))
        return ImageType::JPEG;
    if (cocoaType == String(kUTTypeGIF))
        return ImageType::GIF;
    return ImageType::Invalid;
}

// String literals returned by this function must be defined exactly once
// since read(PasteboardFileReader&) uses HashMap<const char*> to check uniqueness.
static const char* imageTypeToMIMEType(ImageType type)
{
    switch (type) {
    case ImageType::Invalid:
        return nullptr;
    case ImageType::TIFF:
#if PLATFORM(MAC)
        return "image/png"; // For Web compatibility, we pretend to have PNG instead.
#else
        return nullptr; // Don't support pasting TIFF on iOS for now.
#endif
    case ImageType::PNG:
        return "image/png";
    case ImageType::JPEG:
        return "image/jpeg";
    case ImageType::GIF:
        return "image/gif";
    }
}

static const char* imageTypeToFakeFilename(ImageType type)
{
    switch (type) {
    case ImageType::Invalid:
        ASSERT_NOT_REACHED();
        return nullptr;
    case ImageType::TIFF:
#if PLATFORM(MAC)
        return "image.png"; // For Web compatibility, we pretend to have PNG instead.
#else
        ASSERT_NOT_REACHED();
        return nullptr;
#endif
    case ImageType::PNG:
        return "image.png";
    case ImageType::JPEG:
        return "image.jpeg";
    case ImageType::GIF:
        return "image.gif";
    }
}

bool Pasteboard::shouldTreatCocoaTypeAsFile(const String& cocoaType)
{
    return cocoaTypeToImageType(cocoaType) != ImageType::Invalid;
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    bool mayContainFilePaths = platformStrategies()->pasteboardStrategy()->getNumberOfFiles(m_pasteboardName);

#if PLATFORM(IOS_FAMILY)
    if (mayContainFilePaths) {
        // On iOS, files are not written to the pasteboard using file URLs, so we need a heuristic to determine
        // whether or not the pasteboard contains items that represent files. An example of when this gets tricky
        // is differentiating between cases where the user is dragging a plain text file, versus selected text.
        // Also, the presence of any other declared non-text data in the same item indicates that the content
        // being dropped can take on another non-text format, which could be a file.
        // If the item can't be treated as an attachment, it's very likely that the content being dropped is just
        // an inline piece of text, with no files in the pasteboard (and therefore, no risk of leaking file paths
        // to web content). In cases such as these, we should not suppress DataTransfer access.
        auto items = platformStrategies()->pasteboardStrategy()->allPasteboardItemInfo(m_pasteboardName);
        mayContainFilePaths = items.size() != 1 || notFound != items.findMatching([] (auto& item) {
            return item.canBeTreatedAsAttachmentOrFile() || item.isNonTextType || item.containsFileURLAndFileUploadContent;
        });
    }
#endif

    if (!mayContainFilePaths) {
        Vector<String> cocoaTypes;
        platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName);
        if (cocoaTypes.findMatching([](const String& cocoaType) { return shouldTreatCocoaTypeAsFile(cocoaType); }) == notFound)
            return FileContentState::NoFileOrImageData;

        bool containsURL = notFound != cocoaTypes.findMatching([] (auto& cocoaType) {
#if PLATFORM(MAC)
            if (cocoaType == String(legacyURLPasteboardType()))
                return true;
#endif
            return cocoaType == String(kUTTypeURL);
        });
        mayContainFilePaths = containsURL && !Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(readString("text/uri-list"_s));
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return FileContentState::NoFileOrImageData;

    // Even when there's only image data in the pasteboard and no file representations, we still run the risk of exposing file paths
    // to the page if the app has written image data to the pasteboard with a corresponding file path as plain text. An example of
    // this is copying an image with a local `src` in Safari. To mitigate this, we additionally require that the app has not also
    // written URLs to the pasteboard, as this would suggest that the plain text data might contain file paths.
    return mayContainFilePaths ? FileContentState::MayContainFilePaths : FileContentState::InMemoryImage;
}

Vector<String> Pasteboard::typesSafeForBindings(const String& origin)
{
    Vector<String> types = platformStrategies()->pasteboardStrategy()->typesSafeForDOMToReadAndWrite(m_pasteboardName, origin);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return { };

    return types;
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    auto cocoaTypes = readTypesWithSecurityCheck();
    if (cocoaTypes.isEmpty())
        return cocoaTypes;

    ListHashSet<String> result;
    for (auto& cocoaType : cocoaTypes)
        addHTMLClipboardTypesForCocoaType(result, cocoaType);

    return copyToVector(result);
}

#if PLATFORM(MAC)
static Ref<SharedBuffer> convertTIFFToPNG(SharedBuffer& tiffBuffer)
{
    auto image = adoptNS([[NSBitmapImageRep alloc] initWithData: tiffBuffer.createNSData().get()]);
    NSData *pngData = [image representationUsingType:bitmapPNGFileType() properties:@{ }];
    return SharedBuffer::create(pngData);
}
#endif

void Pasteboard::read(PasteboardFileReader& reader)
{
    auto filenames = readFilePaths();
    if (!filenames.isEmpty()) {
        for (auto& filename : filenames)
            reader.readFilename(filename);
        return;
    }

    auto cocoaTypes = readTypesWithSecurityCheck();
    HashSet<const char*> existingMIMEs;
    for (auto& cocoaType : cocoaTypes) {
        auto imageType = cocoaTypeToImageType(cocoaType);
        const char* mimeType = imageTypeToMIMEType(imageType);
        if (!mimeType)
            continue;
        if (existingMIMEs.contains(mimeType))
            continue;
        auto buffer = readBufferForTypeWithSecurityCheck(cocoaType);
#if PLATFORM(MAC)
        if (buffer && imageType == ImageType::TIFF)
            buffer = convertTIFFToPNG(buffer.releaseNonNull());
#endif
        if (!buffer)
            continue;
        existingMIMEs.add(mimeType);
        reader.readBuffer(imageTypeToFakeFilename(imageType), mimeType, buffer.releaseNonNull());
    }
}

Vector<String> Pasteboard::readAllStrings(const String& type)
{
    return readPlatformValuesAsStrings(type, m_changeCount, m_pasteboardName);
}

String Pasteboard::readString(const String& type)
{
    auto values = readPlatformValuesAsStrings(type, m_changeCount, m_pasteboardName);
    return values.isEmpty() ? String() : values.first();
}

String Pasteboard::readStringInCustomData(const String& type)
{
    return readCustomData().sameOriginCustomData.get(type);
}

String Pasteboard::readOrigin()
{
    return readCustomData().origin;
}

const PasteboardCustomData& Pasteboard::readCustomData()
{
    if (m_customDataCache)
        return *m_customDataCache;

    if (auto buffer = readBufferForTypeWithSecurityCheck(PasteboardCustomData::cocoaType()))
        m_customDataCache = PasteboardCustomData::fromSharedBuffer(*buffer);
    else
        m_customDataCache = PasteboardCustomData { };
    return *m_customDataCache; 
}

void Pasteboard::writeCustomData(const PasteboardCustomData& data)
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->writeCustomData(data, m_pasteboardName);
}

long Pasteboard::changeCount() const
{
    return platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName);
}

Vector<String> Pasteboard::readTypesWithSecurityCheck()
{
    Vector<String> cocoaTypes;
    platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return { };

    return cocoaTypes;
}

RefPtr<SharedBuffer> Pasteboard::readBufferForTypeWithSecurityCheck(const String& type)
{
    auto buffer = platformStrategies()->pasteboardStrategy()->bufferForType(type, m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return nullptr;

    return buffer;
}

}
