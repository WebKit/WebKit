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
#include "ChromiumDataObject.h"
#include "ClipboardMimeTypes.h"
#include "ClipboardUtilitiesChromium.h"
#include "DataTransferItemsChromium.h"
#include "Document.h"
#include "DragData.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "NamedNodeMap.h"
#include "Range.h"
#include "RenderImage.h"
#include "ScriptExecutionContext.h"
#include "markup.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

static String normalizeType(const String& type)
{
    String cleanType = type.stripWhiteSpace().lower();
    if (cleanType == mimeTypeText || cleanType.startsWith(mimeTypeTextPlainEtc))
        return mimeTypeTextPlain;
    return cleanType;
}

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardChromium::create(DragAndDrop, dragData->platformData(), policy, frame);
}

ClipboardChromium::ClipboardChromium(ClipboardType clipboardType,
                                     PassRefPtr<ChromiumDataObject> dataObject,
                                     ClipboardAccessPolicy policy,
                                     Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(dataObject)
    , m_frame(frame)
{
}

PassRefPtr<ClipboardChromium> ClipboardChromium::create(ClipboardType clipboardType,
    PassRefPtr<ChromiumDataObject> dataObject, ClipboardAccessPolicy policy, Frame* frame)
{
    return adoptRef(new ClipboardChromium(clipboardType, dataObject, policy, frame));
}

void ClipboardChromium::clearData(const String& type)
{
    if (policy() != ClipboardWritable || !m_dataObject)
        return;

    m_dataObject->clearData(normalizeType(type));

    ASSERT_NOT_REACHED();
}

void ClipboardChromium::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    m_dataObject->clearAll();
}

String ClipboardChromium::getData(const String& type, bool& success) const
{
    success = false;
    if (policy() != ClipboardReadable || !m_dataObject)
        return String();

    return m_dataObject->getData(normalizeType(type), success);
}

bool ClipboardChromium::setData(const String& type, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;

    return m_dataObject->setData(normalizeType(type), data);
}

// extensions beyond IE's API
HashSet<String> ClipboardChromium::types() const
{
    HashSet<String> results;
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    if (!m_dataObject)
        return results;

    results = m_dataObject->types();

    if (m_dataObject->containsFilenames())
        results.add(mimeTypeFiles);

    return results;
}

PassRefPtr<FileList> ClipboardChromium::files() const
{
    if (policy() != ClipboardReadable)
        return FileList::create();

    if (!m_dataObject)
        return FileList::create();

    const Vector<String>& filenames = m_dataObject->filenames();
    RefPtr<FileList> fileList = FileList::create();
    for (size_t i = 0; i < filenames.size(); ++i)
        fileList->append(File::create(filenames.at(i)));

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
    if (m_dragImageElement) {
        if (m_frame) {
            result = m_frame->nodeImage(m_dragImageElement.get());
            loc = m_dragLoc;
        }
    } else if (m_dragImage) {
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
    markup.append('"');
    // Copy over attributes.  If we are dragging an image, we expect things like
    // the id to be copied as well.
    NamedNodeMap* attrs = element->attributes();
    unsigned length = attrs->length();
    for (unsigned i = 0; i < length; ++i) {
        Attribute* attr = attrs->attributeItem(i);
        if (attr->localName() == "src")
            continue;
        markup.append(' ');
        markup.append(attr->localName());
        markup.append("=\"");
        String escapedAttr = attr->value();
        escapedAttr.replace("\"", "&quot;");
        markup.append(escapedAttr);
        markup.append('"');
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

    dataObject->setFileContent(imageBuffer);

    // Determine the filename for the file contents of the image.  We try to
    // use the alt tag if one exists, otherwise we fall back on the suggested
    // filename in the http header, and finally we resort to using the filename
    // in the URL.
    String extension = MIMETypeRegistry::getPreferredExtensionForMIMEType(
        cachedImage->response().mimeType());
    dataObject->setFileExtension(extension.isEmpty() ? "" : "." + extension);
    String title = element->getAttribute(altAttr);
    if (title.isEmpty())
        title = cachedImage->response().suggestedFilename();

    title = ClipboardChromium::validateFileName(title, dataObject);
    dataObject->setFileContentFilename(title + dataObject->fileExtension());
}

void ClipboardChromium::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    if (!m_dataObject)
        return;

    m_dataObject->setData(mimeTypeURL, url);
    m_dataObject->setUrlTitle(title);

    // Write the bytes in the image to the file format.
    writeImageToDataObject(m_dataObject.get(), element, url);

    AtomicString imageURL = element->getAttribute(srcAttr);
    if (imageURL.isEmpty())
        return;

    String fullURL = frame->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(imageURL));
    if (fullURL.isEmpty())
        return;

    // Put img tag on the clipboard referencing the image
    m_dataObject->setData(mimeTypeTextHTML, imageToMarkup(fullURL, element));
}

void ClipboardChromium::writeURL(const KURL& url, const String& title, Frame*)
{
    if (!m_dataObject)
        return;
    ASSERT(!url.isEmpty());
    m_dataObject->setData(mimeTypeURL, url);
    m_dataObject->setUrlTitle(title);

    // The URL can also be used as plain text.
    m_dataObject->setData(mimeTypeTextPlain, url.string());

    // The URL can also be used as an HTML fragment.
    m_dataObject->setData(mimeTypeTextHTML, urlToMarkup(url, title));
    m_dataObject->setHtmlBaseUrl(url);
}

void ClipboardChromium::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_dataObject)
         return;

    m_dataObject->setData(mimeTypeTextHTML, createMarkup(selectedRange, 0, AnnotateForInterchange, false, AbsoluteURLs));
    m_dataObject->setHtmlBaseUrl(frame->document()->url());

    String str = frame->editor()->selectedText();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->setData(mimeTypeTextPlain, str);
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
    m_dataObject->setData(mimeTypeTextPlain, str);
}

bool ClipboardChromium::hasData()
{
    if (!m_dataObject)
        return false;

    return m_dataObject->hasData();
}

#if ENABLE(DATA_TRANSFER_ITEMS)
PassRefPtr<DataTransferItems> ClipboardChromium::items()
{
    RefPtr<DataTransferItemsChromium> items = DataTransferItemsChromium::create(this, m_frame->document()->scriptExecutionContext());

    if (!m_dataObject)
        return items;

    if (isForCopyAndPaste() && policy() == ClipboardReadable) {
        // Iterate through the types and add them.
        HashSet<String> types = m_dataObject->types();
        for (HashSet<String>::const_iterator it = types.begin(); it != types.end(); ++it)
            items->addPasteboardItem(*it);
    }
    return items;
}
#endif

} // namespace WebCore
