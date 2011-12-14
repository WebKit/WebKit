/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "ChromiumDataObject.h"

#include "ClipboardMimeTypes.h"
#include "Pasteboard.h"
#include "PlatformBridge.h"

namespace WebCore {

// Per RFC 2483, the line separator for "text/..." MIME types is CR-LF.
static char const* const textMIMETypeLineSeparator = "\r\n";

void ChromiumDataObject::clearData(const String& type)
{
    if (type == mimeTypeTextPlain) {
        m_plainText = "";
        return;
    }

    if (type == mimeTypeURL || type == mimeTypeTextURIList) {
        m_uriList = "";
        m_url = KURL();
        m_urlTitle = "";
        return;
    }

    if (type == mimeTypeTextHTML) {
        m_textHtml = "";
        m_htmlBaseUrl = KURL();
        return;
    }

    if (type == mimeTypeDownloadURL) {
        m_downloadMetadata = "";
        return;
    }
}

void ChromiumDataObject::clearAll()
{
    clearAllExceptFiles();
    m_filenames.clear();
}

void ChromiumDataObject::clearAllExceptFiles()
{
    m_urlTitle = "";
    m_url = KURL();
    m_uriList = "";
    m_downloadMetadata = "";
    m_fileExtension = "";
    m_plainText = "";
    m_textHtml = "";
    m_htmlBaseUrl = KURL();
    m_fileContentFilename = "";
    if (m_fileContent)
        m_fileContent->clear();
}

bool ChromiumDataObject::hasData() const
{
    return !m_url.isEmpty()
        || !m_uriList.isEmpty()
        || !m_downloadMetadata.isEmpty()
        || !m_fileExtension.isEmpty()
        || !m_filenames.isEmpty()
        || !m_plainText.isEmpty()
        || !m_textHtml.isEmpty()
        || m_fileContent;
}

HashSet<String> ChromiumDataObject::types() const
{
    if (m_clipboardType == Clipboard::CopyAndPaste) {
        bool ignoredContainsFilenames;
        return PlatformBridge::clipboardReadAvailableTypes(PasteboardPrivate::StandardBuffer,
                                                           &ignoredContainsFilenames);
    }

    HashSet<String> results;

    if (!m_plainText.isEmpty()) {
        results.add(mimeTypeText);
        results.add(mimeTypeTextPlain);
    }

    if (m_url.isValid())
        results.add(mimeTypeURL);

    if (!m_uriList.isEmpty())
        results.add(mimeTypeTextURIList);

    if (!m_textHtml.isEmpty())
        results.add(mimeTypeTextHTML);

    return results;
}

String ChromiumDataObject::getData(const String& type, bool& success)
{
    if (type == mimeTypeTextPlain) {
        if (m_clipboardType == Clipboard::CopyAndPaste) {
            PasteboardPrivate::ClipboardBuffer buffer =
                Pasteboard::generalPasteboard()->isSelectionMode() ?
                PasteboardPrivate::SelectionBuffer :
                PasteboardPrivate::StandardBuffer;
            String text = PlatformBridge::clipboardReadPlainText(buffer);
            success = !text.isEmpty();
            return text;
        }
        success = !m_plainText.isEmpty();
        return m_plainText;
    }

    if (type == mimeTypeURL) {
        success = !m_url.isEmpty();
        return m_url.string();
    }

    if (type == mimeTypeTextURIList) {
        success = !m_uriList.isEmpty();
        return m_uriList;
    }

    if (type == mimeTypeTextHTML) {
        if (m_clipboardType == Clipboard::CopyAndPaste) {
            PasteboardPrivate::ClipboardBuffer buffer =
                Pasteboard::generalPasteboard()->isSelectionMode() ?
                PasteboardPrivate::SelectionBuffer :
                PasteboardPrivate::StandardBuffer;
            String htmlText;
            KURL sourceURL;
            PlatformBridge::clipboardReadHTML(buffer, &htmlText, &sourceURL);
            success = !htmlText.isEmpty();
            return htmlText;
        }
        success = !m_textHtml.isEmpty();
        return m_textHtml;
    }

    if (type == mimeTypeDownloadURL) {
        success = !m_downloadMetadata.isEmpty();
        return m_downloadMetadata;
    }

    success = false;
    return String();
}

bool ChromiumDataObject::setData(const String& type, const String& data)
{
    if (type == mimeTypeTextPlain) {
        m_plainText = data;
        return true;
    }

    if (type == mimeTypeURL || type == mimeTypeTextURIList) {
        m_url = KURL();
        Vector<String> uriList;
        // Line separator is \r\n per RFC 2483 - however, for compatibility
        // reasons we also allow just \n here.
        data.split('\n', uriList);
        // Process the input and copy the first valid URL into the url member.
        // In case no URLs can be found, subsequent calls to getData("URL")
        // will get an empty string. This is in line with the HTML5 spec (see
        // "The DragEvent and DataTransfer interfaces").
        for (size_t i = 0; i < uriList.size(); ++i) {
            String& line = uriList[i];
            line = line.stripWhiteSpace();
            if (line.isEmpty()) {
                continue;
            }
            if (line[0] == '#')
                continue;
            KURL url = KURL(ParsedURLString, line);
            if (url.isValid()) {
                m_url = url;
                break;
            }
        }
        m_uriList = data;
        return true;
    }

    if (type == mimeTypeTextHTML) {
        m_textHtml = data;
        m_htmlBaseUrl = KURL();
        return true;
    }

    if (type == mimeTypeDownloadURL) {
        m_downloadMetadata = data;
        return true;
    }

    return false;
}

bool ChromiumDataObject::containsFilenames() const
{
    bool containsFilenames;
    if (m_clipboardType == Clipboard::CopyAndPaste) {
        HashSet<String> ignoredResults =
            PlatformBridge::clipboardReadAvailableTypes(PasteboardPrivate::StandardBuffer,
                                                        &containsFilenames);
    } else
        containsFilenames = !m_filenames.isEmpty();
    return containsFilenames;
}

ChromiumDataObject::ChromiumDataObject(Clipboard::ClipboardType clipboardType)
    : m_clipboardType(clipboardType)
{
}

ChromiumDataObject::ChromiumDataObject(const ChromiumDataObject& other)
    : RefCounted<ChromiumDataObject>()
    , m_clipboardType(other.m_clipboardType)
    , m_urlTitle(other.m_urlTitle)
    , m_downloadMetadata(other.m_downloadMetadata)
    , m_fileExtension(other.m_fileExtension)
    , m_filenames(other.m_filenames)
    , m_plainText(other.m_plainText)
    , m_textHtml(other.m_textHtml)
    , m_htmlBaseUrl(other.m_htmlBaseUrl)
    , m_fileContentFilename(other.m_fileContentFilename)
    , m_url(other.m_url)
    , m_uriList(other.m_uriList)
{
    if (other.m_fileContent.get())
        m_fileContent = other.m_fileContent->copy();
}

} // namespace WebCore

