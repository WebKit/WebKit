/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#else
#import <CoreServices/CoreServices.h>
#endif

namespace API {

static WTF::String mimeTypeInferredFromFileExtension(const API::Attachment& attachment)
{
    if (RetainPtr<NSString> fileExtension = [attachment.fileName().createNSString() pathExtension])
        return WebCore::MIMETypeRegistry::mimeTypeForExtension(WTF::String(fileExtension.get()));

    return { };
}

static BOOL isDeclaredOrDynamicTypeIdentifier(UTType *type)
{
    return type.declared || type.dynamic;
}

void Attachment::setFileWrapper(NSFileWrapper *fileWrapper)
{
    Locker locker { m_fileWrapperLock };

    m_fileWrapper = fileWrapper;
}

void Attachment::doWithFileWrapper(NOESCAPE Function<void(NSFileWrapper *)>&& function) const
{
    Locker locker { m_fileWrapperLock };

    function(m_fileWrapper.get());
}

WTF::String Attachment::mimeType() const
{
    RetainPtr contentType = m_contentType.isEmpty() ? mimeTypeInferredFromFileExtension(*this).createNSString() : m_contentType.createNSString();
    if (!contentType.get().length)
        return nullString();

    if (RetainPtr utType = [UTType typeWithIdentifier:contentType.get()]) {
        if (isDeclaredOrDynamicTypeIdentifier(utType.get()))
            return [utType preferredMIMEType];
    }

    return contentType.get();
}

WTF::String Attachment::utiType() const
{
    RetainPtr contentType = m_contentType.isEmpty() ? mimeTypeInferredFromFileExtension(*this).createNSString() : m_contentType.createNSString();
    if (!contentType.get().length)
        return nullString();

    if (RetainPtr utType = [UTType typeWithIdentifier:contentType.get()]) {
        if (isDeclaredOrDynamicTypeIdentifier(utType.get()))
            return contentType.get();
    }

    RetainPtr preferredType = [UTType typeWithMIMEType:contentType.get()];
    return [preferredType identifier];
}

WTF::String Attachment::fileName() const
{
    Locker locker { m_fileWrapperLock };

    if ([m_fileWrapper filename].length)
        return [m_fileWrapper filename];

    return [m_fileWrapper preferredFilename];
}

void Attachment::setFileWrapperAndUpdateContentType(NSFileWrapper *fileWrapper, NSString *contentType)
{
    RetainPtr updatedContentType = contentType;
    if (!updatedContentType.get().length) {
        if (fileWrapper.directory)
            updatedContentType = UTTypeDirectory.identifier;
        else if (fileWrapper.regularFile) {
            if (RetainPtr<NSString> pathExtension = (fileWrapper.filename.length ? fileWrapper.filename : fileWrapper.preferredFilename).pathExtension)
                updatedContentType = WebCore::MIMETypeRegistry::mimeTypeForExtension(WTF::String(pathExtension.get())).createNSString();
            if (!updatedContentType.get().length)
                updatedContentType = UTTypeData.identifier;
        }
    }

    setContentType(updatedContentType.get());
    setFileWrapper(fileWrapper);
}

std::optional<uint64_t> Attachment::fileSizeForDisplay() const
{
    Locker locker { m_fileWrapperLock };

    if (![m_fileWrapper isRegularFile]) {
        // FIXME: We should display a size estimate for directory-type file wrappers.
        return std::nullopt;
    }

    if (auto fileSize = [[m_fileWrapper fileAttributes][NSFileSize] unsignedLongLongValue])
        return fileSize;

    return [m_fileWrapper regularFileContents].length;
}

RefPtr<WebCore::FragmentedSharedBuffer> Attachment::associatedElementData() const
{
    if (m_associatedElementType == WebCore::AttachmentAssociatedElementType::None)
        return nullptr;

    NSData *data = nil;
    {
        Locker locker { m_fileWrapperLock };

        if (![m_fileWrapper isRegularFile])
            return nullptr;

        data = [m_fileWrapper regularFileContents];
    }

    if (!data)
        return nullptr;

    return WebCore::SharedBuffer::create(data);
}

RetainPtr<NSData> Attachment::associatedElementNSData() const
{
    Locker locker { m_fileWrapperLock };

    if (![m_fileWrapper isRegularFile])
        return nil;

    return [m_fileWrapper regularFileContents];
}

bool Attachment::isEmpty() const
{
    Locker locker { m_fileWrapperLock };

    return !m_fileWrapper;
}

RefPtr<WebCore::SharedBuffer> Attachment::createSerializedRepresentation() const
{
    NSData *serializedData = nil;
    {
        Locker locker { m_fileWrapperLock };

        if (!m_fileWrapper || !m_webPage)
            return nullptr;

        serializedData = [NSKeyedArchiver archivedDataWithRootObject:m_fileWrapper.get() requiringSecureCoding:YES error:nullptr];
    }
    if (!serializedData)
        return nullptr;

    return WebCore::SharedBuffer::create(serializedData);
}

void Attachment::updateFromSerializedRepresentation(Ref<WebCore::SharedBuffer>&& serializedRepresentation, const WTF::String& contentType)
{
    RefPtr webPage = m_webPage.get();
    if (!webPage)
        return;

    RefPtr pageClient = webPage->pageClient();
    if (!pageClient)
        return;

    auto serializedData = serializedRepresentation->createNSData();
    if (!serializedData)
        return;

    RetainPtr classes = pageClient->serializableFileWrapperClasses();
    RetainPtr fileWrapper = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes.get() fromData:serializedData.get() error:nullptr];
    if (![fileWrapper isKindOfClass:NSFileWrapper.class])
        return;

    m_isCreatedFromSerializedRepresentation = true;
    setFileWrapperAndUpdateContentType(fileWrapper.get(), contentType.createNSString().get());
    webPage->updateAttachmentAttributes(*this, [] { });
}

void Attachment::cloneFileWrapperTo(Attachment& other)
{
    other.m_isCreatedFromSerializedRepresentation = m_isCreatedFromSerializedRepresentation;

    Locker locker { m_fileWrapperLock };
    other.setFileWrapper(m_fileWrapper.get());
}

bool Attachment::shouldUseFileWrapperIconForDirectory() const
{
    if (m_contentType != "public.directory"_s)
        return false;

    if (m_isCreatedFromSerializedRepresentation)
        return false;

    {
        Locker locker { m_fileWrapperLock };
        if (![m_fileWrapper isDirectory])
            return false;
    }

    return true;
}

} // namespace API
