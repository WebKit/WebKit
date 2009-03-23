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

#include "CString.h"
#include "ChromeClient.h"
#include "FileSystem.h"
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
            formData->m_elements.append(FormDataElement(e.m_filename, e.m_shouldGenerateFile));
            break;
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
    m_elements.append(FormDataElement(filename, shouldGenerateFile));
}

void FormData::flatten(Vector<char>& data) const
{
    // Concatenate all the byte arrays, but omit any files.
    data.clear();
    size_t n = m_elements.size();
    for (size_t i = 0; i < n; ++i) {
        const FormDataElement& e = m_elements[i];
        if (e.m_type == FormDataElement::data) {
            size_t oldSize = data.size();
            size_t delta = e.m_data.size();
            data.grow(oldSize + delta);
            memcpy(data.data() + oldSize, e.m_data.data(), delta);
        }
    }
}

String FormData::flattenToString() const
{
    Vector<char> bytes;
    flatten(bytes);
    return Latin1Encoding().decode(bytes.data(), bytes.size());
}

void FormData::generateFiles(ChromeClient* client)
{
    ASSERT(!m_hasGeneratedFiles);
    
    if (m_hasGeneratedFiles)
        return;
    
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
