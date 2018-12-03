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
#include "SharedBuffer.h"
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
        if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
            fileData->generatedFilename = { };
            fileData->ownsGeneratedFile = false;
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

Ref<FormData> FormData::create(Vector<char>&& vector)
{
    auto data = create();
    data->m_elements.append(WTFMove(vector));
    return data;
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
    return switchOn(data,
        [] (const Vector<char>& bytes) {
            return static_cast<uint64_t>(bytes.size());
        }, [] (const FormDataElement::EncodedFileData& fileData) {
            if (fileData.fileLength != BlobDataItem::toEndOfFile)
                return static_cast<uint64_t>(fileData.fileLength);
            long long fileSize;
            if (FileSystem::getFileSize(fileData.shouldGenerateFile ? fileData.generatedFilename : fileData.filename, fileSize))
                return static_cast<uint64_t>(fileSize);
            return static_cast<uint64_t>(0);
        }, [] (const FormDataElement::EncodedBlobData& blobData) {
            return ThreadableBlobRegistry::blobSize(blobData.url);
        }
    );
}

FormDataElement FormDataElement::isolatedCopy() const
{
    return switchOn(data,
        [] (const Vector<char>& bytes) {
            Vector<char> copy;
            copy.append(bytes.data(), bytes.size());
            return FormDataElement(WTFMove(copy));
        }, [] (const FormDataElement::EncodedFileData& fileData) {
            return FormDataElement(fileData.isolatedCopy());
        }, [] (const FormDataElement::EncodedBlobData& blobData) {
            return FormDataElement(blobData.url.isolatedCopy());
        }
    );
}

void FormData::appendData(const void* data, size_t size)
{
    m_lengthInBytes = std::nullopt;
    if (!m_elements.isEmpty()) {
        if (auto* vector = WTF::get_if<Vector<char>>(m_elements.last().data)) {
            vector->append(reinterpret_cast<const char*>(data), size);
            return;
        }
    }
    Vector<char> vector;
    vector.append(reinterpret_cast<const char*>(data), size);
    m_elements.append(WTFMove(vector));
}

void FormData::appendFile(const String& filename, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, 0, BlobDataItem::toEndOfFile, std::nullopt, shouldGenerateFile));
    m_lengthInBytes = std::nullopt;
}

void FormData::appendFileRange(const String& filename, long long start, long long length, std::optional<WallTime> expectedModificationTime, bool shouldGenerateFile)
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

Vector<char> FormData::flatten() const
{
    // Concatenate all the byte arrays, but omit any files.
    Vector<char> data;
    for (auto& element : m_elements) {
        if (auto* vector = WTF::get_if<Vector<char>>(element.data))
            data.append(vector->data(), vector->size());
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
        if (WTF::holds_alternative<FormDataElement::EncodedBlobData>(element.data)) {
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
        switchOn(element.data,
            [&] (const Vector<char>& bytes) {
                newFormData->appendData(bytes.data(), bytes.size());
            }, [&] (const FormDataElement::EncodedFileData& fileData) {
                newFormData->appendFileRange(fileData.filename, fileData.fileStart, fileData.fileLength, fileData.expectedFileModificationTime, fileData.shouldGenerateFile);
            }, [&] (const FormDataElement::EncodedBlobData& blobData) {
                appendBlobResolved(newFormData.ptr(), blobData.url);
            }
        );
    }
    return newFormData;
}

void FormData::generateFiles(Document* document)
{
    Page* page = document->page();
    if (!page)
        return;

    for (auto& element : m_elements) {
        if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
            if (fileData->shouldGenerateFile) {
                ASSERT(!fileData->ownsGeneratedFile);
                ASSERT(fileData->generatedFilename.isEmpty());
                if (!fileData->generatedFilename.isEmpty())
                    continue;
                fileData->generatedFilename = page->chrome().client().generateReplacementFile(fileData->filename);
                if (!fileData->generatedFilename.isEmpty())
                    fileData->ownsGeneratedFile = true;
            }
        }
    }
}

bool FormData::hasGeneratedFiles() const
{
    for (auto& element : m_elements) {
        if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
            if (!fileData->generatedFilename.isEmpty())
                return true;
        }
    }
    return false;
}

bool FormData::hasOwnedGeneratedFiles() const
{
    for (auto& element : m_elements) {
        if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
            if (fileData->ownsGeneratedFile) {
                ASSERT(!fileData->generatedFilename.isEmpty());
                return true;
            }
        }
    }
    return false;
}

void FormData::removeGeneratedFilesIfNeeded()
{
    for (auto& element : m_elements) {
        if (auto* fileData = WTF::get_if<FormDataElement::EncodedFileData>(element.data)) {
            if (fileData->ownsGeneratedFile) {
                ASSERT(!fileData->generatedFilename.isEmpty());
                ASSERT(fileData->shouldGenerateFile);
                String directory = FileSystem::directoryName(fileData->generatedFilename);
                FileSystem::deleteFile(fileData->generatedFilename);
                FileSystem::deleteEmptyDirectory(directory);
                fileData->generatedFilename = String();
                fileData->ownsGeneratedFile = false;
            }
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
        if (!WTF::holds_alternative<Vector<char>>(element.data))
            return nullptr;
    }
    return SharedBuffer::create(flatten());
}

URL FormData::asBlobURL() const
{
    if (m_elements.size() != 1)
        return { };

    if (auto* blobData = WTF::get_if<FormDataElement::EncodedBlobData>(m_elements.first().data))
        return blobData->url;
    return { };
}

} // namespace WebCore
