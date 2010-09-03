/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "BlobData.h"
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
    , m_hasGeneratedFiles(false)
    , m_alwaysStream(false)
{
}

inline FormData::FormData(const FormData& data)
    : RefCounted<FormData>()
    , m_elements(data.m_elements)
    , m_identifier(data.m_identifier)
    , m_hasGeneratedFiles(false)
    , m_alwaysStream(false)
{
    // We shouldn't be copying FormData that hasn't already removed its generated files
    // but just in case, make sure the new FormData is ready to generate its own files.
    if (data.m_hasGeneratedFiles) {
        size_t n = m_elements.size();
        for (size_t i = 0; i < n; ++i) {
            FormDataElement& e = m_elements[i];
            if (e.m_type == FormDataElement::encodedFile)
                e.m_generatedFilename = String();
        }
    }
}

FormData::~FormData()
{
    // This cleanup should've happened when the form submission finished.
    // Just in case, let's assert, and do the cleanup anyway in release builds.
    ASSERT(!m_hasGeneratedFiles);
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

PassRefPtr<FormData> FormData::create(const FormDataList& list, const TextEncoding& encoding)
{
    RefPtr<FormData> result = create();
    result->appendKeyValuePairItems(list, encoding, false, 0);
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

    size_t n = m_elements.size();
    formData->m_elements.reserveInitialCapacity(n);
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        switch (e.m_type) {
        case FormDataElement::data:
            formData->m_elements.append(FormDataElement(e.m_data));
            break;
        case FormDataElement::encodedFile:
#if ENABLE(BLOB)
            formData->m_elements.append(FormDataElement(e.m_filename, e.m_fileStart, e.m_fileLength, e.m_expectedFileModificationTime, e.m_shouldGenerateFile));
#else
            formData->m_elements.append(FormDataElement(e.m_filename, e.m_shouldGenerateFile));
#endif
            break;
#if ENABLE(BLOB)
        case FormDataElement::encodedBlob:
            formData->m_elements.append(FormDataElement(e.m_blobURL));
            break;
#endif
        }
    }
    return formData.release();
}

void FormData::appendData(const void* data, size_t size)
{
    if (m_elements.isEmpty() || m_elements.last().m_type != FormDataElement::data)
        m_elements.append(FormDataElement());
    FormDataElement& e = m_elements.last();
    size_t oldSize = e.m_data.size();
    e.m_data.grow(oldSize + size);
    memcpy(e.m_data.data() + oldSize, data, size);
}

void FormData::appendFile(const String& filename, bool shouldGenerateFile)
{
#if ENABLE(BLOB)
    m_elements.append(FormDataElement(filename, 0, BlobDataItem::toEndOfFile, BlobDataItem::doNotCheckFileChange, shouldGenerateFile));
#else
    m_elements.append(FormDataElement(filename, shouldGenerateFile));
#endif
}

#if ENABLE(BLOB)
void FormData::appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime, bool shouldGenerateFile)
{
    m_elements.append(FormDataElement(filename, start, length, expectedModificationTime, shouldGenerateFile));
}

void FormData::appendBlob(const KURL& blobURL)
{
    m_elements.append(FormDataElement(blobURL));
}
#endif

void FormData::appendKeyValuePairItems(const FormDataList& list, const TextEncoding& encoding, bool isMultiPartForm, Document* document)
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
                    // For file blob, use the filename (or relative path if it is present) as the name.
                    File* file = static_cast<File*>(value.blob());
#if ENABLE(DIRECTORY_UPLOAD)                
                    name = file->webkitRelativePath().isEmpty() ? file->name() : file->webkitRelativePath();
#else
                    name = file->name();
#endif                    

                    // Let the application specify a filename if it's going to generate a replacement file for the upload.
                    if (Page* page = document->page()) {
                        String generatedFileName;
                        shouldGenerateFile = page->chrome()->client()->shouldReplaceWithGeneratedFileForUpload(file->path(), generatedFileName);
                        if (shouldGenerateFile)
                            name = generatedFileName;
                    }
                } else {
                    // For non-file blob, use the identifier part of the URL as the name.
                    name = "Blob" + BlobURL::getIdentifier(value.blob()->url());
                    name = name.replace("-", ""); // For safety, remove '-' from the filename since some servers may not like it.
                }

                // We have to include the filename=".." part in the header, even if the filename is empty
                FormDataBuilder::addFilenameToMultiPartHeader(header, encoding, name);

                // Add the content type if it is available.
                if (!value.blob()->type().isEmpty())
                    FormDataBuilder::addContentTypeToMultiPartHeader(header, value.blob()->type().latin1());
            }

            FormDataBuilder::finishMultiPartHeader(header);

            // Append body
            appendData(header.data(), header.size());
            if (value.blob()) {
                if (value.blob()->isFile()) {
                    // Do not add the file if the path is empty.
                    if (!static_cast<File*>(value.blob())->path().isEmpty())
                        appendFile(static_cast<File*>(value.blob())->path(), shouldGenerateFile);
                }
#if ENABLE(BLOB)
                else
                    appendBlob(value.blob()->url());
#endif
            } else
                appendData(value.data().data(), value.data().length());
            appendData("\r\n", 2);
        } else {
            // Omit the name "isindex" if it's the first form data element.
            // FIXME: Why is this a good rule? Is this obsolete now?
            if (encodedData.isEmpty() && key.data() == "isindex")
                FormDataBuilder::encodeStringAsFormData(encodedData, value.data());
            else
                FormDataBuilder::addKeyValuePairAsFormData(encodedData, key.data(), value.data());
        }
    }

    if (isMultiPartForm)
        FormDataBuilder::addBoundaryToMultiPartHeader(encodedData, m_boundary.data(), true);

    appendData(encodedData.data(), encodedData.size());
}

void FormData::flatten(Vector<char>& data) const
{
    // Concatenate all the byte arrays, but omit any files.
    data.clear();
    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::data)
            data.append(e.m_data.data(), static_cast<size_t>(e.m_data.size()));
    }
}

String FormData::flattenToString() const
{
    Vector<char> bytes;
    flatten(bytes);
    return Latin1Encoding().decode(bytes.data(), bytes.size());
}

void FormData::generateFiles(Document* document)
{
    ASSERT(!m_hasGeneratedFiles);

    if (m_hasGeneratedFiles)
        return;

    Page* page = document->page();
    if (!page)
        return;
    ChromeClient* client = page->chrome()->client();

    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::encodedFile && e.m_shouldGenerateFile) {
            e.m_generatedFilename = client->generateReplacementFile(e.m_filename);
            m_hasGeneratedFiles = true;
        }
    }
}

void FormData::removeGeneratedFilesIfNeeded()
{
    if (!m_hasGeneratedFiles)
        return;

    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::encodedFile && !e.m_generatedFilename.isEmpty()) {
            ASSERT(e.m_shouldGenerateFile);
            String directory = directoryName(e.m_generatedFilename);
            deleteFile(e.m_generatedFilename);
            deleteEmptyDirectory(directory);
            e.m_generatedFilename = String();
        }
    }
    m_hasGeneratedFiles = false;
}

} // namespace WebCore
