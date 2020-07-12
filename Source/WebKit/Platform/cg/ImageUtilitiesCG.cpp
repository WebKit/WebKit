/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "ImageUtilities.h"

#include "Logging.h"
#include <ImageIO/ImageIO.h>
#include <WebCore/MIMETypeRegistry.h>
#include <wtf/FileSystem.h>
#include <wtf/text/CString.h>

namespace WebKit {

static String transcodeImage(const String& path, const String& destinationUTI, const String& destinationExtension)
{
    auto sourceURL = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path.createCFString().get(), kCFURLPOSIXPathStyle, false));
    auto source = adoptCF(CGImageSourceCreateWithURL(sourceURL.get(), nullptr));
    if (!source)
        return nullString();

    auto sourceUTI = String(CGImageSourceGetType(source.get()));
    if (sourceUTI == destinationUTI)
        return nullString();

    // It is important to add the appropriate file extension to the temporary file path.
    // The File object depends solely on the extension to know the MIME type of the file.
    auto suffix = makeString('.', destinationExtension);

    FileSystem::PlatformFileHandle destinationFileHandle;
    String destinationPath = FileSystem::openTemporaryFile("tempImage"_s, destinationFileHandle, suffix);
    if (destinationFileHandle == FileSystem::invalidPlatformFileHandle) {
        RELEASE_LOG_ERROR(Images, "transcodeImage: Destination image could not be created: %s %s\n", path.utf8().data(), destinationUTI.utf8().data());
        return nullString();
    }

    CGDataConsumerCallbacks callbacks = {
        [](void* info, const void* buffer, size_t count) -> size_t {
            auto handle = *static_cast<FileSystem::PlatformFileHandle*>(info);
            return FileSystem::writeToFile(handle, static_cast<const char*>(buffer), count);
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(&destinationFileHandle, &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI.createCFString().get(), 1, nullptr));

    CGImageDestinationAddImageFromSource(destination.get(), source.get(), 0, nullptr);

    if (!CGImageDestinationFinalize(destination.get())) {
        RELEASE_LOG_ERROR(Images, "transcodeImage: Image transcoding fails: %s %s\n", path.utf8().data(), destinationUTI.utf8().data());
        FileSystem::closeFile(destinationFileHandle);
        FileSystem::deleteFile(destinationPath);
        return nullString();
    }

    FileSystem::closeFile(destinationFileHandle);
    return destinationPath;
}

Vector<String> findImagesForTranscoding(const Vector<String>& paths, const Vector<String>& allowedMIMETypes)
{
    Vector<String> transcodingPaths;
    transcodingPaths.reserveInitialCapacity(paths.size());

    bool needsTranscoding = false;

    for (auto& path : paths) {
        // Append a path of the image which needs transcoding. Otherwise append a null string.
        if (!allowedMIMETypes.contains(WebCore::MIMETypeRegistry::mimeTypeForPath(path))) {
            transcodingPaths.uncheckedAppend(path);
            needsTranscoding = true;
        } else
            transcodingPaths.uncheckedAppend(nullString());
    }

    // If none of the files needs image transcoding, return an empty Vector.
    return needsTranscoding ? transcodingPaths : Vector<String>();
}

Vector<String> transcodeImages(const Vector<String>& paths, const String& destinationUTI, const String& destinationExtension)
{
    ASSERT(!destinationUTI.isNull());
    ASSERT(!destinationExtension.isNull());
    
    Vector<String> transcodedPaths;
    transcodedPaths.reserveInitialCapacity(paths.size());

    for (auto& path : paths) {
        // Append the transcoded path if the image needs transcoding. Otherwise append a null string.
        if (!path.isNull())
            transcodedPaths.uncheckedAppend(transcodeImage(path, destinationUTI, destinationExtension));
        else
            transcodedPaths.uncheckedAppend(nullString());
    }

    return transcodedPaths;
}

} // namespace WebKit
