/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ReadableDataObject.h"

#include "ChromiumBridge.h"
#include "ClipboardMimeTypes.h"

namespace WebCore {

static PasteboardPrivate::ClipboardBuffer clipboardBuffer(bool isForDragging)
{
    return isForDragging ? PasteboardPrivate::DragBuffer : PasteboardPrivate::StandardBuffer;
}

PassRefPtr<ReadableDataObject> ReadableDataObject::create(bool isForDragging)
{
    return adoptRef(new ReadableDataObject(isForDragging));
}

ReadableDataObject::ReadableDataObject(bool isForDragging)
    : m_isForDragging(isForDragging)
    , m_containsFilenames(false)
    , m_isTypeCacheInitialized(false)
{
}

bool ReadableDataObject::hasData() const
{
    ensureTypeCacheInitialized();
    return !m_types.isEmpty() || m_containsFilenames;
}

HashSet<String> ReadableDataObject::types() const
{
    ensureTypeCacheInitialized();
    return m_types;
}

String ReadableDataObject::getData(const String& type, bool& succeeded) const
{
    String data;
    String ignoredMetadata;
    succeeded = ChromiumBridge::clipboardReadData(
        clipboardBuffer(m_isForDragging), type, data, ignoredMetadata);
    return data;
}

String ReadableDataObject::getURL(String* title) const
{
    String url;
    String ignoredTitle;
    if (!title)
        title = &ignoredTitle;
    ChromiumBridge::clipboardReadData(
        clipboardBuffer(m_isForDragging), textUriListType, url, *title);
    return url;
}

String ReadableDataObject::getHTML(String* baseURL) const
{
    String html;
    String ignoredBaseURL;
    if (!baseURL)
        baseURL = &ignoredBaseURL;
    ChromiumBridge::clipboardReadData(
        clipboardBuffer(m_isForDragging), textHtmlType, html, *baseURL);
    return html;
}

bool ReadableDataObject::hasFilenames() const
{
    ensureTypeCacheInitialized();
    return m_containsFilenames;
}

Vector<String> ReadableDataObject::filenames() const
{
    return ChromiumBridge::clipboardReadFilenames(clipboardBuffer(m_isForDragging));
}

void ReadableDataObject::ensureTypeCacheInitialized() const
{
    if (m_isTypeCacheInitialized)
        return;

    m_types = ChromiumBridge::clipboardReadAvailableTypes(
        clipboardBuffer(m_isForDragging), &m_containsFilenames);
    m_isTypeCacheInitialized = true;
}

} // namespace WebCore
