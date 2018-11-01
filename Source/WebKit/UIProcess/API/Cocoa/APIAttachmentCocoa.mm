/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "APIAttachment.h"

#import "PageClient.h"
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/SharedBuffer.h>
#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#else
#import <CoreServices/CoreServices.h>
#endif
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>

#define CAN_SECURELY_ARCHIVE_FILE_WRAPPER (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000)

namespace API {

static WTF::String mimeTypeInferredFromFileExtension(const API::Attachment& attachment)
{
    if (NSString *fileExtension = [(NSString *)attachment.fileName() pathExtension])
        return WebCore::MIMETypeRegistry::getMIMETypeForExtension(fileExtension);

    return { };
}

static BOOL isDeclaredOrDynamicTypeIdentifier(NSString *type)
{
    return UTTypeIsDeclared((__bridge CFStringRef)type) || UTTypeIsDynamic((__bridge CFStringRef)type);
}

WTF::String Attachment::mimeType() const
{
    NSString *contentType = m_contentType.isEmpty() ? mimeTypeInferredFromFileExtension(*this) : m_contentType;
    if (!isDeclaredOrDynamicTypeIdentifier(contentType))
        return contentType;

    return adoptCF(UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)contentType, kUTTagClassMIMEType)).get();
}

WTF::String Attachment::utiType() const
{
    NSString *contentType = m_contentType.isEmpty() ? mimeTypeInferredFromFileExtension(*this) : m_contentType;
    if (isDeclaredOrDynamicTypeIdentifier(contentType))
        return contentType;

    return adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (__bridge CFStringRef)contentType, nullptr)).get();
}

WTF::String Attachment::fileName() const
{
    if ([m_fileWrapper filename].length)
        return [m_fileWrapper filename];

    return [m_fileWrapper preferredFilename];
}

void Attachment::setFileWrapperAndUpdateContentType(NSFileWrapper *fileWrapper, NSString *contentType)
{
    if (!contentType.length) {
        if (fileWrapper.directory)
            contentType = (NSString *)kUTTypeDirectory;
        else if (fileWrapper.regularFile) {
            if (NSString *pathExtension = (fileWrapper.filename.length ? fileWrapper.filename : fileWrapper.preferredFilename).pathExtension)
                contentType = WebCore::MIMETypeRegistry::getMIMETypeForExtension(pathExtension);
            if (!contentType.length)
                contentType = (NSString *)kUTTypeData;
        }
    }

    setContentType(contentType);
    setFileWrapper(fileWrapper);
}

std::optional<uint64_t> Attachment::fileSizeForDisplay() const
{
    if (![m_fileWrapper isRegularFile]) {
        // FIXME: We should display a size estimate for directory-type file wrappers.
        return std::nullopt;
    }

    if (auto fileSize = [[m_fileWrapper fileAttributes][NSFileSize] unsignedLongLongValue])
        return fileSize;

    return [m_fileWrapper regularFileContents].length;
}

RefPtr<WebCore::SharedBuffer> Attachment::enclosingImageData() const
{
    if (!m_hasEnclosingImage)
        return nullptr;

    if (![m_fileWrapper isRegularFile])
        return nullptr;

    NSData *data = [m_fileWrapper regularFileContents];
    if (!data)
        return nullptr;

    return WebCore::SharedBuffer::create(data);
}

bool Attachment::isEmpty() const
{
    return !m_fileWrapper;
}

RefPtr<WebCore::SharedBuffer> Attachment::createSerializedRepresentation() const
{
    if (!m_fileWrapper || !m_webPage)
        return nullptr;

#if CAN_SECURELY_ARCHIVE_FILE_WRAPPER
    NSData *serializedData = securelyArchivedDataWithRootObject(m_fileWrapper.get());
#else
    NSData *serializedData = insecurelyArchivedDataWithRootObject(m_fileWrapper.get());
#endif

    if (!serializedData)
        return nullptr;

    return WebCore::SharedBuffer::create(serializedData);
}

void Attachment::updateFromSerializedRepresentation(Ref<WebCore::SharedBuffer>&& serializedRepresentation, const WTF::String& contentType)
{
    if (!m_webPage)
        return;

    auto serializedData = serializedRepresentation->createNSData();
    if (!serializedData)
        return;

#if CAN_SECURELY_ARCHIVE_FILE_WRAPPER
    NSFileWrapper *fileWrapper = unarchivedObjectOfClassesFromData(m_webPage->pageClient().serializableFileWrapperClasses(), serializedData.get());
#else
    NSFileWrapper *fileWrapper = insecurelyUnarchiveObjectFromData(serializedData.get());
#endif

    if (![fileWrapper isKindOfClass:NSFileWrapper.class])
        return;

    setFileWrapperAndUpdateContentType(fileWrapper, contentType);
    m_webPage->updateAttachmentAttributes(*this, [] (auto) { });
}

} // namespace API
