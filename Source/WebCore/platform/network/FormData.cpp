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
#include "Document.h"
#include "File.h"
#include "FileSystem.h"
#include "FormDataBuilder.h"
#include "FormDataList.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "TextEncoding.h"

namespace WebCore {

inline FormData::FormData()
    : m_identifier(0)
    , m_alwaysStream(false)
    , m_containsPasswordData(false)
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
    for (FormDataElement& element : m_elements) {
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

PassRefPtr<FormData> FormData::create()
{
    return adoptRef(new FormData);
}

PassRefPtr<FormData> FormData::create(const void* data, size_t size)
{
    RefPtr<FormData> result = create();
    result->appendData(data, size);
    return result.release();
}

PassRefPtr<FormData> FormData::create(const CString& string)
{
    RefPtr<FormData> result = create();
    result->appendData(string.data(), string.length());
    return result.release();
}

PassRefPtr<FormData> FormData::create(const Vector<char>& vector)
{
    RefPtr<FormData> result = create();
    result->appendData(vector.data(), vector.size());
    return result.release();
}

PassRefPtr<FormData> FormData::create(const FormDataList& list, const TextEncoding& encoding, EncodingType encodingType)
{
    RefPtr<FormData> result = create();
    result->appendKeyValuePairItems(list, encoding, false, 0, encodingType);
    return result.release();
}

PassRefPtr<FormData> FormData::createMultiPart(const FormDataList& list, const TextEncoding& encoding, Document* document)
{
    RefPtr<FormData> result = create();
    result->appendKeyValuePairItems(list, encoding, true, document);
    return result.release();
}

PassRefPtr<FormData> FormData::copy() const
{
    return adoptRef(new FormData(*this));
}

PassRefPtr<FormData> FormData::deepCopy() const
{
    RefPtr<FormData> formData(create());

    formData->m_alwaysStream = m_alwaysStream;

    formData->m_elements.reserveInitialCapacity(m_elements.size());
    for (const FormDataElement& element : m_elements) {
        switch (element.m_type) {
        case FormDataElement::Type::Data:
            formData->m_elements.uncheckedAppend(FormDataElement(element.m_data));
            break;
        case FormDataElement::Type::EncodedFile:
            formData->m_elements.uncheckedAppend(FormDataElement(element.m_filename, element.m_fileStart, element.m_fileLength, element.m_expectedFileModificationTime, element.m_shouldGenerateFile));
            break;
        case FormDataElement::Type::EncodedBlob:
            formData->m_elements.uncheckedAppend(FormDataElement(element.m_url));
            break;
        }
    }
    return formData.release();
}

void FormData::appendData(const void* data, size_t size)
{
    memcpy(expandDataStore(size), data, size);
}

void FormData::appendFile(const String& filename, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, 0, BlobDataItem::toEndOfFile, invalidFileTime(), shouldGenerateFile));
}

void FormData::appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, start, length, expectedModificationTime, shouldGenerateFile));
}

void FormData::appendBlob(const URL& blobURL)
{
    m_elements.append(FormDataElement(blobURL));
}

void FormData::appendKeyValuePairItems(const FormDataList& list, const TextEncoding& encoding, bool isMultiPartForm, Document* document, EncodingType encodingType)
{
    if (isMultiPartForm)
        m_boundary = FormDataBuilder::generateUniqueBoundaryString();

    Vector<char> encodedData;

    const Vector<FormDataList::Item>& items = list.items();
    size_t formDataListSize = items.size();
    ASSERT(!(formDataListSize % 2));
    for (size_t i = 0; i < formDataListSize; i += 2) {
        const FormDataList::Item& key = items[i];
        const FormDataList::Item& value = items[i + 1];
        if (isMultiPartForm) {
            Vector<char> header;
            FormDataBuilder::beginMultiPartHeader(header, m_boundary.data(), key.data());

            bool shouldGenerateFile = false;

            // If the current type is blob, then we also need to include the filename
            if (value.blob()) {
                String name;
                if (value.blob()->isFile()) {
                    File* file = toFile(value.blob());
                    name = file->name();
                    // Let the application specify a filename if it's going to generate a replacement file for the upload.
                    const String& path = file->path();
                    if (!path.isEmpty()) {
                        if (Page* page = document->page()) {
                            String generatedFileName;
                            shouldGenerateFile = page->chrome().client().shouldReplaceWithGeneratedFileForUpload(path, generatedFileName);
                            if (shouldGenerateFile)
                                name = generatedFileName;
                        }
                    }

                    // If a filename is passed in FormData.append(), use it instead of the file blob's name.
                    if (!value.filename().isNull())
                        name = value.filename();
                } else {
                    // For non-file blob, use the filename if it is passed in FormData.append().
                    if (!value.filename().isNull())
                        name = value.filename();
                    else
                        name = "blob";
                }

                // We have to include the filename=".." part in the header, even if the filename is empty
                FormDataBuilder::addFilenameToMultiPartHeader(header, encoding, name);

                // Add the content type if available, or "application/octet-stream" otherwise (RFC 1867).
                String contentType = value.blob()->type();
                if (contentType.isEmpty())
                    contentType = "application/octet-stream";
                ASSERT(Blob::isNormalizedContentType(contentType));
                FormDataBuilder::addContentTypeToMultiPartHeader(header, contentType.ascii());
            }

            FormDataBuilder::finishMultiPartHeader(header);

            // Append body
            appendData(header.data(), header.size());
            if (value.blob()) {
                if (value.blob()->isFile()) {
                    File* file = toFile(value.blob());
                    // Do not add the file if the path is empty.
                    if (!file->path().isEmpty())
                        appendFile(file->path(), shouldGenerateFile);
                }
                else
                    appendBlob(value.blob()->url());
            } else
                appendData(value.data().data(), value.data().length());
            appendData("\r\n", 2);
        } else {
            // Omit the name "isindex" if it's the first form data element.
            // FIXME: Why is this a good rule? Is this obsolete now?
            if (encodedData.isEmpty() && key.data() == "isindex")
                FormDataBuilder::encodeStringAsFormData(encodedData, value.data());
            else
                FormDataBuilder::addKeyValuePairAsFormData(encodedData, key.data(), value.data(), encodingType);
        }
    }

    if (isMultiPartForm)
        FormDataBuilder::addBoundaryToMultiPartHeader(encodedData, m_boundary.data(), true);

    appendData(encodedData.data(), encodedData.size());
}

char* FormData::expandDataStore(size_t size)
{
    if (m_elements.isEmpty() || m_elements.last().m_type != FormDataElement::Type::Data)
        m_elements.append(FormDataElement());
    FormDataElement& e = m_elements.last();
    size_t oldSize = e.m_data.size();
    e.m_data.grow(oldSize + size);
    return e.m_data.data() + oldSize;
}

void FormData::flatten(Vector<char>& data) const
{
    // Concatenate all the byte arrays, but omit any files.
    data.clear();
    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::Type::Data)
            data.append(e.m_data.data(), static_cast<size_t>(e.m_data.size()));
    }
}

String FormData::flattenToString() const
{
    Vector<char> bytes;
    flatten(bytes);
    return Latin1Encoding().decode(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

static void appendBlobResolved(FormData* formData, const URL& url)
{
    if (!blobRegistry().isBlobRegistryImpl()) {
        LOG_ERROR("Tried to resolve a blob without a usable registry");
        return;
    }
    BlobData* blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(URL(ParsedURLString, url));
    if (!blobData) {
        LOG_ERROR("Could not get blob data from a registry");
        return;
    }

    BlobDataItemList::const_iterator it = blobData->items().begin();
    const BlobDataItemList::const_iterator itend = blobData->items().end();
    for (; it != itend; ++it) {
        const BlobDataItem& blobItem = *it;
        if (blobItem.type == BlobDataItem::Data)
            formData->appendData(blobItem.data->data() + static_cast<int>(blobItem.offset()), static_cast<int>(blobItem.length()));
        else if (blobItem.type == BlobDataItem::File)
            formData->appendFileRange(blobItem.file->path(), blobItem.offset(), blobItem.length(), blobItem.file->expectedModificationTime());
        else
            ASSERT_NOT_REACHED();
    }
}

PassRefPtr<FormData> FormData::resolveBlobReferences()
{
    // First check if any blobs needs to be resolved, or we can take the fast path.
    bool hasBlob = false;
    Vector<FormDataElement>::const_iterator it = elements().begin();
    const Vector<FormDataElement>::const_iterator itend = elements().end();
    for (; it != itend; ++it) {
        if (it->m_type == FormDataElement::Type::EncodedBlob) {
            hasBlob = true;
            break;
        }
    }

    if (!hasBlob)
        return this;

    // Create a copy to append the result into.
    RefPtr<FormData> newFormData = FormData::create();
    newFormData->setAlwaysStream(alwaysStream());
    newFormData->setIdentifier(identifier());
    it = elements().begin();
    for (; it != itend; ++it) {
        const FormDataElement& element = *it;
        if (element.m_type == FormDataElement::Type::Data)
            newFormData->appendData(element.m_data.data(), element.m_data.size());
        else if (element.m_type == FormDataElement::Type::EncodedFile)
            newFormData->appendFileRange(element.m_filename, element.m_fileStart, element.m_fileLength, element.m_expectedFileModificationTime, element.m_shouldGenerateFile);
        else if (element.m_type == FormDataElement::Type::EncodedBlob)
            appendBlobResolved(newFormData.get(), element.m_url);
        else
            ASSERT_NOT_REACHED();
    }
    return newFormData.release();
}

void FormData::generateFiles(Document* document)
{
    Page* page = document->page();
    if (!page)
        return;

    for (FormDataElement& element : m_elements) {
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
    for (const FormDataElement& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && !element.m_generatedFilename.isEmpty())
            return true;
    }
    return false;
}

bool FormData::hasOwnedGeneratedFiles() const
{
    for (const FormDataElement& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && element.m_ownsGeneratedFile) {
            ASSERT(!element.m_generatedFilename.isEmpty());
            return true;
        }
    }
    return false;
}

void FormData::removeGeneratedFilesIfNeeded()
{
    for (FormDataElement& element : m_elements) {
        if (element.m_type == FormDataElement::Type::EncodedFile && element.m_ownsGeneratedFile) {
            ASSERT(!element.m_generatedFilename.isEmpty());
            ASSERT(element.m_shouldGenerateFile);
            String directory = directoryName(element.m_generatedFilename);
            deleteFile(element.m_generatedFilename);
            deleteEmptyDirectory(directory);
            element.m_generatedFilename = String();
            element.m_ownsGeneratedFile = false;
        }
    }
}

} // namespace WebCore
