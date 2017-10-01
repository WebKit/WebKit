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

#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SharedBuffer.h"
#include <ImageIO/ImageIO.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(IOS)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

// Making this non-inline so that WebKit 2's decoding doesn't have to include SharedBuffer.h.
PasteboardWebContent::PasteboardWebContent() = default;
PasteboardWebContent::~PasteboardWebContent() = default;

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
    if (cocoaType == String(NSTIFFPboardType))
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

static const char* imageTypeToMIMEType(ImageType type)
{
    switch (type) {
    case ImageType::Invalid:
        return nullptr;
    case ImageType::TIFF:
#if PLATFORM(MAC)
        return "image/png"; // For Web compatibility, we pretend to have PNG instead.
#else
        return nullptr; // Don't support TIFF on iOS for now.
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
        ASSERT_NOT_REACHED();
        return nullptr;
    case ImageType::PNG:
        return "image.png";
    case ImageType::JPEG:
        return "image.jpeg";
    case ImageType::GIF:
        return "image.gif";
    }
}

static ImageType mimeTypeToImageType(const String& mimeType)
{
#if PLATFORM(MAC)
    if (mimeType == "image/tiff")
        return ImageType::TIFF;
#endif
    if (mimeType == "image/png")
        return ImageType::PNG;
    if (mimeType == "image/jpeg")
        return ImageType::JPEG;
    if (mimeType == "image/gif")
        return ImageType::GIF;
    return ImageType::Invalid;
}

bool Pasteboard::shouldTreatCocoaTypeAsFile(const String& cocoaType)
{
    return cocoaTypeToImageType(cocoaType) != ImageType::Invalid;
}

Vector<String> Pasteboard::typesTreatedAsFiles()
{
    Vector<String> cocoaTypes;
    platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return { };

    ListHashSet<String> result;
    for (auto& cocoaType : cocoaTypes) {
        if (auto* mimeType = imageTypeToMIMEType(cocoaTypeToImageType(cocoaType)))
            result.add(mimeType);
    }

    Vector<String> types;
    copyToVector(result, types);
    return types;
}

bool Pasteboard::containsFiles()
{
    if (!platformStrategies()->pasteboardStrategy()->getNumberOfFiles(m_pasteboardName)) {
        Vector<String> cocoaTypes;
        platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName);
        if (cocoaTypes.findMatching([](const String& cocoaType) { return shouldTreatCocoaTypeAsFile(cocoaType); }) == notFound)
            return false;
    }

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return false;

    return true;
}

Vector<String> Pasteboard::typesSafeForBindings()
{
    Vector<String> types = platformStrategies()->pasteboardStrategy()->typesSafeForDOMToReadAndWrite(m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return { };

    return types;
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return { };

    ListHashSet<String> result;
    for (auto& cocoaType : types)
        addHTMLClipboardTypesForCocoaType(result, cocoaType, m_pasteboardName);

    copyToVector(result, types);
    return types;
}

void Pasteboard::read(PasteboardFileReader& reader)
{
    auto imageType = mimeTypeToImageType(reader.type);
    if (imageType == ImageType::Invalid)
        return;

    String cocoaTypeToRead;
    String tiffCocoaTypeInPasteboard;
    Vector<String> cocoaTypes;
    platformStrategies()->pasteboardStrategy()->getTypes(cocoaTypes, m_pasteboardName);
    for (auto& cocoaType : cocoaTypes) {
        auto imageTypeInPasteboard = cocoaTypeToImageType(cocoaType);
        if (imageTypeInPasteboard == imageType) {
            cocoaTypeToRead = cocoaType;
            break;
        }
        if (tiffCocoaTypeInPasteboard.isNull() && imageTypeInPasteboard == ImageType::TIFF)
            tiffCocoaTypeInPasteboard = cocoaType;
    }

    bool tiffWasTreatedAsPNGForWebCompatibility = imageType == ImageType::PNG && cocoaTypeToRead.isNull() && !tiffCocoaTypeInPasteboard.isNull();
    if (tiffWasTreatedAsPNGForWebCompatibility)
        cocoaTypeToRead = tiffCocoaTypeInPasteboard;

    RefPtr<SharedBuffer> buffer = platformStrategies()->pasteboardStrategy()->bufferForType(cocoaTypeToRead, m_pasteboardName);

    // Enforce changeCount ourselves for security. We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (!buffer || m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return;

#if PLATFORM(MAC)
    if (tiffWasTreatedAsPNGForWebCompatibility) {
        auto tiffData = buffer->createNSData();
        auto image = adoptNS([[NSBitmapImageRep alloc] initWithData: tiffData.get()]);
        NSData *pngData = [image representationUsingType:NSPNGFileType properties:@{ }];
        reader.read(imageTypeToFakeFilename(imageType), SharedBuffer::create(pngData));
        return;
    }
#endif
    reader.read(imageTypeToFakeFilename(imageType), buffer.releaseNonNull());
}

String Pasteboard::readString(const String& type)
{
    return readPlatformValueAsString(type, m_changeCount, m_pasteboardName);
}

String Pasteboard::readStringInCustomData(const String& type)
{
    auto buffer = platformStrategies()->pasteboardStrategy()->bufferForType(customWebKitPasteboardDataType, m_pasteboardName);
    if (!buffer)
        return { };

    NSString *customDataValue = customDataFromSharedBuffer(*buffer).sameOriginCustomData.get(type);
    if (!customDataValue.length)
        return { };
    return customDataValue;
}

void Pasteboard::writeCustomData(const PasteboardCustomData& data)
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->writeCustomData(data, m_pasteboardName);
}

long Pasteboard::changeCount() const
{
    return platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName);
}

}

