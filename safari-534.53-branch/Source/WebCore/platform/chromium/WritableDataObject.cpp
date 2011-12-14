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
#include "WritableDataObject.h"

#include "ClipboardMimeTypes.h"
#include "PlatformBridge.h"

namespace WebCore {

PassRefPtr<WritableDataObject> WritableDataObject::create(Clipboard::ClipboardType clipboardType)
{
    return adoptRef(new WritableDataObject(clipboardType));
}

WritableDataObject::WritableDataObject(Clipboard::ClipboardType clipboardType)
    : m_clipboardType(clipboardType)
{
}

void WritableDataObject::clearData(const String& type)
{
    m_dataMap.remove(type);
    if (type == mimeTypeTextURIList)
        m_urlTitle = "";
    else if (type == mimeTypeTextHTML)
        m_htmlBaseURL = KURL();
}

void WritableDataObject::clearAllExceptFiles()
{
    // FIXME: The spec does not provide a way to populate FileList currently. In
    // fact, the spec explicitly states that dragging files can only happen from
    // outside a browsing context.
    clearAll();
}

void WritableDataObject::clearAll()
{
    m_dataMap.clear();
    m_urlTitle = "";
    m_htmlBaseURL = KURL();
    m_fileContentFilename = "";
    if (m_fileContent)
        m_fileContent->clear();
    m_fileExtension = "";
}

bool WritableDataObject::setData(const String& type, const String& data)
{
    if (m_clipboardType == Clipboard::CopyAndPaste) {
        // FIXME: This is currently unimplemented on the Chromium-side. This is
        // "okay" for now since the original implementation didn't support it
        // anyway. Going forward, this is something we'll need to fix though.
        PlatformBridge::clipboardWriteData(type, data, "");
        return true;
    }
    m_dataMap.set(type, data);
    if (type == mimeTypeTextURIList)
        m_urlTitle = "";
    else if (type == mimeTypeTextHTML)
        m_htmlBaseURL = KURL();
    return true;
}

} // namespace WebCore
