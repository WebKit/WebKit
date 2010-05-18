/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ClipboardChromium.h"

#include "CachedImage.h"
#include "ChromiumBridge.h"
#include "ChromiumDataObject.h"
#include "ClipboardUtilitiesChromium.h"
#include "Document.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "NamedNodeMap.h"
#include "Pasteboard.h"
#include "PlatformString.h"
#include "Range.h"
#include "RenderImage.h"
#include "StringBuilder.h"
#include "markup.h"

namespace WebCore {

using namespace HTMLNames;

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

enum ClipboardDataType {
    ClipboardDataTypeNone,

    ClipboardDataTypeURL,
    ClipboardDataTypeURIList,
    ClipboardDataTypeDownloadURL,
    ClipboardDataTypePlainText,
    ClipboardDataTypeHTML,

    ClipboardDataTypeOther,
};

// Per RFC 2483, the line separator for "text/..." MIME types is CR-LF.
static char const* const textMIMETypeLineSeparator = "\r\n";

static ClipboardDataType clipboardTypeFromMIMEType(const String& type)
{
    String cleanType = type.stripWhiteSpace().lower();
    if (cleanType.isEmpty())
        return ClipboardDataTypeNone;

    // Includes two special cases for IE compatibility.
    if (cleanType == "text" || cleanType == "text/plain" || cleanType.startsWith("text/plain;"))
        return ClipboardDataTypePlainText;
    if (cleanType == "url")
        return ClipboardDataTypeURL;
    if (cleanType == "text/uri-list")
        return ClipboardDataTypeURIList;
    if (cleanType == "downloadurl")
        return ClipboardDataTypeDownloadURL;
    if (cleanType == "text/html")
        return ClipboardDataTypeHTML;

    return ClipboardDataTypeOther;
}

ClipboardChromium::ClipboardChromium(bool isForDragging,
                                     PassRefPtr<ChromiumDataObject> dataObject,
                                     ClipboardAccessPolicy policy)
    : Clipboard(policy, isForDragging)
    , m_dataObject(dataObject)
{
}

PassRefPtr<ClipboardChromium> ClipboardChromium::create(bool isForDragging,
    PassRefPtr<ChromiumDataObject> dataObject, ClipboardAccessPolicy policy)
{
    return adoptRef(new ClipboardChromium(isForDragging, dataObject, policy));
}

void ClipboardChromium::clearData(const String& type)
{
    if (policy() != ClipboardWritable || !m_dataObject)
        return;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    switch (dataType) {
    case ClipboardDataTypeNone:
        // If called with no arguments, everything except the file list must be cleared.
        // (See HTML5 spec, "The DragEvent and DataTransfer interfaces")
        m_dataObject->clearAllExceptFiles();
        return;

    case ClipboardDataTypeURL:
    case ClipboardDataTypeURIList:
        m_dataObject->clearURL();
        return;

    case ClipboardDataTypeDownloadURL:
        m_dataObject->downloadMetadata = "";
        return;
        
    case ClipboardDataTypePlainText:
        m_dataObject->plainText = "";
        return;

    case ClipboardDataTypeHTML:
        m_dataObject->textHtml = "";
        m_dataObject->htmlBaseUrl = KURL();
        return;

    case ClipboardDataTypeOther:
        // Not yet implemented, see https://bugs.webkit.org/show_bug.cgi?id=34410
        return;
    }

    ASSERT_NOT_REACHED();
}

void ClipboardChromium::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    m_dataObject->clear();
}

String ClipboardChromium::getData(const String& type, bool& success) const
{
    success = false;
    if (policy() != ClipboardReadable || !m_dataObject)
        return String();

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    switch (dataType) {
    case ClipboardDataTypeNone:
        return String();

    // Hack for URLs. file URLs are used internally for drop's default action, but we don't want
    // to expose them to the page, so we filter them out here.
    case ClipboardDataTypeURIList:
        {
            String text;
            for (size_t i = 0; i < m_dataObject->uriList.size(); ++i) {
                const String& uri = m_dataObject->uriList[i];
                if (protocolIs(uri, "file"))
                    continue;
                ASSERT(!uri.isEmpty());
                if (!text.isEmpty())
                    text.append(textMIMETypeLineSeparator);
                // URIs have already been canonicalized, so copy everything verbatim.
                text.append(uri);
            }
            success = !text.isEmpty();
            return text;
        }

    case ClipboardDataTypeURL:
        // In case of a previous setData('text/uri-list'), setData() has already
        // prepared the 'url' member, so we can just retrieve it here.
        if (!m_dataObject->url.isEmpty() && !m_dataObject->url.isLocalFile()) {
            success = true;
            return m_dataObject->url.string();
        }
        return String();

    case ClipboardDataTypeDownloadURL:
        success = !m_dataObject->downloadMetadata.isEmpty();
        return m_dataObject->downloadMetadata;
    
    case ClipboardDataTypePlainText:
        if (!isForDragging()) {
            // If this isn't for a drag, it's for a cut/paste event handler.
            // In this case, we need to check the clipboard.
            PasteboardPrivate::ClipboardBuffer buffer = 
                Pasteboard::generalPasteboard()->isSelectionMode() ?
                PasteboardPrivate::SelectionBuffer : 
                PasteboardPrivate::StandardBuffer;
            String text = ChromiumBridge::clipboardReadPlainText(buffer);
            success = !text.isEmpty();
            return text;
        }
        // Otherwise return whatever is stored in plainText.
        success = !m_dataObject->plainText.isEmpty();
        return m_dataObject->plainText;

    case ClipboardDataTypeHTML:
        if (!isForDragging()) {
            // If this isn't for a drag, it's for a cut/paste event handler.
            // In this case, we need to check the clipboard.
            PasteboardPrivate::ClipboardBuffer buffer = 
                Pasteboard::generalPasteboard()->isSelectionMode() ?
                PasteboardPrivate::SelectionBuffer : 
                PasteboardPrivate::StandardBuffer;
            String htmlText;
            KURL sourceURL;
            ChromiumBridge::clipboardReadHTML(buffer, &htmlText, &sourceURL);
            success = !htmlText.isEmpty();
            return htmlText;
        }
        // Otherwise return whatever is stored in textHtml.
        success = !m_dataObject->textHtml.isEmpty();
        return m_dataObject->textHtml;

    case ClipboardDataTypeOther:
        // not yet implemented, see https://bugs.webkit.org/show_bug.cgi?id=34410
        return String();
    }

    ASSERT_NOT_REACHED();
    return String();
}

bool ClipboardChromium::setData(const String& type, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    switch (dataType) {
    case ClipboardDataTypeNone:
        return false;

    case ClipboardDataTypeURL:
        // For setData(), "URL" must be treated as "text/uri-list".
        // (See HTML5 spec, "The DragEvent and DataTransfer interfaces")
    case ClipboardDataTypeURIList:
        m_dataObject->url = KURL();
        // Line separator is \r\n per RFC 2483 - however, for compatibility reasons
        // we also allow just \n here. 
        data.split('\n', m_dataObject->uriList);
        // Strip white space on all lines, including trailing \r from above split.
        // If this leaves a line empty, remove it completely.
        //
        // Also, copy the first valid URL into the 'url' member as well.
        // In case no entry is a valid URL (i.e., remarks only), then we leave 'url' empty.
        // I.e., in that case subsequent calls to getData("URL") will get an empty string.
        // This is in line with the HTML5 spec (see "The DragEvent and DataTransfer interfaces").
        for (size_t i = 0; i < m_dataObject->uriList.size(); /**/) {
            String& line = m_dataObject->uriList[i];
            line = line.stripWhiteSpace();
            if (line.isEmpty()) {
                m_dataObject->uriList.remove(i);
                continue;
            }
            ++i;
            // Only copy the first valid URL.
            if (m_dataObject->url.isValid())
                continue;
            // Skip remarks.
            if (line[0] == '#')
                continue;
            KURL url = KURL(ParsedURLString, line);
            if (url.isValid())
                m_dataObject->url = url;
        }
        if (m_dataObject->uriList.isEmpty()) {
            ASSERT(m_dataObject->url.isEmpty());
            return data.isEmpty();
        }
        return true;

    case ClipboardDataTypeDownloadURL:
        m_dataObject->downloadMetadata = data;
        return true;

    case ClipboardDataTypePlainText:
        m_dataObject->plainText = data;
        return true;

    case ClipboardDataTypeHTML:
        m_dataObject->textHtml = data;
        return true;

    case ClipboardDataTypeOther:
        // Not yet implemented, see https://bugs.webkit.org/show_bug.cgi?id=34410
        return false;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

// extensions beyond IE's API
HashSet<String> ClipboardChromium::types() const
{
    HashSet<String> results;
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    if (!m_dataObject)
        return results;

    if (!m_dataObject->filenames.isEmpty())
        results.add("Files");

    // Hack for URLs. file URLs are used internally for drop's default action, but we don't want
    // to expose them to the page, so we filter them out here.
    if (m_dataObject->url.isValid() && !m_dataObject->url.isLocalFile()) {
        ASSERT(!m_dataObject->uriList.isEmpty());
        results.add("URL");
    }

    if (!m_dataObject->uriList.isEmpty()) {
        // Verify that the URI list contains at least one non-file URL.
        for (Vector<String>::const_iterator it = m_dataObject->uriList.begin();
             it != m_dataObject->uriList.end(); ++it) {
            if (!protocolIs(*it, "file")) {
                // Note that even if the URI list is not empty, it may not actually
                // contain a valid URL, so we can't return "URL" here.
                results.add("text/uri-list");
                break;
            }
        }
    }

    if (!m_dataObject->plainText.isEmpty()) {
        results.add("Text");
        results.add("text/plain");
    }

    return results;
}

PassRefPtr<FileList> ClipboardChromium::files() const
{
    if (policy() != ClipboardReadable)
        return FileList::create();

    if (!m_dataObject || m_dataObject->filenames.isEmpty())
        return FileList::create();

    RefPtr<FileList> fileList = FileList::create();
    for (size_t i = 0; i < m_dataObject->filenames.size(); ++i)
        fileList->append(File::create(m_dataObject->filenames.at(i)));

    return fileList.release();
}

void ClipboardChromium::setDragImage(CachedImage* image, Node* node, const IntPoint& loc)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable)
        return;

    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

void ClipboardChromium::setDragImage(CachedImage* img, const IntPoint& loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardChromium::setDragImageElement(Node* node, const IntPoint& loc)
{
    setDragImage(0, node, loc);
}

DragImageRef ClipboardChromium::createDragImage(IntPoint& loc) const
{
    DragImageRef result = 0;
    if (m_dragImage) {
        result = createDragImageFromImage(m_dragImage->image());
        loc = m_dragLoc;
    }
    return result;
}

static String imageToMarkup(const String& url, Element* element)
{
    StringBuilder markup;
    markup.append("<img src=\"");
    markup.append(url);
    markup.append("\"");
    // Copy over attributes.  If we are dragging an image, we expect things like
    // the id to be copied as well.
    NamedNodeMap* attrs = element->attributes();
    unsigned length = attrs->length();
    for (unsigned i = 0; i < length; ++i) {
        Attribute* attr = attrs->attributeItem(i);
        if (attr->localName() == "src")
            continue;
        markup.append(" ");
        markup.append(attr->localName());
        markup.append("=\"");
        String escapedAttr = attr->value();
        escapedAttr.replace("\"", "&quot;");
        markup.append(escapedAttr);
        markup.append("\"");
    }

    markup.append("/>");
    return markup.toString();
}

static CachedImage* getCachedImage(Element* element)
{
    // Attempt to pull CachedImage from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = toRenderImage(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

static void writeImageToDataObject(ChromiumDataObject* dataObject, Element* element,
                                   const KURL& url)
{
    // Shove image data into a DataObject for use as a file
    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->image() || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->image()->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    dataObject->fileContent = imageBuffer;

    // Determine the filename for the file contents of the image.  We try to
    // use the alt tag if one exists, otherwise we fall back on the suggested
    // filename in the http header, and finally we resort to using the filename
    // in the URL.
    String extension = MIMETypeRegistry::getPreferredExtensionForMIMEType(
        cachedImage->response().mimeType());
    dataObject->fileExtension = extension.isEmpty() ? "" : "." + extension;
    String title = element->getAttribute(altAttr);
    if (title.isEmpty())
        title = cachedImage->response().suggestedFilename();

    title = ClipboardChromium::validateFileName(title, dataObject);
    dataObject->fileContentFilename = title + dataObject->fileExtension;
}

void ClipboardChromium::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    if (!m_dataObject)
        return;

    m_dataObject->url = url;
    m_dataObject->urlTitle = title;

    // Write the bytes in the image to the file format.
    writeImageToDataObject(m_dataObject.get(), element, url);

    AtomicString imageURL = element->getAttribute(srcAttr);
    if (imageURL.isEmpty())
        return;

    String fullURL = frame->document()->completeURL(deprecatedParseURL(imageURL));
    if (fullURL.isEmpty())
        return;

    // Put img tag on the clipboard referencing the image
    m_dataObject->textHtml = imageToMarkup(fullURL, element);
}

void ClipboardChromium::writeURL(const KURL& url, const String& title, Frame*)
{
    if (!m_dataObject)
        return;
    m_dataObject->url = url;
    m_dataObject->urlTitle = title;

    // The URL can also be used as plain text.
    m_dataObject->plainText = url.string();

    // The URL can also be used as an HTML fragment.
    m_dataObject->textHtml = urlToMarkup(url, title);
    m_dataObject->htmlBaseUrl = url;
}

void ClipboardChromium::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_dataObject)
         return;

    m_dataObject->textHtml = createMarkup(selectedRange, 0,
        AnnotateForInterchange);
    m_dataObject->htmlBaseUrl = frame->document()->url();

    String str = frame->selectedText();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->plainText = str;
}

void ClipboardChromium::writePlainText(const String& text)
{
    if (!m_dataObject)
        return;

    String str = text;
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->plainText = str;
}

bool ClipboardChromium::hasData()
{
    if (!m_dataObject)
        return false;

    return m_dataObject->hasData();
}

} // namespace WebCore
