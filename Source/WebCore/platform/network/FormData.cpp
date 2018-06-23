/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FormData.h"

#include "BlobRegistryImpl.h"
#include "BlobURL.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMFormData.h"
#include "Document.h"
#include "File.h"
#include "FileSystem.h"
#include "FormDataBuilder.h"
#include "Page.h"
#include "TextEncoding.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/text/LineEnding.h>

namespace WebCore {

inline FormData::FormData()
{
}

inline FormData::FormData(const FormData& data)
    : RefCounted<FormData>()
    , m_elements(data.m_elements)
    , m_identifier(data.m_identifier)
    , m_alwaysStream(false)
    , m_containsPasswordData(data.m_containsPasswordData)
{
    // We shouldn't be copying FormData that hasn't already removed its generated files
    // but just in case, make sure the new FormData is ready to generate its own files.
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile) {
            element.m_generatedFilename = String();
            element.m_ownsGeneratedFile = false;
        }
    }
}

FormData::~FormData()
{
    // This cleanup should've happened when the form submission finished.
    // Just in case, let's assert, and do the cleanup anyway in release builds.
    ASSERT(!hasOwnedGeneratedFiles());
    removeGeneratedFilesIfNeeded();
}

Ref<FormData> FormData::create()
{
    return adoptRef(*new FormData);
}

Ref<FormData> FormData::create(const void* data, size_t size)
{
    auto result = create();
    result->appendData(data, size);
    return result;
}

Ref<FormData> FormData::create(const CString& string)
{
    return create(string.data(), string.length());
}

Ref<FormData> FormData::create(const Vector<char>& vector)
{
    return create(vector.data(), vector.size());
}

Ref<FormData> FormData::create(const Vector<uint8_t>& vector)
{
    return create(vector.data(), vector.size());
}

Ref<FormData> FormData::create(const DOMFormData& formData, EncodingType encodingType)
{
    auto result = create();
    result->appendNonMultiPartKeyValuePairItems(formData, encodingType);
    return result;
}

Ref<FormData> FormData::createMultiPart(const DOMFormData& formData, Document* document)
{
    auto result = create();
    result->appendMultiPartKeyValuePairItems(formData, document);
    return result;
}

Ref<FormData> FormData::copy() const
{
    return adoptRef(*new FormData(*this));
}

Ref<FormData> FormData::isolatedCopy() const
{
    // FIXME: isolatedCopy() does not copy m_identifier, m_boundary, or m_containsPasswordData.
    // Is all of that correct and intentional?

    auto formData = create();

    formData->m_alwaysStream = m_alwaysStream;

    formData->m_elements.reserveInitialCapacity(m_elements.size());
    for (auto& element : m_elements)
        formData->m_elements.uncheckedAppend(element.isolatedCopy());

    return formData;
}

uint64_t FormDataElement::lengthInBytes() const
{
    switch (m_type) {
    case Type::Data:
        return m_data.size();
    case Type::EncodedFile: {
        if (m_fileLength != BlobDataItem::toEndOfFile)
            return m_fileLength;
        long long fileSize;
        if (FileSystem::getFileSize(m_shouldGenerateFile ? m_generatedFilename : m_filename, fileSize))
            return fileSize;
        return 0;
    }
    case Type::EncodedBlob:
        return ThreadableBlobRegistry::blobSize(m_url);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

FormDataElement FormDataElement::isolatedCopy() const
{
    switch (m_type) {
    case Type::Data:
        return FormDataElement(m_data);
    case Type::EncodedFile:
        return FormDataElement(m_filename.isolatedCopy(), m_fileStart, m_fileLength, m_expectedFileModificationTime, m_shouldGenerateFile);
    case Type::EncodedBlob:
        return FormDataElement(m_url.isolatedCopy());
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void FormData::appendData(const void* data, size_t size)
{
    memcpy(expandDataStore(size), data, size);
}

void FormData::appendFile(const String& filename, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, 0, BlobDataItem::toEndOfFile, FileSystem::invalidFileTime(), shouldGenerateFile));
    m_lengthInBytes = std::nullopt;
}

void FormData::appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, start, length, expectedModificationTime, shouldGenerateFile));
    m_lengthInBytes = std::nullopt;
}

void FormData::appendBlob(const URL& blobURL)
{
    m_elements.append(FormDataElement(blobURL));
    m_lengthInBytes = std::nullopt;
}

static Vector<uint8_t> normalizeStringData(TextEncoding& encoding, const String& value)
{
    return normalizeLineEndingsToCRLF(encoding.encode(value, UnencodableHandling::Entities));
}

void FormData::appendMultiPartFileValue(const File& file, Vector<char>& header, TextEncoding& encoding, Document* document)
{
    auto name = file.name();

    // Let the application specify a filename if it's going to generate a replacement file for the upload.
    bool shouldGenerateFile = false;
    auto& path = file.path();
    if (!path.isEmpty()) {
        if (Page* page = document->page()) {
            String generatedFileName;
            shouldGenerateFile = page->chrome().client().shouldReplaceWithGeneratedFileForUpload(path, generatedFileName);
            if (shouldGenerateFile)
                name = generatedFileName;
        }
    }

    // We have to include the filename=".." part in the header, even if the filename is empty
    FormDataBuilder::addFilenameToMultiPartHeader(header, encoding, name);

    // Add the content type if available, or "application/octet-stream" otherwise (RFC 1867).
    auto contentType = file.type();
    if (contentType.isEmpty())
        contentType = "application/octet-stream"_s;
    ASSERT(Blob::isNormalizedContentType(contentType));

    FormDataBuilder::addContentTypeToMultiPartHeader(header, contentType.ascii());

    FormDataBuilder::finishMultiPartHeader(header);
    appendData(header.data(), header.size());

    if (!file.path().isEmpty())
        appendFile(file.path(), shouldGenerateFile);
    else if (file.size())
        appendBlob(file.url());
}

void FormData::appendMultiPartStringValue(const String& string, Vector<char>& header, TextEncoding& encoding)
{
    FormDataBuilder::finishMultiPartHeader(header);
    appendData(header.data(), header.size());

    auto normalizedStringData = normalizeStringData(encoding, string);
    appendData(normalizedStringData.data(), normalizedStringData.size());
}

void FormData::appendMultiPartKeyValuePairItems(const DOMFormData& formData, Document* document)
{
    m_boundary = FormDataBuilder::generateUniqueBoundaryString();

    auto encoding = formData.encoding();

    Vector<char> encodedData;
    for (auto& item : formData.items()) {
        auto normalizedName = normalizeStringData(encoding, item.name);
    
        Vector<char> header;
        FormDataBuilder::beginMultiPartHeader(header, m_boundary.data(), normalizedName);

        if (WTF::holds_alternative<RefPtr<File>>(item.data))
            appendMultiPartFileValue(*WTF::get<RefPtr<File>>(item.data), header, encoding, document);
        else
            appendMultiPartStringValue(WTF::get<String>(item.data), header, encoding);

        appendData("\r\n", 2);
    }
    
    FormDataBuilder::addBoundaryToMultiPartHeader(encodedData, m_boundary.data(), true);

    appendData(encodedData.data(), encodedData.size());
}

void FormData::appendNonMultiPartKeyValuePairItems(const DOMFormData& formData, EncodingType encodingType)
{
    auto encoding = formData.encoding();

    Vector<char> encodedData;
    for (auto& item : formData.items()) {
        ASSERT(WTF::holds_alternative<String>(item.data));

        auto normalizedName = normalizeStringData(encoding, item.name);
        auto normalizedStringData = normalizeStringData(encoding, WTF::get<String>(item.data));
        FormDataBuilder::addKeyValuePairAsFormData(encodedData, normalizedName, normalizedStringData, encodingType);
    }

    appendData(encodedData.data(), encodedData.size());
}

char* FormData::expandDataStore(size_t size)
{
    m_lengthInBytes = std::nullopt;
    if (m_elements.isEmpty() || m_elements.last().m_type != FormDataElement::Type::Data)
        m_elements.append({ });

    auto& lastElement = m_elements.last();
    size_t oldSize = lastElement.m_data.size();

    auto newSize = Checked<size_t>(oldSize) + size;

    lastElement.m_data.grow(newSize.unsafeGet());
    return lastElement.m_data.data() + oldSize;
}

Vector<char> FormData::flatten() const
{
    // Concatenate all the byte arrays, but omit any files.
    Vector<char> data;
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::Data)
            data.append(element.m_data.data(), static_cast<size_t>(element.m_data.size()));
    }
    return data;
}

String FormData::flattenToString() const
{
    auto bytes = flatten();
    return Latin1Encoding().decode(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

static void appendBlobResolved(FormData* formData, const URL& url)
{
    if (!blobRegistry().isBlobRegistryImpl()) {
        LOG_ERROR("Tried to resolve a blob without a usable registry");
        return;
    }

    BlobData* blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(url);
    if (!blobData) {
        LOG_ERROR("Could not get blob data from a registry");
        return;
    }

    for (const auto& blobItem : blobData->items()) {
        if (blobItem.type() == BlobDataItem::Type::Data) {
            ASSERT(blobItem.data().data());
            formData->appendData(blobItem.data().data()->data() + static_cast<int>(blobItem.offset()), static_cast<int>(blobItem.length()));
        } else if (blobItem.type() == BlobDataItem::Type::File)
            formData->appendFileRange(blobItem.file()->path(), blobItem.offset(), blobItem.length(), blobItem.file()->expectedModificationTime());
        else
            ASSERT_NOT_REACHED();
    }
}

Ref<FormData> FormData::resolveBlobReferences()
{
    // First check if any blobs needs to be resolved, or we can take the fast path.
    bool hasBlob = false;
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedBlob) {
            hasBlob = true;
            break;
        }
    }

    if (!hasBlob)
        return *this;

    // Create a copy to append the result into.
    auto newFormData = FormData::create();
    newFormData->setAlwaysStream(alwaysStream());
    newFormData->setIdentifier(identifier());

    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::Data)
            newFormData->appendData(element.m_data.data(), element.m_data.size());
        else if (element.m_type == FormDataElement::Type::EncodedFile)
            newFormData->appendFileRange(element.m_filename, element.m_fileStart, element.m_fileLength, element.m_expectedFileModificationTime, element.m_shouldGenerateFile);
        else if (element.m_type == FormDataElement::Type::EncodedBlob)
            appendBlobResolved(newFormData.ptr(), element.m_url);
        else
            ASSERT_NOT_REACHED();
    }
    return newFormData;
}

void FormData::generateFiles(Document* document)
{
    Page* page = document->page();
    if (!page)
        return;

    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && element.m_shouldGenerateFile) {
            ASSERT(!element.m_ownsGeneratedFile);
            ASSERT(element.m_generatedFilename.isEmpty());
            if (!element.m_generatedFilename.isEmpty())
                continue;
            element.m_generatedFilename = page->chrome().client().generateReplacementFile(element.m_filename);
            if (!element.m_generatedFilename.isEmpty())
                element.m_ownsGeneratedFile = true;
        }
    }
}

bool FormData::hasGeneratedFiles() const
{
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && !element.m_generatedFilename.isEmpty())
            return true;
    }
    return false;
}

bool FormData::hasOwnedGeneratedFiles() const
{
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && element.m_ownsGeneratedFile) {
            ASSERT(!element.m_generatedFilename.isEmpty());
            return true;
        }
    }
    return false;
}

void FormData::removeGeneratedFilesIfNeeded()
{
    for (auto& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && element.m_ownsGeneratedFile) {
            ASSERT(!element.m_generatedFilename.isEmpty());
            ASSERT(element.m_shouldGenerateFile);
            String directory = FileSystem::directoryName(element.m_generatedFilename);
            FileSystem::deleteFile(element.m_generatedFilename);
            FileSystem::deleteEmptyDirectory(directory);
            element.m_generatedFilename = String();
            element.m_ownsGeneratedFile = false;
        }
    }
}

uint64_t FormData::lengthInBytes() const
{
    if (!m_lengthInBytes) {
        uint64_t length = 0;
        for (auto& element : m_elements)
            length += element.lengthInBytes();
        m_lengthInBytes = length;
    }
    return *m_lengthInBytes;
}

RefPtr<SharedBuffer> FormData::asSharedBuffer() const
{
    for (auto& element : m_elements) {
        if (element.m_type != FormDataElement::Type::Data)
            return nullptr;
    }
    return SharedBuffer::create(flatten());
}

URL FormData::asBlobURL() const
{
    if (m_elements.size() != 1)
        return { };

    ASSERT(m_elements.first().m_type == FormDataElement::Type::EncodedBlob || m_elements.first().m_url.isNull());
    return m_elements.first().m_url;
}

} // namespace WebCore
