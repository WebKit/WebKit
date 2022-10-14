/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Pasteboard.h"

#import "LegacyNSPasteboardTypes.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "SharedBuffer.h"
#import <ImageIO/ImageIO.h>
#import <wtf/ListHashSet.h>
#import <wtf/text/StringHash.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

#if PLATFORM(MAC)
static NSBitmapImageFileType bitmapPNGFileType()
{
    return NSBitmapImageFileTypePNG;
}
#endif // PLATFORM(MAC)

enum class ImageType {
    Invalid = 0,
    TIFF,
    PNG,
    JPEG,
    GIF,
};

static ImageType cocoaTypeToImageType(const String& cocoaType)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if PLATFORM(MAC)
    if (cocoaType == String(legacyTIFFPasteboardType()))
        return ImageType::TIFF;
#endif
    if (cocoaType == String(kUTTypeTIFF))
        return ImageType::TIFF;
#if PLATFORM(MAC)
    if (cocoaType == String(legacyPNGPasteboardType())) // NSPNGPboardType
        return ImageType::PNG;
#endif
    if (cocoaType == String(kUTTypePNG))
        return ImageType::PNG;
    if (cocoaType == String(kUTTypeJPEG))
        return ImageType::JPEG;
    if (cocoaType == String(kUTTypeGIF))
        return ImageType::GIF;
ALLOW_DEPRECATED_DECLARATIONS_END
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

static ASCIILiteral imageTypeToFakeFilename(ImageType type)
{
    switch (type) {
    case ImageType::Invalid:
        ASSERT_NOT_REACHED();
        return { };
    case ImageType::TIFF:
#if PLATFORM(MAC)
        return "image.png"_s; // For Web compatibility, we pretend to have PNG instead.
#else
        ASSERT_NOT_REACHED();
        return { };
#endif
    case ImageType::PNG:
        return "image.png"_s;
    case ImageType::JPEG:
        return "image.jpeg"_s;
    case ImageType::GIF:
        return "image.gif"_s;
    }
}

bool Pasteboard::shouldTreatCocoaTypeAsFile(const String& cocoaType)
{
    return cocoaTypeToImageType(cocoaType) != ImageType::Invalid;
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    bool mayContainFilePaths = platformStrategies()->pasteboardStrategy()->getNumberOfFiles(m_pasteboardName, context());

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
        auto items = allPasteboardItemInfo();
        if (!items)
            return FileContentState::NoFileOrImageData;

        mayContainFilePaths = items->size() != 1 || notFound != items->findIf([] (auto& item) {
            return item.canBeTreatedAsAttachmentOrFile() || item.isNonTextType || item.containsFileURLAndFileUploadContent;
        });
    }
#endif

    if (!mayContainFilePaths) {
        Vector<String> cocoaTypes;
        platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName, context());
        if (cocoaTypes.findIf([](const String& cocoaType) { return shouldTreatCocoaTypeAsFile(cocoaType); }) == notFound)
            return FileContentState::NoFileOrImageData;

        auto indexOfURL = cocoaTypes.findIf([](auto& cocoaType) {
#if PLATFORM(MAC)
            if (cocoaType == String(legacyURLPasteboardType()))
                return true;
#endif
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            return cocoaType == String(kUTTypeURL);
ALLOW_DEPRECATED_DECLARATIONS_END
        });
        mayContainFilePaths = indexOfURL != notFound && !platformStrategies()->pasteboardStrategy()->containsStringSafeForDOMToReadForType(cocoaTypes[indexOfURL], m_pasteboardName, context());
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
        return FileContentState::NoFileOrImageData;

    // Even when there's only image data in the pasteboard and no file representations, we still run the risk of exposing file paths
    // to the page if the app has written image data to the pasteboard with a corresponding file path as plain text. An example of
    // this is copying an image with a local `src` in Safari. To mitigate this, we additionally require that the app has not also
    // written URLs to the pasteboard, as this would suggest that the plain text data might contain file paths.
    return mayContainFilePaths ? FileContentState::MayContainFilePaths : FileContentState::InMemoryImage;
}

Vector<String> Pasteboard::typesSafeForBindings(const String& origin)
{
    Vector<String> types = platformStrategies()->pasteboardStrategy()->typesSafeForDOMToReadAndWrite(m_pasteboardName, origin, context());

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
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
static Ref<SharedBuffer> convertTIFFToPNG(FragmentedSharedBuffer& tiffBuffer)
{
    auto image = adoptNS([[NSBitmapImageRep alloc] initWithData: tiffBuffer.makeContiguous()->createNSData().get()]);
    NSData *pngData = [image representationUsingType:bitmapPNGFileType() properties:@{ }];
    return SharedBuffer::create(pngData);
}
#endif

void Pasteboard::read(PasteboardFileReader& reader, std::optional<size_t> itemIndex)
{
    if (!itemIndex) {
        auto filenames = readFilePaths();
        if (!filenames.isEmpty()) {
            for (auto& filename : filenames)
                reader.readFilename(filename);
            return;
        }
    }

    auto readBufferAtIndex = [&](const PasteboardItemInfo& info, size_t itemIndex) {
        for (auto cocoaType : info.platformTypesByFidelity) {
            auto imageType = cocoaTypeToImageType(cocoaType);
            auto* mimeType = imageTypeToMIMEType(imageType);
            if (!mimeType || !reader.shouldReadBuffer(String::fromLatin1(mimeType)))
                continue;
            auto buffer = readBuffer(itemIndex, cocoaType);
#if PLATFORM(MAC)
            if (buffer && imageType == ImageType::TIFF)
                buffer = convertTIFFToPNG(buffer.releaseNonNull());
#endif
            if (buffer) {
                reader.readBuffer(imageTypeToFakeFilename(imageType), String::fromLatin1(mimeType), buffer.releaseNonNull());
                break;
            }
        }
    };

    if (itemIndex) {
        if (auto info = pasteboardItemInfo(*itemIndex))
            readBufferAtIndex(*info, *itemIndex);
        return;
    }

    if (auto allInfo = allPasteboardItemInfo()) {
        for (size_t itemIndex = 0; itemIndex < allInfo->size(); ++itemIndex)
            readBufferAtIndex(allInfo->at(itemIndex), itemIndex);
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
    return readCustomData().readStringInCustomData(type);
}

String Pasteboard::readOrigin()
{
    return readCustomData().origin();
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

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->writeCustomData(data, name(), context());
}

int64_t Pasteboard::changeCount() const
{
    return platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context());
}

Vector<String> Pasteboard::readTypesWithSecurityCheck()
{
    Vector<String> cocoaTypes;
    platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName, context());

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
        return { };

    return cocoaTypes;
}

RefPtr<SharedBuffer> Pasteboard::readBufferForTypeWithSecurityCheck(const String& type)
{
    auto buffer = platformStrategies()->pasteboardStrategy()->bufferForType(type, m_pasteboardName, context());

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName, context()))
        return nullptr;

    return buffer;
}

}
