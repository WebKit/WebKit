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

static RefPtr<StringImpl> mimeTypeInferredFromFileExtension(const API::Attachment& attachment)
{
    if (RetainPtr<NSString> fileExtension = [createNSString(attachment.fileName()) pathExtension])
        return WebCore::MIMETypeRegistry::mimeTypeForExtension(StringImpl::create(fileExtension.get()).get());

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

void Attachment::doWithFileWrapper(NOESCAPE const Function<void(NSFileWrapper *)>& function) const
{
    Locker locker { m_fileWrapperLock };

    function(protect(m_fileWrapper).get());
}

RefPtr<StringImpl> Attachment::mimeType() const
{
    RetainPtr contentType = WTF::isEmpty(m_contentType) ? createNSString(mimeTypeInferredFromFileExtension(*this)) : createNSString(m_contentType);
    if (!contentType.get().length)
        return nullString();

    if (RetainPtr utType = [UTType typeWithIdentifier:contentType.get()]) {
        if (isDeclaredOrDynamicTypeIdentifier(utType.get()))
            return StringImpl::create([utType preferredMIMEType]);
    }

    return StringImpl::create(contentType.get());
}

RefPtr<StringImpl> Attachment::utiType() const
{
    RetainPtr contentType = WTF::isEmpty(m_contentType) ? createNSString(mimeTypeInferredFromFileExtension(*this)) : createNSString(m_contentType);
    if (!contentType.get().length)
        return nullString();

    if (RetainPtr utType = [UTType typeWithIdentifier:contentType.get()]) {
        if (isDeclaredOrDynamicTypeIdentifier(utType.get()))
            return StringImpl::create(contentType.get());
    }

    RetainPtr preferredType = [UTType typeWithMIMEType:contentType.get()];
    return StringImpl::create([preferredType identifier]);
}

RefPtr<StringImpl> Attachment::fileName() const
{
    Locker locker { m_fileWrapperLock };

    RetainPtr fileWrapper = m_fileWrapper;
    if ([fileWrapper filename].length)
        return StringImpl::create([fileWrapper filename]);

    return StringImpl::create([fileWrapper preferredFilename]);
}

void Attachment::setFileWrapperAndUpdateContentType(NSFileWrapper *fileWrapper, NSString *contentType)
{
    RetainPtr updatedContentType = contentType;
    if (!updatedContentType.get().length) {
        if (fileWrapper.directory)
            updatedContentType = UTTypeDirectory.identifier;
        else if (fileWrapper.regularFile) {
            if (RetainPtr<NSString> pathExtension = protect(protect(fileWrapper.filename).get().length ? fileWrapper.filename : fileWrapper.preferredFilename).get().pathExtension)
                updatedContentType = createNSString(WebCore::MIMETypeRegistry::mimeTypeForExtension(StringImpl::create(pathExtension.get()).get()));
            if (!updatedContentType.get().length)
                updatedContentType = UTTypeData.identifier;
        }
    }

    setContentType(StringImpl::create(updatedContentType.get()).get());
    setFileWrapper(fileWrapper);
}

std::optional<uint64_t> Attachment::fileSizeForDisplay() const
{
    Locker locker { m_fileWrapperLock };

    RetainPtr fileWrapper = m_fileWrapper;
    if (![fileWrapper isRegularFile]) {
        // FIXME: We should display a size estimate for directory-type file wrappers.
        return std::nullopt;
    }

    if (auto fileSize = [[fileWrapper fileAttributes][NSFileSize] unsignedLongLongValue])
        return fileSize;

    return [fileWrapper regularFileContents].length;
}

RefPtr<WebCore::FragmentedSharedBuffer> Attachment::associatedElementData() const
{
    if (m_associatedElementType == WebCore::AttachmentAssociatedElementType::None)
        return nullptr;

    RetainPtr<NSData> data;
    {
        Locker locker { m_fileWrapperLock };

        RetainPtr fileWrapper = m_fileWrapper;
        if (![fileWrapper isRegularFile])
            return nullptr;

        data = [fileWrapper regularFileContents];
    }

    if (!data)
        return nullptr;

    return WebCore::SharedBuffer::create(data.get());
}

RetainPtr<NSData> Attachment::associatedElementNSData() const
{
    Locker locker { m_fileWrapperLock };

    RetainPtr fileWrapper = m_fileWrapper;
    if (![fileWrapper isRegularFile])
        return nil;

    return [fileWrapper regularFileContents];
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

        RetainPtr fileWrapper = m_fileWrapper;
        if (!fileWrapper || !m_webPage)
            return nullptr;

        serializedData = [NSKeyedArchiver archivedDataWithRootObject:fileWrapper.get() requiringSecureCoding:YES error:nullptr];
    }
    if (!serializedData)
        return nullptr;

    return WebCore::SharedBuffer::create(serializedData);
}

void Attachment::updateFromSerializedRepresentation(Ref<WebCore::SharedBuffer>&& serializedRepresentation, StringImpl* contentType)
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
    RetainPtr fileWrapper = dynamic_objc_cast<NSFileWrapper>([NSKeyedUnarchiver unarchivedObjectOfClasses:classes.get() fromData:serializedData.get() error:nullptr]);
    if (!fileWrapper)
        return;

    m_isCreatedFromSerializedRepresentation = true;
    setFileWrapperAndUpdateContentType(fileWrapper.get(), createNSString(contentType).get());
    webPage->updateAttachmentAttributes(*this, [] { });
}

void Attachment::cloneFileWrapperTo(Attachment& other)
{
    other.m_isCreatedFromSerializedRepresentation = m_isCreatedFromSerializedRepresentation;

    Locker locker { m_fileWrapperLock };
    other.setFileWrapper(protect(m_fileWrapper));
}

bool Attachment::shouldUseFileWrapperIconForDirectory() const
{
    if (!equal(m_contentType, "public.directory"_s))
        return false;

    if (m_isCreatedFromSerializedRepresentation)
        return false;

    {
        Locker locker { m_fileWrapperLock };
        if (![protect(m_fileWrapper) isDirectory])
            return false;
    }

    return true;
}

} // namespace API
