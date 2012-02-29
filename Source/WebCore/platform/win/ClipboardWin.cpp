/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "ClipboardWin.h"

#include "CachedImage.h"
#include "ClipboardUtilitiesWin.h"
#include "Document.h"
#include "DragData.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "FileList.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformMouseEvent.h"
#include "Range.h"
#include "RenderImage.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "WCDataObject.h"
#include "markup.h"
#include <shlwapi.h>
#include <wininet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/StringHash.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

enum ClipboardDataType { ClipboardDataTypeNone, ClipboardDataTypeURL, ClipboardDataTypeText, ClipboardDataTypeTextHTML };

static ClipboardDataType clipboardTypeFromMIMEType(const String& type)
{
    String qType = type.stripWhiteSpace().lower();

    // two special cases for IE compatibility
    if (qType == "text" || qType == "text/plain" || qType.startsWith("text/plain;"))
        return ClipboardDataTypeText;
    if (qType == "url" || qType == "text/uri-list")
        return ClipboardDataTypeURL;
    if (qType == "text/html")
        return ClipboardDataTypeTextHTML;

    return ClipboardDataTypeNone;
}

#if !OS(WINCE)
static inline void pathRemoveBadFSCharacters(PWSTR psz, size_t length)
{
    size_t writeTo = 0;
    size_t readFrom = 0;
    while (readFrom < length) {
        UINT type = PathGetCharType(psz[readFrom]);
        if (!psz[readFrom] || type & (GCT_LFNCHAR | GCT_SHORTCHAR))
            psz[writeTo++] = psz[readFrom];

        readFrom++;
    }
    psz[writeTo] = 0;
}
#endif

static String filesystemPathFromUrlOrTitle(const String& url, const String& title, const UChar* extension, bool isLink)
{
#if OS(WINCE)
    notImplemented();
    return String();
#else
    static const size_t fsPathMaxLengthExcludingNullTerminator = MAX_PATH - 1;
    bool usedURL = false;
    WCHAR fsPathBuffer[MAX_PATH];
    fsPathBuffer[0] = 0;
    int extensionLen = extension ? lstrlen(extension) : 0;
    int fsPathMaxLengthExcludingExtension = fsPathMaxLengthExcludingNullTerminator - extensionLen;

    if (!title.isEmpty()) {
        size_t len = min<size_t>(title.length(), fsPathMaxLengthExcludingExtension);
        CopyMemory(fsPathBuffer, title.characters(), len * sizeof(UChar));
        fsPathBuffer[len] = 0;
        pathRemoveBadFSCharacters(fsPathBuffer, len);
    }

    if (!lstrlen(fsPathBuffer)) {
        KURL kurl(ParsedURLString, url);
        usedURL = true;
        // The filename for any content based drag or file url should be the last element of 
        // the path.  If we can't find it, or we're coming up with the name for a link
        // we just use the entire url.
        DWORD len = fsPathMaxLengthExcludingExtension;
        String lastComponent = kurl.lastPathComponent();
        if (kurl.isLocalFile() || (!isLink && !lastComponent.isEmpty())) {
            len = min<DWORD>(fsPathMaxLengthExcludingExtension, lastComponent.length());
            CopyMemory(fsPathBuffer, lastComponent.characters(), len * sizeof(UChar));
        } else {
            len = min<DWORD>(fsPathMaxLengthExcludingExtension, url.length());
            CopyMemory(fsPathBuffer, url.characters(), len * sizeof(UChar));
        }
        fsPathBuffer[len] = 0;
        pathRemoveBadFSCharacters(fsPathBuffer, len);
    }

    if (!extension)
        return String(static_cast<UChar*>(fsPathBuffer));

    if (!isLink && usedURL) {
        PathRenameExtension(fsPathBuffer, extension);
        return String(static_cast<UChar*>(fsPathBuffer));
    }

    String result(static_cast<UChar*>(fsPathBuffer));
    result += String(extension);
    return result;
#endif
}

static HGLOBAL createGlobalImageFileContent(SharedBuffer* data)
{
    HGLOBAL memObj = GlobalAlloc(GPTR, data->size());
    if (!memObj) 
        return 0;

    char* fileContents = (PSTR)GlobalLock(memObj);

    CopyMemory(fileContents, data->data(), data->size());
    
    GlobalUnlock(memObj);
    
    return memObj;
}

static HGLOBAL createGlobalHDropContent(const KURL& url, String& fileName, SharedBuffer* data)
{
    if (fileName.isEmpty() || !data)
        return 0;

    WCHAR filePath[MAX_PATH];

    if (url.isLocalFile()) {
        String localPath = decodeURLEscapeSequences(url.path());
        // windows does not enjoy a leading slash on paths
        if (localPath[0] == '/')
            localPath = localPath.substring(1);
        LPCWSTR localPathStr = localPath.charactersWithNullTermination();
        if (wcslen(localPathStr) + 1 < MAX_PATH)
            wcscpy_s(filePath, MAX_PATH, localPathStr);
        else
            return 0;
    } else {
#if OS(WINCE)
        notImplemented();
        return 0;
#else
        WCHAR tempPath[MAX_PATH];
        WCHAR extension[MAX_PATH];
        if (!::GetTempPath(WTF_ARRAY_LENGTH(tempPath), tempPath))
            return 0;
        if (!::PathAppend(tempPath, fileName.charactersWithNullTermination()))
            return 0;
        LPCWSTR foundExtension = ::PathFindExtension(tempPath);
        if (foundExtension) {
            if (wcscpy_s(extension, MAX_PATH, foundExtension))
                return 0;
        } else
            *extension = 0;
        ::PathRemoveExtension(tempPath);
        for (int i = 1; i < 10000; i++) {
            if (swprintf_s(filePath, MAX_PATH, TEXT("%s-%d%s"), tempPath, i, extension) == -1)
                return 0;
            if (!::PathFileExists(filePath))
                break;
        }
        HANDLE tempFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (tempFileHandle == INVALID_HANDLE_VALUE)
            return 0;

        // Write the data to this temp file.
        DWORD written;
        BOOL tempWriteSucceeded = WriteFile(tempFileHandle, data->data(), data->size(), &written, 0);
        CloseHandle(tempFileHandle);
        if (!tempWriteSucceeded)
            return 0;
#endif
    }

    SIZE_T dropFilesSize = sizeof(DROPFILES) + (sizeof(WCHAR) * (wcslen(filePath) + 2));
    HGLOBAL memObj = GlobalAlloc(GHND | GMEM_SHARE, dropFilesSize);
    if (!memObj) 
        return 0;

    DROPFILES* dropFiles = (DROPFILES*) GlobalLock(memObj);
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;
    wcscpy((LPWSTR)(dropFiles + 1), filePath);    
    GlobalUnlock(memObj);
    
    return memObj;
}

static HGLOBAL createGlobalImageFileDescriptor(const String& url, const String& title, CachedImage* image)
{
    ASSERT_ARG(image, image);
    ASSERT(image->image()->data());

    HRESULT hr = S_OK;
    HGLOBAL memObj = 0;
    String fsPath;
    memObj = GlobalAlloc(GPTR, sizeof(FILEGROUPDESCRIPTOR));
    if (!memObj)
        return 0;

    FILEGROUPDESCRIPTOR* fgd = (FILEGROUPDESCRIPTOR*)GlobalLock(memObj);
    memset(fgd, 0, sizeof(FILEGROUPDESCRIPTOR));
    fgd->cItems = 1;
    fgd->fgd[0].dwFlags = FD_FILESIZE;
    fgd->fgd[0].nFileSizeLow = image->image()->data()->size();
    
    const String& preferredTitle = title.isEmpty() ? image->response().suggestedFilename() : title;
    String extension = image->image()->filenameExtension();
    if (extension.isEmpty()) {
        // Do not continue processing in the rare and unusual case where a decoded image is not able 
        // to provide a filename extension. Something tricky (like a bait-n-switch) is going on
        return 0;
    }
    extension.insert(".", 0);
    fsPath = filesystemPathFromUrlOrTitle(url, preferredTitle, extension.charactersWithNullTermination(), false);

    if (fsPath.length() <= 0) {
        GlobalUnlock(memObj);
        GlobalFree(memObj);
        return 0;
    }

    int maxSize = min(fsPath.length(), WTF_ARRAY_LENGTH(fgd->fgd[0].cFileName));
    CopyMemory(fgd->fgd[0].cFileName, (LPCWSTR)fsPath.characters(), maxSize * sizeof(UChar));
    GlobalUnlock(memObj);
    
    return memObj;
}


// writeFileToDataObject takes ownership of fileDescriptor and fileContent
static HRESULT writeFileToDataObject(IDataObject* dataObject, HGLOBAL fileDescriptor, HGLOBAL fileContent, HGLOBAL hDropContent)
{
    HRESULT hr = S_OK;
    FORMATETC* fe;
    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;

    if (!fileDescriptor || !fileContent)
        goto exit;

    // Descriptor
    fe = fileDescriptorFormat();

    medium.hGlobal = fileDescriptor;

    if (FAILED(hr = dataObject->SetData(fe, &medium, TRUE)))
        goto exit;

    // Contents
    fe = fileContentFormatZero();
    medium.hGlobal = fileContent;
    if (FAILED(hr = dataObject->SetData(fe, &medium, TRUE)))
        goto exit;

#if USE(CF)
    // HDROP
    if (hDropContent) {
        medium.hGlobal = hDropContent;
        hr = dataObject->SetData(cfHDropFormat(), &medium, TRUE);
    }
#endif

exit:
    if (FAILED(hr)) {
        if (fileDescriptor)
            GlobalFree(fileDescriptor);
        if (fileContent)
            GlobalFree(fileContent);
        if (hDropContent)
            GlobalFree(hDropContent);
    }
    return hr;
}

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    if (dragData->platformData())
        return ClipboardWin::create(DragAndDrop, dragData->platformData(), policy, frame);
    return ClipboardWin::create(DragAndDrop, dragData->dragDataMap(), policy, frame);
}

ClipboardWin::ClipboardWin(ClipboardType clipboardType, IDataObject* dataObject, ClipboardAccessPolicy policy, Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(dataObject)
    , m_writableDataObject(0)
    , m_frame(frame)
{
}

ClipboardWin::ClipboardWin(ClipboardType clipboardType, WCDataObject* dataObject, ClipboardAccessPolicy policy, Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(dataObject)
    , m_writableDataObject(dataObject)
    , m_frame(frame)
{
}

ClipboardWin::ClipboardWin(ClipboardType clipboardType, const DragDataMap& dataMap, ClipboardAccessPolicy policy, Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(0)
    , m_writableDataObject(0)
    , m_frame(frame)
    , m_dragDataMap(dataMap)
{
}

ClipboardWin::~ClipboardWin()
{
}

static bool writeURL(WCDataObject *data, const KURL& url, String title, bool withPlainText, bool withHTML)
{
    ASSERT(data);

    if (url.isEmpty())
        return false;
    
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;

    medium.hGlobal = createGlobalData(url, title);
    bool success = false;
    if (medium.hGlobal && FAILED(data->SetData(urlWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);
    else
        success = true;

    if (withHTML) {
        Vector<char> cfhtmlData;
        markupToCFHTML(urlToMarkup(url, title), "", cfhtmlData);
        medium.hGlobal = createGlobalData(cfhtmlData);
        if (medium.hGlobal && FAILED(data->SetData(htmlFormat(), &medium, TRUE)))
            ::GlobalFree(medium.hGlobal);
        else
            success = true;
    }

    if (withPlainText) {
        medium.hGlobal = createGlobalData(url.string());
        if (medium.hGlobal && FAILED(data->SetData(plainTextWFormat(), &medium, TRUE)))
            ::GlobalFree(medium.hGlobal);
        else
            success = true;
    }

    return success;
}

void ClipboardWin::clearData(const String& type)
{
    // FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragAndDrop());
    if (policy() != ClipboardWritable || !m_writableDataObject)
        return;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);

    if (dataType == ClipboardDataTypeURL) {
        m_writableDataObject->clearData(urlWFormat()->cfFormat);
        m_writableDataObject->clearData(urlFormat()->cfFormat);
    }
    if (dataType == ClipboardDataTypeText) {
        m_writableDataObject->clearData(plainTextFormat()->cfFormat);
        m_writableDataObject->clearData(plainTextWFormat()->cfFormat);
    }

}

void ClipboardWin::clearAllData()
{
    // FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragAndDrop());
    if (policy() != ClipboardWritable)
        return;
    
    m_writableDataObject = 0;
    WCDataObject::createInstance(&m_writableDataObject);
    m_dataObject = m_writableDataObject;
}

String ClipboardWin::getData(const String& type) const
{     
    if (policy() != ClipboardReadable || (!m_dataObject && m_dragDataMap.isEmpty()))
        return "";

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    if (dataType == ClipboardDataTypeText)
        return m_dataObject ? getPlainText(m_dataObject.get()) : getPlainText(&m_dragDataMap);
    if (dataType == ClipboardDataTypeURL)
        return m_dataObject ? getURL(m_dataObject.get(), DragData::DoNotConvertFilenames) : getURL(&m_dragDataMap, DragData::DoNotConvertFilenames);
    else if (dataType == ClipboardDataTypeTextHTML) {
        String data = m_dataObject ? getTextHTML(m_dataObject.get()) : getTextHTML(&m_dragDataMap);
        if (!data.isEmpty())
            return data;
        return m_dataObject ? getCFHTML(m_dataObject.get()) : getCFHTML(&m_dragDataMap);
    }
    
    return "";
}

bool ClipboardWin::setData(const String& type, const String& data)
{
    // FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragAndDrop());
    if (policy() != ClipboardWritable || !m_writableDataObject)
        return false;

    ClipboardDataType winType = clipboardTypeFromMIMEType(type);

    if (winType == ClipboardDataTypeURL)
        return WebCore::writeURL(m_writableDataObject.get(), KURL(ParsedURLString, data), String(), false, true);

    if (winType == ClipboardDataTypeText) {
        STGMEDIUM medium = {0};
        medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = createGlobalData(data);
        if (!medium.hGlobal)
            return false;

        if (FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE))) {
            ::GlobalFree(medium.hGlobal);
            return false;
        }
        return true;
    }

    return false;
}

static void addMimeTypesForFormat(HashSet<String>& results, const FORMATETC& format)
{
    // URL and Text are provided for compatibility with IE's model
    if (format.cfFormat == urlFormat()->cfFormat || format.cfFormat == urlWFormat()->cfFormat) {
        results.add("URL");
        results.add("text/uri-list");
    }

    if (format.cfFormat == plainTextWFormat()->cfFormat || format.cfFormat == plainTextFormat()->cfFormat) {
        results.add("Text");
        results.add("text/plain");
    }
}

// extensions beyond IE's API
HashSet<String> ClipboardWin::types() const
{ 
    HashSet<String> results; 
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    if (!m_dataObject && m_dragDataMap.isEmpty())
        return results;

    if (m_dataObject) {
        COMPtr<IEnumFORMATETC> itr;

        if (FAILED(m_dataObject->EnumFormatEtc(DATADIR_GET, &itr)))
            return results;

        if (!itr)
            return results;

        FORMATETC data;

        // IEnumFORMATETC::Next returns S_FALSE if there are no more items.
        while (itr->Next(1, &data, 0) == S_OK)
            addMimeTypesForFormat(results, data);
    } else {
        for (DragDataMap::const_iterator it = m_dragDataMap.begin(); it != m_dragDataMap.end(); ++it) {
            FORMATETC data;
            data.cfFormat = (*it).first;
            addMimeTypesForFormat(results, data);
        }
    }

    return results;
}

PassRefPtr<FileList> ClipboardWin::files() const
{
#if OS(WINCE)
    notImplemented();
    return 0;
#else
    RefPtr<FileList> files = FileList::create();
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return files.release();

    if (!m_dataObject && m_dragDataMap.isEmpty())
        return files.release();

    if (m_dataObject) {
        STGMEDIUM medium;
        if (FAILED(m_dataObject->GetData(cfHDropFormat(), &medium)))
            return files.release();

        HDROP hdrop = reinterpret_cast<HDROP>(GlobalLock(medium.hGlobal));
        if (!hdrop)
            return files.release();

        WCHAR filename[MAX_PATH];
        UINT fileCount = DragQueryFileW(hdrop, 0xFFFFFFFF, 0, 0);
        for (UINT i = 0; i < fileCount; i++) {
            if (!DragQueryFileW(hdrop, i, filename, WTF_ARRAY_LENGTH(filename)))
                continue;
            files->append(File::create(reinterpret_cast<UChar*>(filename)));
        }

        GlobalUnlock(medium.hGlobal);
        ReleaseStgMedium(&medium);
        return files.release();
    }
    if (!m_dragDataMap.contains(cfHDropFormat()->cfFormat))
        return files.release();
    Vector<String> filesVector = m_dragDataMap.get(cfHDropFormat()->cfFormat);
    for (Vector<String>::iterator it = filesVector.begin(); it != filesVector.end(); ++it)
        files->append(File::create(*it));
    return files.release();
#endif
}

void ClipboardWin::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
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

void ClipboardWin::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardWin::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

DragImageRef ClipboardWin::createDragImage(IntPoint& loc) const
{
    HBITMAP result = 0;
    if (m_dragImage) {
        result = createDragImageFromImage(m_dragImage->image());        
        loc = m_dragLoc;
    } else if (m_dragImageElement) {
        Node* node = m_dragImageElement.get();
        result = node->document()->frame()->nodeImage(node);
        loc = m_dragLoc;
    }
    return result;
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

static void writeImageToDataObject(IDataObject* dataObject, Element* element, const KURL& url)
{
    // Shove image data into a DataObject for use as a file
    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->imageForRenderer(element->renderer()) || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->imageForRenderer(element->renderer())->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    HGLOBAL imageFileDescriptor = createGlobalImageFileDescriptor(url.string(), element->getAttribute(altAttr), cachedImage);
    if (!imageFileDescriptor)
        return;

    HGLOBAL imageFileContent = createGlobalImageFileContent(imageBuffer);
    if (!imageFileContent) {
        GlobalFree(imageFileDescriptor);
        return;
    }

    String fileName = cachedImage->response().suggestedFilename();
    HGLOBAL hDropContent = createGlobalHDropContent(url, fileName, imageBuffer);
    if (!hDropContent) {
        GlobalFree(hDropContent);
        return;
    }

    writeFileToDataObject(dataObject, imageFileDescriptor, imageFileContent, hDropContent);
}

void ClipboardWin::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    // Order is important here for Explorer's sake
    if (!m_writableDataObject)
         return;
    WebCore::writeURL(m_writableDataObject.get(), url, title, true, false);

    writeImageToDataObject(m_writableDataObject.get(), element, url);

    AtomicString imageURL = element->getAttribute(srcAttr);
    if (imageURL.isEmpty()) 
        return;

    KURL fullURL = frame->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(imageURL));
    if (fullURL.isEmpty()) 
        return;
    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;

    // Put img tag on the clipboard referencing the image
    Vector<char> data;
    markupToCFHTML(createMarkup(element, IncludeNode, 0, ResolveAllURLs), "", data);
    medium.hGlobal = createGlobalData(data);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);
}

void ClipboardWin::writeURL(const KURL& kurl, const String& titleStr, Frame*)
{
    if (!m_writableDataObject)
         return;
    WebCore::writeURL(m_writableDataObject.get(), kurl, titleStr, true, true);

    String url = kurl.string();
    ASSERT(url.containsOnlyASCII()); // KURL::string() is URL encoded.

    String fsPath = filesystemPathFromUrlOrTitle(url, titleStr, L".URL", true);
    String contentString("[InternetShortcut]\r\nURL=" + url + "\r\n");
    CString content = contentString.latin1();

    if (fsPath.length() <= 0)
        return;

    HGLOBAL urlFileDescriptor = GlobalAlloc(GPTR, sizeof(FILEGROUPDESCRIPTOR));
    if (!urlFileDescriptor)
        return;

    HGLOBAL urlFileContent = GlobalAlloc(GPTR, content.length());
    if (!urlFileContent) {
        GlobalFree(urlFileDescriptor);
        return;
    }

    FILEGROUPDESCRIPTOR* fgd = static_cast<FILEGROUPDESCRIPTOR*>(GlobalLock(urlFileDescriptor));
    ZeroMemory(fgd, sizeof(FILEGROUPDESCRIPTOR));
    fgd->cItems = 1;
    fgd->fgd[0].dwFlags = FD_FILESIZE;
    fgd->fgd[0].nFileSizeLow = content.length();

    unsigned maxSize = min(fsPath.length(), WTF_ARRAY_LENGTH(fgd->fgd[0].cFileName));
    CopyMemory(fgd->fgd[0].cFileName, fsPath.characters(), maxSize * sizeof(UChar));
    GlobalUnlock(urlFileDescriptor);

    char* fileContents = static_cast<char*>(GlobalLock(urlFileContent));
    CopyMemory(fileContents, content.data(), content.length());
    GlobalUnlock(urlFileContent);

    writeFileToDataObject(m_writableDataObject.get(), urlFileDescriptor, urlFileContent, 0);
}

void ClipboardWin::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_writableDataObject)
         return;

    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;
    ExceptionCode ec = 0;

    Vector<char> data;
    markupToCFHTML(createMarkup(selectedRange, 0, AnnotateForInterchange),
        selectedRange->startContainer(ec)->document()->url().string(), data);
    medium.hGlobal = createGlobalData(data);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

    String str = frame->editor()->selectedText();
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    medium.hGlobal = createGlobalData(str);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

    medium.hGlobal = 0;
    if (frame->editor()->canSmartCopyOrDelete())
        m_writableDataObject->SetData(smartPasteFormat(), &medium, TRUE);
}

void ClipboardWin::writePlainText(const String& text)
{
    if (!m_writableDataObject)
        return;
    
    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;
    
    String str = text;
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    medium.hGlobal = createGlobalData(str);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);        

    medium.hGlobal = 0;
}
    
bool ClipboardWin::hasData()
{
    if (!m_dataObject && m_dragDataMap.isEmpty())
        return false;

    if (m_dataObject) {
        COMPtr<IEnumFORMATETC> itr;
        if (FAILED(m_dataObject->EnumFormatEtc(DATADIR_GET, &itr)))
            return false;

        if (!itr)
            return false;

        FORMATETC data;

        // IEnumFORMATETC::Next returns S_FALSE if there are no more items.
        if (itr->Next(1, &data, 0) == S_OK) {
            // There is at least one item in the IDataObject
            return true;
        }

        return false;
    }
    return !m_dragDataMap.isEmpty();
}

void ClipboardWin::setExternalDataObject(IDataObject *dataObject)
{
    ASSERT(isForDragAndDrop());

    m_writableDataObject = 0;
    m_dataObject = dataObject;
}

} // namespace WebCore
