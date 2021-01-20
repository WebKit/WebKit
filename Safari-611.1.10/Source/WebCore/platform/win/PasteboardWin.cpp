/*
 * Copyright (C) 2006, 2007, 2013-2014 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Xueqing Huang <huangxueqing@baidu.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Pasteboard.h"

#include "BitmapInfo.h"
#include "CachedImage.h"
#include "ClipboardUtilitiesWin.h"
#include "Color.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HWndDC.h"
#include "HitTestResult.h"
#include "Image.h"
#include "NotImplemented.h"
#include "Range.h"
#include "RenderImage.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "WebCoreInstanceHandle.h"
#include "markup.h"
#include <wtf/Optional.h>
#include <wtf/URL.h>
#include <wtf/WindowsExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>
#include <wtf/text/win/WCharStringExtras.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

static UINT HTMLClipboardFormat = 0;
static UINT BookmarkClipboardFormat = 0;
static UINT WebSmartPasteFormat = 0;
static UINT CustomDataClipboardFormat = 0;

static LRESULT CALLBACK PasteboardOwnerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lresult = 0;

    switch (message) {
    case WM_RENDERFORMAT:
        // This message comes when SetClipboardData was sent a null data handle 
        // and now it's come time to put the data on the clipboard.
        break;
    case WM_RENDERALLFORMATS:
        // This message comes when SetClipboardData was sent a null data handle
        // and now this application is about to quit, so it must put data on 
        // the clipboard before it exits.
        break;
    case WM_DESTROY:
        break;
    case WM_DRAWCLIPBOARD:
        break;
    case WM_CHANGECBCHAIN:
        break;
    default:
        lresult = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }
    return lresult;
}

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    auto pasteboard = makeUnique<Pasteboard>();
    COMPtr<IDataObject> clipboardData;
    if (!SUCCEEDED(OleGetClipboard(&clipboardData)))
        clipboardData = 0;
    pasteboard->setExternalDataObject(clipboardData.get());
    return pasteboard;
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    COMPtr<WCDataObject> dataObject;
    WCDataObject::createInstance(&dataObject);
    return makeUnique<Pasteboard>(dataObject.get());
}

// static
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    if (dragData.platformData())
        return makeUnique<Pasteboard>(dragData.platformData());
    // FIXME: Should add a const overload of dragDataMap so we don't need a const_cast here.
    return makeUnique<Pasteboard>(const_cast<DragData&>(dragData).dragDataMap());
}
#endif

void Pasteboard::finishCreatingPasteboard()
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(WNDCLASS));
    wc.lpfnWndProc    = PasteboardOwnerWndProc;
    wc.hInstance      = WebCore::instanceHandle();
    wc.lpszClassName  = L"PasteboardOwnerWindowClass";
    RegisterClass(&wc);

    m_owner = ::CreateWindow(L"PasteboardOwnerWindowClass", L"PasteboardOwnerWindow", 0, 0, 0, 0, 0,
        HWND_MESSAGE, 0, 0, 0);

    HTMLClipboardFormat = ::RegisterClipboardFormat(L"HTML Format");
    BookmarkClipboardFormat = ::RegisterClipboardFormat(L"UniformResourceLocatorW");
    WebSmartPasteFormat = ::RegisterClipboardFormat(L"WebKit Smart Paste Format");
    CustomDataClipboardFormat = ::RegisterClipboardFormat(L"WebKit Custom Data Format");
}

Pasteboard::Pasteboard()
    : m_dataObject(0)
    , m_writableDataObject(0)
{
    finishCreatingPasteboard();
}

Pasteboard::Pasteboard(IDataObject* dataObject)
    : m_dataObject(dataObject)
    , m_writableDataObject(0)
{
    finishCreatingPasteboard();
}

Pasteboard::Pasteboard(WCDataObject* dataObject)
    : m_dataObject(dataObject)
    , m_writableDataObject(dataObject)
{
    finishCreatingPasteboard();
}

Pasteboard::Pasteboard(const DragDataMap& dataMap)
    : m_dataObject(0)
    , m_writableDataObject(0)
    , m_dragDataMap(dataMap)
{
    finishCreatingPasteboard();
}

void Pasteboard::clear()
{
    if (::OpenClipboard(m_owner)) {
        ::EmptyClipboard();
        ::CloseClipboard();
    }
}

enum ClipboardDataType { ClipboardDataTypeNone, ClipboardDataTypeURL, ClipboardDataTypeText, ClipboardDataTypeTextHTML };

static ClipboardDataType clipboardTypeFromMIMEType(const String& type)
{
    // two special cases for IE compatibility
    if (equalLettersIgnoringASCIICase(type, "text/plain"))
        return ClipboardDataTypeText;
    if (equalLettersIgnoringASCIICase(type, "text/uri-list"))
        return ClipboardDataTypeURL;
    if (equalLettersIgnoringASCIICase(type, "text/html"))
        return ClipboardDataTypeTextHTML;

    return ClipboardDataTypeNone;
}

void Pasteboard::clear(const String& type)
{
    if (!m_writableDataObject)
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

bool Pasteboard::hasData()
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

static void addMimeTypesForFormat(ListHashSet<String>& results, const FORMATETC& format)
{
    if (format.cfFormat == urlFormat()->cfFormat || format.cfFormat == urlWFormat()->cfFormat)
        results.add("text/uri-list");
    if (format.cfFormat == plainTextWFormat()->cfFormat || format.cfFormat == plainTextFormat()->cfFormat)
        results.add("text/plain");
}

Optional<PasteboardCustomData> Pasteboard::readPasteboardCustomData()
{
    if (::IsClipboardFormatAvailable(CustomDataClipboardFormat) && ::OpenClipboard(m_owner)) {
        if (HANDLE cbData = ::GetClipboardData(CustomDataClipboardFormat)) {
            size_t size = GlobalSize(cbData);
            auto data = static_cast<uint8_t*>(GlobalLock(cbData));
            auto customData = PasteboardCustomData::fromPersistenceDecoder({data, size});

            GlobalUnlock(cbData);
            ::CloseClipboard();

            return customData;
        }
        ::CloseClipboard();
    }

    return WTF::nullopt;
}

Vector<String> Pasteboard::typesSafeForBindings(const String& origin)
{
    ListHashSet<String> domPasteboardTypes;

    Optional<PasteboardCustomData> customData = readPasteboardCustomData();

    if (customData && customData->origin() == origin) {
        for (const auto& type : customData->orderedTypes())
            domPasteboardTypes.add(type);
    }

    domPasteboardTypes.add("text/plain");
    domPasteboardTypes.add("text/uri-list");
    domPasteboardTypes.add("text/html");

    return copyToVector(domPasteboardTypes);
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    ListHashSet<String> results;

    if (!m_dataObject && m_dragDataMap.isEmpty())
        return Vector<String>();

    if (m_dataObject) {
        COMPtr<IEnumFORMATETC> itr;

        if (FAILED(m_dataObject->EnumFormatEtc(DATADIR_GET, &itr)))
            return Vector<String>();

        if (!itr)
            return Vector<String>();

        FORMATETC data;

        // IEnumFORMATETC::Next returns S_FALSE if there are no more items.
        while (itr->Next(1, &data, 0) == S_OK)
            addMimeTypesForFormat(results, data);
    } else {
        for (DragDataMap::const_iterator it = m_dragDataMap.begin(); it != m_dragDataMap.end(); ++it) {
            FORMATETC data;
            data.cfFormat = (*it).key;
            addMimeTypesForFormat(results, data);
        }
    }

    return copyToVector(results);
}

String Pasteboard::readOrigin()
{
    Optional<PasteboardCustomData> customData = readPasteboardCustomData();

    if (customData)
        return customData->origin();

    return { };
}

String Pasteboard::readString(const String& type)
{
    if (!m_dataObject && m_dragDataMap.isEmpty())
        return "";

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    if (dataType == ClipboardDataTypeText)
        return m_dataObject ? getPlainText(m_dataObject.get()) : getPlainText(&m_dragDataMap);
    if (dataType == ClipboardDataTypeURL)
        return m_dataObject ? getURL(m_dataObject.get(), DragData::DoNotConvertFilenames) : getURL(&m_dragDataMap, DragData::DoNotConvertFilenames);
    if (dataType == ClipboardDataTypeTextHTML) {
        String data = m_dataObject ? getTextHTML(m_dataObject.get()) : getTextHTML(&m_dragDataMap);
        if (!data.isEmpty())
            return data;
        return m_dataObject ? getCFHTML(m_dataObject.get()) : getCFHTML(&m_dragDataMap);
    }

    return "";
}

String Pasteboard::readStringInCustomData(const String& type)
{
    Optional<PasteboardCustomData> customData = readPasteboardCustomData();

    if (customData)
        return customData->readStringInCustomData(type);

    return { };
}

struct PasteboardFileCounter final : PasteboardFileReader {
    void readFilename(const String&) final { ++count; }
    void readBuffer(const String&, const String&, Ref<SharedBuffer>&&) final { ++count; }

    unsigned count { 0 };
};

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    // FIXME: This implementation can be slightly more efficient by avoiding calls to DragQueryFileW.
    PasteboardFileCounter reader;
    read(reader);
    return reader.count ? FileContentState::MayContainFilePaths : FileContentState::NoFileOrImageData;
}

void Pasteboard::read(PasteboardFileReader& reader, Optional<size_t>)
{
#if USE(CF)
    if (m_dataObject) {
        STGMEDIUM medium;
        if (FAILED(m_dataObject->GetData(cfHDropFormat(), &medium)))
            return;

        HDROP hdrop = reinterpret_cast<HDROP>(GlobalLock(medium.hGlobal));
        if (!hdrop)
            return;

        WCHAR filename[MAX_PATH];
        UINT fileCount = DragQueryFileW(hdrop, 0xFFFFFFFF, 0, 0);
        for (UINT i = 0; i < fileCount; i++) {
            if (!DragQueryFileW(hdrop, i, filename, WTF_ARRAY_LENGTH(filename)))
                continue;
            reader.readFilename(filename);
        }

        GlobalUnlock(medium.hGlobal);
        ReleaseStgMedium(&medium);
        return;
    }
    auto list = m_dragDataMap.find(cfHDropFormat()->cfFormat);
    if (list == m_dragDataMap.end())
        return;

    for (auto& filename : list->value)
        reader.readFilename(filename);
#else
    UNUSED_PARAM(reader);
    notImplemented();
    return;
#endif
}

static bool writeURL(WCDataObject *data, const URL& url, String title, bool withPlainText, bool withHTML)
{
    ASSERT(data);

    if (url.isEmpty())
        return false;

    if (title.isEmpty()) {
        title = url.lastPathComponent().toString();
        if (title.isEmpty())
            title = url.host().toString();
    }

    STGMEDIUM medium { };
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

void Pasteboard::writeString(const String& type, const String& data)
{
    if (!m_writableDataObject)
        return;

    ClipboardDataType winType = clipboardTypeFromMIMEType(type);

    if (winType == ClipboardDataTypeURL) {
        WebCore::writeURL(m_writableDataObject.get(), URL(URL(), data), String(), false, true);
        return;
    }

    if (winType == ClipboardDataTypeText) {
        STGMEDIUM medium { };
        medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = createGlobalData(data);
        if (!medium.hGlobal)
            return;

        if (FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
            ::GlobalFree(medium.hGlobal);
    }
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
    // Do nothing in Windows.
}
#endif

void Pasteboard::writeRangeToDataObject(const SimpleRange& selectedRange, Frame& frame)
{
    if (!m_writableDataObject)
        return;

    STGMEDIUM medium { };
    medium.tymed = TYMED_HGLOBAL;

    Vector<char> data;
    markupToCFHTML(serializePreservingVisualAppearance(selectedRange, nullptr, AnnotateForInterchange::Yes),
        selectedRange.start.container->document().url().string(), data);
    medium.hGlobal = createGlobalData(data);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

    String str = frame.editor().selectedTextForDataTransfer();
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    medium.hGlobal = createGlobalData(str);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

    medium.hGlobal = 0;
    if (frame.editor().canSmartCopyOrDelete())
        m_writableDataObject->SetData(smartPasteFormat(), &medium, TRUE);
}

void Pasteboard::writeSelection(const SimpleRange& selectedRange, bool canSmartCopyOrDelete, Frame& frame, ShouldSerializeSelectedTextForDataTransfer shouldSerializeSelectedTextForDataTransfer)
{
    clear();

    // Put CF_HTML format on the pasteboard 
    if (::OpenClipboard(m_owner)) {
        Vector<char> data;
        // FIXME: Use ResolveURLs::YesExcludingLocalFileURLsForPrivacy.
        markupToCFHTML(serializePreservingVisualAppearance(frame.selection().selection()),
            selectedRange.start.container->document().url().string(), data);
        HGLOBAL cbData = createGlobalData(data);
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }
    
    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    String str = shouldSerializeSelectedTextForDataTransfer == IncludeImageAltTextForDataTransfer ? frame.editor().selectedTextForDataTransfer() : frame.editor().selectedText();
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(str);
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // enable smart-replacing later on by putting dummy data on the pasteboard
    if (canSmartCopyOrDelete) {
        if (::OpenClipboard(m_owner)) {
            ::SetClipboardData(WebSmartPasteFormat, 0);
            ::CloseClipboard();
        }
    }

    writeRangeToDataObject(selectedRange, frame);
}

void Pasteboard::writePlainTextToDataObject(const String& text, SmartReplaceOption smartReplaceOption)
{
    if (!m_writableDataObject)
        return;

    STGMEDIUM medium { };
    medium.tymed = TYMED_HGLOBAL;

    String str = text;
    replaceNewlinesWithWindowsStyleNewlines(str);
    replaceNBSPWithSpace(str);
    medium.hGlobal = createGlobalData(str);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);        
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    clear();

    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    String str = text;
    replaceNewlinesWithWindowsStyleNewlines(str);
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(str);
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // enable smart-replacing later on by putting dummy data on the pasteboard
    if (smartReplaceOption == CanSmartReplace) {
        if (::OpenClipboard(m_owner)) {
            ::SetClipboardData(WebSmartPasteFormat, 0);
            ::CloseClipboard();
        }
    }

    writePlainTextToDataObject(text, smartReplaceOption);
}

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

static String filesystemPathFromUrlOrTitle(const String& url, const String& title, const String& extension, bool isLink)
{
    static const size_t fsPathMaxLengthExcludingNullTerminator = MAX_PATH - 1;
    bool usedURL = false;
    UChar fsPathBuffer[MAX_PATH];
    fsPathBuffer[0] = 0;
    int fsPathMaxLengthExcludingExtension = fsPathMaxLengthExcludingNullTerminator - extension.length();

    if (!title.isEmpty()) {
        size_t len = std::min<size_t>(title.length(), fsPathMaxLengthExcludingExtension);
        StringView(title).substring(0, len).getCharactersWithUpconvert(fsPathBuffer);
        fsPathBuffer[len] = 0;
        pathRemoveBadFSCharacters(wcharFrom(fsPathBuffer), len);
    }

    if (!wcslen(wcharFrom(fsPathBuffer))) {
        URL kurl(URL(), url);
        usedURL = true;
        // The filename for any content based drag or file url should be the last element of 
        // the path. If we can't find it, or we're coming up with the name for a link
        // we just use the entire url.
        DWORD len = fsPathMaxLengthExcludingExtension;
        auto lastComponent = kurl.lastPathComponent();
        if (kurl.isLocalFile() || (!isLink && !lastComponent.isEmpty())) {
            len = std::min<DWORD>(fsPathMaxLengthExcludingExtension, lastComponent.length());
            lastComponent.substring(0, len).getCharactersWithUpconvert(fsPathBuffer);
        } else {
            len = std::min<DWORD>(fsPathMaxLengthExcludingExtension, url.length());
            StringView(url).substring(0, len).getCharactersWithUpconvert(fsPathBuffer);
        }
        fsPathBuffer[len] = 0;
        pathRemoveBadFSCharacters(wcharFrom(fsPathBuffer), len);
    }

    if (extension.isEmpty())
        return String(fsPathBuffer);

    if (!isLink && usedURL) {
        PathRenameExtension(wcharFrom(fsPathBuffer), extension.wideCharacters().data());
        return String(fsPathBuffer);
    }

    return makeString(const_cast<const UChar*>(fsPathBuffer), extension);
}

// writeFileToDataObject takes ownership of fileDescriptor and fileContent
static HRESULT writeFileToDataObject(IDataObject* dataObject, HGLOBAL fileDescriptor, HGLOBAL fileContent, HGLOBAL hDropContent)
{
    HRESULT hr = S_OK;
    FORMATETC* fe;
    STGMEDIUM medium { };
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

void Pasteboard::writeURLToDataObject(const URL& kurl, const String& titleStr)
{
    if (!m_writableDataObject)
        return;
    WebCore::writeURL(m_writableDataObject.get(), kurl, titleStr, true, true);

    String url = kurl.string();
    ASSERT(url.isAllASCII()); // URL::string() is URL encoded.

    String fsPath = filesystemPathFromUrlOrTitle(url, titleStr, ".URL", true);
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
    if (!fgd) {
        GlobalFree(urlFileDescriptor);
        return;
    }

    ZeroMemory(fgd, sizeof(FILEGROUPDESCRIPTOR));
    fgd->cItems = 1;
    fgd->fgd[0].dwFlags = FD_FILESIZE;
    fgd->fgd[0].nFileSizeLow = content.length();

    unsigned maxSize = std::min<unsigned>(fsPath.length(), WTF_ARRAY_LENGTH(fgd->fgd[0].cFileName));
    StringView(fsPath).substring(0, maxSize).getCharactersWithUpconvert(ucharFrom(fgd->fgd[0].cFileName));
    GlobalUnlock(urlFileDescriptor);

    char* fileContents = static_cast<char*>(GlobalLock(urlFileContent));
    if (!fileContents) {
        GlobalFree(urlFileDescriptor);
        return;
    }

    CopyMemory(fileContents, content.data(), content.length());
    GlobalUnlock(urlFileContent);

    writeFileToDataObject(m_writableDataObject.get(), urlFileDescriptor, urlFileContent, 0);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());

    clear();

    String title(pasteboardURL.title);
    if (title.isEmpty()) {
        title = pasteboardURL.url.lastPathComponent().toString();
        if (title.isEmpty())
            title = pasteboardURL.url.host().toString();
    }

    // write to clipboard in format com.apple.safari.bookmarkdata to be able to paste into the bookmarks view with appropriate title
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(pasteboardURL.url, title);
        if (!::SetClipboardData(BookmarkClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // write to clipboard in format CF_HTML to be able to paste into contenteditable areas as a link
    if (::OpenClipboard(m_owner)) {
        Vector<char> data;
        markupToCFHTML(urlToMarkup(pasteboardURL.url, title), "", data);
        HGLOBAL cbData = createGlobalData(data);
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // bare-bones CF_UNICODETEXT support
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(pasteboardURL.url.string());
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    writeURLToDataObject(pasteboardURL.url, pasteboardURL.title);
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::writeImage(Element& element, const URL&, const String&)
{
    if (!is<RenderImage>(element.renderer()))
        return;

    auto& renderer = downcast<RenderImage>(*element.renderer());
    CachedImage* cachedImage = renderer.cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->imageForRenderer(&renderer);
    ASSERT(image);

    clear();

    HWndDC dc(0);
    auto compatibleDC = adoptGDIObject(::CreateCompatibleDC(0));
    auto sourceDC = adoptGDIObject(::CreateCompatibleDC(0));
    auto resultBitmap = adoptGDIObject(::CreateCompatibleBitmap(dc, image->width(), image->height()));
    HGDIOBJ oldBitmap = ::SelectObject(compatibleDC.get(), resultBitmap.get());

    BitmapInfo bmInfo = BitmapInfo::create(IntSize(image->size()));

    auto coreBitmap = adoptGDIObject(::CreateDIBSection(dc, &bmInfo, DIB_RGB_COLORS, 0, 0, 0));
    HGDIOBJ oldSource = ::SelectObject(sourceDC.get(), coreBitmap.get());
    image->getHBITMAP(coreBitmap.get());

    ::BitBlt(compatibleDC.get(), 0, 0, image->width(), image->height(), sourceDC.get(), 0, 0, SRCCOPY);

    ::SelectObject(sourceDC.get(), oldSource);
    ::SelectObject(compatibleDC.get(), oldBitmap);

    if (::OpenClipboard(m_owner)) {
        ::SetClipboardData(CF_BITMAP, resultBitmap.leak());
        ::CloseClipboard();
    }
}

bool Pasteboard::canSmartReplace()
{ 
    return ::IsClipboardFormatAvailable(WebSmartPasteFormat);
}

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, Optional<size_t>)
{
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT) && ::OpenClipboard(m_owner)) {
        if (HANDLE cbData = ::GetClipboardData(CF_UNICODETEXT)) {
            text.text = static_cast<UChar*>(GlobalLock(cbData));
            GlobalUnlock(cbData);
            ::CloseClipboard();
            return;
        }
        ::CloseClipboard();
    }

    if (::IsClipboardFormatAvailable(CF_TEXT) && ::OpenClipboard(m_owner)) {
        if (HANDLE cbData = ::GetClipboardData(CF_TEXT)) {
            // FIXME: This treats the characters as Latin-1, not UTF-8 or even Windows Latin-1. Is that the right encoding?
            text.text = static_cast<char*>(GlobalLock(cbData));
            GlobalUnlock(cbData);
            ::CloseClipboard();
            return;
        }
        ::CloseClipboard();
    }
}

RefPtr<DocumentFragment> Pasteboard::documentFragment(Frame& frame, const SimpleRange& context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    
    if (::IsClipboardFormatAvailable(HTMLClipboardFormat) && ::OpenClipboard(m_owner)) {
        // get data off of clipboard
        HANDLE cbData = ::GetClipboardData(HTMLClipboardFormat);
        if (cbData) {
            SIZE_T dataSize = ::GlobalSize(cbData);
            String cfhtml(UTF8Encoding().decode(static_cast<char*>(GlobalLock(cbData)), dataSize));
            GlobalUnlock(cbData);
            ::CloseClipboard();

            return fragmentFromCFHTML(frame.document(), cfhtml);
        } else 
            ::CloseClipboard();
    }
     
    if (allowPlainText && ::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        chosePlainText = true;
        if (::OpenClipboard(m_owner)) {
            HANDLE cbData = ::GetClipboardData(CF_UNICODETEXT);
            if (cbData) {
                UChar* buffer = static_cast<UChar*>(GlobalLock(cbData));
                String str(buffer);
                GlobalUnlock(cbData);
                ::CloseClipboard();
                return createFragmentFromText(context, str);
            } else 
                ::CloseClipboard();
        }
    }

    if (allowPlainText && ::IsClipboardFormatAvailable(CF_TEXT)) {
        chosePlainText = true;
        if (::OpenClipboard(m_owner)) {
            HANDLE cbData = ::GetClipboardData(CF_TEXT);
            if (cbData) {
                char* buffer = static_cast<char*>(GlobalLock(cbData));
                String str(buffer);
                GlobalUnlock(cbData);
                ::CloseClipboard();
                return createFragmentFromText(context, str);
            } else
                ::CloseClipboard();
        }
    }
    
    return nullptr;
}

void Pasteboard::setExternalDataObject(IDataObject *dataObject)
{
    m_writableDataObject = 0;
    m_dataObject = dataObject;
}

static CachedImage* getCachedImage(Element& element)
{
    // Attempt to pull CachedImage from element
    RenderObject* renderer = element.renderer();
    if (!is<RenderImage>(renderer))
        return nullptr;

    auto* image = downcast<RenderImage>(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return nullptr;
}

static HGLOBAL createGlobalImageFileDescriptor(const String& url, const String& title, CachedImage* image)
{
    ASSERT_ARG(image, image);
    ASSERT(image->image()->data());

    String fsPath;
    HGLOBAL memObj = GlobalAlloc(GPTR, sizeof(FILEGROUPDESCRIPTOR));
    if (!memObj)
        return 0;

    FILEGROUPDESCRIPTOR* fgd = (FILEGROUPDESCRIPTOR*)GlobalLock(memObj);
    if (!fgd) {
        GlobalFree(memObj);
        return 0;
    }

    memset(fgd, 0, sizeof(FILEGROUPDESCRIPTOR));
    fgd->cItems = 1;
    fgd->fgd[0].dwFlags = FD_FILESIZE;
    fgd->fgd[0].nFileSizeLow = image->image()->data()->size();

    const String& preferredTitle = title.isEmpty() ? image->response().suggestedFilename() : title;
    String extension = image->image()->filenameExtension();
    if (extension.isEmpty()) {
        // Do not continue processing in the rare and unusual case where a decoded image is not able 
        // to provide a filename extension. Something tricky (like a bait-n-switch) is going on
        GlobalUnlock(memObj);
        GlobalFree(memObj);
        return 0;
    }
    extension.insert(".", 0);
    fsPath = filesystemPathFromUrlOrTitle(url, preferredTitle, extension, false);

    if (fsPath.length() <= 0) {
        GlobalUnlock(memObj);
        GlobalFree(memObj);
        return 0;
    }

    int maxSize = std::min<int>(fsPath.length(), WTF_ARRAY_LENGTH(fgd->fgd[0].cFileName));
    StringView(fsPath).substring(0, maxSize).getCharactersWithUpconvert(ucharFrom(fgd->fgd[0].cFileName));
    GlobalUnlock(memObj);

    return memObj;
}

static HGLOBAL createGlobalImageFileContent(SharedBuffer* data)
{
    HGLOBAL memObj = GlobalAlloc(GPTR, data->size());
    if (!memObj) 
        return 0;

    char* fileContents = (PSTR)GlobalLock(memObj);
    if (!fileContents) {
        GlobalFree(memObj);
        return 0;
    }

    if (data->data())
        CopyMemory(fileContents, data->data(), data->size());

    GlobalUnlock(memObj);

    return memObj;
}

static HGLOBAL createGlobalHDropContent(const URL& url, String& fileName, SharedBuffer* data)
{
    if (fileName.isEmpty() || !data)
        return 0;

    WCHAR filePath[MAX_PATH];

    if (url.isLocalFile()) {
        String localPath = decodeURLEscapeSequences(url.path());
        // windows does not enjoy a leading slash on paths
        if (localPath[0] == '/')
            localPath = localPath.substring(1);
        LPCWSTR localPathStr = localPath.wideCharacters().data();
        if (localPathStr && wcslen(localPathStr) + 1 < MAX_PATH)
            wcscpy_s(filePath, MAX_PATH, localPathStr);
        else
            return 0;
    } else {
        WCHAR tempPath[MAX_PATH];
        WCHAR extension[MAX_PATH];
        if (!::GetTempPath(WTF_ARRAY_LENGTH(tempPath), tempPath))
            return 0;
        if (!::PathAppend(tempPath, fileName.wideCharacters().data()))
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
        BOOL tempWriteSucceeded = FALSE;
        if (data->data())
            tempWriteSucceeded = WriteFile(tempFileHandle, data->data(), data->size(), &written, 0);
        CloseHandle(tempFileHandle);
        if (!tempWriteSucceeded)
            return 0;
    }

    SIZE_T dropFilesSize = sizeof(DROPFILES) + (sizeof(WCHAR) * (wcslen(filePath) + 2));
    HGLOBAL memObj = GlobalAlloc(GHND | GMEM_SHARE, dropFilesSize);
    if (!memObj) 
        return 0;

    DROPFILES* dropFiles = (DROPFILES*) GlobalLock(memObj);
    if (!dropFiles) {
        GlobalFree(memObj);
        return 0;
    }

    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;
    wcscpy(reinterpret_cast<LPWSTR>(dropFiles + 1), filePath);
    GlobalUnlock(memObj);

    return memObj;
}

void Pasteboard::writeImageToDataObject(Element& element, const URL& url)
{
    // Shove image data into a DataObject for use as a file
    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->imageForRenderer(element.renderer()) || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->imageForRenderer(element.renderer())->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    HGLOBAL imageFileDescriptor = createGlobalImageFileDescriptor(url.string(), element.attributeWithoutSynchronization(HTMLNames::altAttr), cachedImage);
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
        GlobalFree(imageFileDescriptor);
        GlobalFree(imageFileContent);
        return;
    }

    writeFileToDataObject(m_writableDataObject.get(), imageFileDescriptor, imageFileContent, hDropContent);
}

void Pasteboard::writeURLToWritableDataObject(const URL& url, const String& title)
{
    WebCore::writeURL(m_writableDataObject.get(), url, title, true, false);
}

void Pasteboard::writeMarkup(const String& markup)
{
    Vector<char> data;
    markupToCFHTML(markup, "", data);

    STGMEDIUM medium { };
    medium.tymed = TYMED_HGLOBAL;

    medium.hGlobal = createGlobalData(data);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        GlobalFree(medium.hGlobal);
}

void Pasteboard::write(const PasteboardWebContent&)
{
}

void Pasteboard::read(PasteboardWebContentReader&, WebContentReadingPolicy, Optional<size_t>)
{
}

void Pasteboard::write(const PasteboardImage&)
{
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    if (data.isEmpty() || data.size() > 1) {
        // We don't support more than one custom item in the clipboard.
        return;
    }

    clear();

    if (::OpenClipboard(m_owner)) {
        const auto& customData = data.first();
        customData.forEachPlatformStringOrBuffer([](auto& type, auto& stringOrBuffer) {
            if (WTF::holds_alternative<String>(stringOrBuffer)) {
                ClipboardDataType dataType = clipboardTypeFromMIMEType(type);

                String str = WTF::get<String>(stringOrBuffer);
                replaceNewlinesWithWindowsStyleNewlines(str);
                HGLOBAL cbData = createGlobalData(str);

                if (dataType == ClipboardDataTypeText) {
                    if (cbData && !::SetClipboardData(CF_UNICODETEXT, cbData))
                        ::GlobalFree(cbData);
                } else if (dataType == ClipboardDataTypeURL) {
                    if (cbData && !::SetClipboardData(BookmarkClipboardFormat, cbData))
                        ::GlobalFree(cbData);
                } else if (dataType == ClipboardDataTypeTextHTML) {
                    if (cbData && !::SetClipboardData(HTMLClipboardFormat, cbData))
                        ::GlobalFree(cbData);
                }
            }
        });

        if (customData.hasSameOriginCustomData() || !customData.origin().isEmpty()) {
            auto sharedBuffer = customData.createSharedBuffer();
            HGLOBAL cbData = createGlobalData(reinterpret_cast<const uint8_t*>(sharedBuffer->data()), sharedBuffer->size());
            if (cbData && !::SetClipboardData(CustomDataClipboardFormat, cbData))
                ::GlobalFree(cbData);
        }

        ::CloseClipboard();
    }
}

void Pasteboard::write(const Color&)
{
}

} // namespace WebCore
