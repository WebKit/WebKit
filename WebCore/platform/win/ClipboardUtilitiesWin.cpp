/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "ClipboardUtilitiesWin.h"

#include "KURL.h"
#include "CString.h"
#include "DocumentFragment.h"
#include "markup.h"
#include "PlatformString.h"
#include "TextEncoding.h"
#include <shlwapi.h>
#include <wininet.h>    // for INTERNET_MAX_URL_LENGTH

namespace WebCore {

FORMATETC* cfHDropFormat()
{
    static FORMATETC urlFormat = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

static bool getWebLocData(IDataObject* dataObject, String& url, String* title) 
{
    bool succeeded = false;
    WCHAR filename[MAX_PATH];
    WCHAR urlBuffer[INTERNET_MAX_URL_LENGTH];

    STGMEDIUM medium;
    if (FAILED(dataObject->GetData(cfHDropFormat(), &medium)))
        return false;

    HDROP hdrop = (HDROP)GlobalLock(medium.hGlobal);
   
    if (!hdrop)
        return false;

    if (!DragQueryFileW(hdrop, 0, filename, ARRAYSIZE(filename)))
        goto exit;

    if (_wcsicmp(PathFindExtensionW(filename), L".url"))
        goto exit;    
    
    if (!GetPrivateProfileStringW(L"InternetShortcut", L"url", 0, urlBuffer, ARRAYSIZE(urlBuffer), filename))
        goto exit;
    
    if (title) {
        PathRemoveExtension(filename);
        *title = String((UChar*)filename);
    }
    
    url = String((UChar*)urlBuffer);
    succeeded = true;

exit:
    // Free up memory.
    DragFinish(hdrop);
    GlobalUnlock(medium.hGlobal);
    return succeeded;
}

static String extractURL(const String &inURL, String* title)
{
    String url = inURL;
    int splitLoc = url.find('\n');
    if (splitLoc > 0) {
        if (title)
            *title = url.substring(splitLoc+1);
        url.truncate(splitLoc);
    } else if (title)
        *title = url;
    return url;
}

//Firefox text/html
static FORMATETC* texthtmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"text/html");
    static FORMATETC texthtmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &texthtmlFormat;
}

HGLOBAL createGlobalData(const KURL& url, const String& title)
{
    String mutableURL(url.url());
    String mutableTitle(title);
    SIZE_T size = mutableURL.length() + mutableTitle.length() + 2;  // +1 for "\n" and +1 for null terminator
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size * sizeof(UChar));

    if (cbData) {
        PWSTR buffer = (PWSTR)::GlobalLock(cbData);
        swprintf_s(buffer, size, L"%s\n%s", mutableURL.charactersWithNullTermination(), mutableTitle.charactersWithNullTermination());
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

HGLOBAL createGlobalData(String str)
{   
    SIZE_T size = (str.length() + 1) * sizeof(UChar);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size);
    if (cbData) {
        void* buffer = ::GlobalLock(cbData);
        memcpy(buffer, str.charactersWithNullTermination(), size);
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

HGLOBAL createGlobalData(CString str)
{
    SIZE_T size = str.length() * sizeof(char);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size + 1);
    if (cbData) {
        char* buffer = static_cast<char*>(::GlobalLock(cbData));
        memcpy(buffer, str.data(), size);
        buffer[size] = 0;
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

// Documentation for the CF_HTML format is available at http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
DeprecatedCString markupToCF_HTML(const String& markup, const String& srcURL)
{
    if (!markup.length())
        return DeprecatedCString();

    DeprecatedCString cf_html        ("Version:0.9");
    DeprecatedCString startHTML      ("\nStartHTML:");
    DeprecatedCString endHTML        ("\nEndHTML:");
    DeprecatedCString startFragment  ("\nStartFragment:");
    DeprecatedCString endFragment    ("\nEndFragment:");
    DeprecatedCString sourceURL      ("\nSourceURL:");

    bool shouldFillSourceURL = !srcURL.isEmpty() && (srcURL != "about:blank");
    if (shouldFillSourceURL)
        sourceURL.append(srcURL.utf8().data());

    DeprecatedCString startMarkup    ("\n<HTML>\n<BODY>\n<!--StartFragment-->\n");
    DeprecatedCString endMarkup      ("\n<!--EndFragment-->\n</BODY>\n</HTML>");

    // calculate offsets
    const unsigned UINT_MAXdigits = 10; // number of digits in UINT_MAX in base 10
    unsigned startHTMLOffset = cf_html.length() + startHTML.length() + endHTML.length() + startFragment.length() + endFragment.length() + (shouldFillSourceURL ? sourceURL.length() : 0) + (4*UINT_MAXdigits);
    unsigned startFragmentOffset = startHTMLOffset + startMarkup.length();
    CString markupUTF8 = markup.utf8();
    unsigned endFragmentOffset = startFragmentOffset + markupUTF8.length();
    unsigned endHTMLOffset = endFragmentOffset + endMarkup.length();

    // fill in needed data
    startHTML.append(String::format("%010u", startHTMLOffset).deprecatedString().utf8());
    endHTML.append(String::format("%010u", endHTMLOffset).deprecatedString().utf8());
    startFragment.append(String::format("%010u", startFragmentOffset).deprecatedString().utf8());
    endFragment.append(String::format("%010u", endFragmentOffset).deprecatedString().utf8());
    startMarkup.append(markupUTF8.data());

    // create full cf_html string from the fragments
    cf_html.append(startHTML);
    cf_html.append(endHTML);
    cf_html.append(startFragment);
    cf_html.append(endFragment);
    if (shouldFillSourceURL)
        cf_html.append(sourceURL);
    cf_html.append(startMarkup);
    cf_html.append(endMarkup);

    return cf_html;
}

String urlToMarkup(const KURL& url, const String& title)
{
    String markup("<a href=\"");
    markup.append(url.url());
    markup.append("\">");
    markup.append(title);
    markup.append("</a>");
    return markup;
}

void replaceNewlinesWithWindowsStyleNewlines(String& str)
{
    static const UChar Newline = '\n';
    static const String WindowsNewline("\r\n");
    str.replace(Newline, WindowsNewline);
}

void replaceNBSPWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}

FORMATETC* urlWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocatorW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* urlFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocator");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* plainTextFormat()
{
    static FORMATETC textFormat = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

FORMATETC* plainTextWFormat()
{
    static FORMATETC textFormat = {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

FORMATETC* filenameWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileNameW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

FORMATETC* filenameFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileName");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

//MSIE HTML Format
FORMATETC* htmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"HTML Format");
    static FORMATETC htmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &htmlFormat;
}

String getURL(IDataObject* dataObject, bool& success, String* title)
{
    STGMEDIUM store;
    String url;
    success = false;
    if (getWebLocData(dataObject, url, title)) {
        success = true;
        return url;
    } else if (SUCCEEDED(dataObject->GetData(urlWFormat(), &store))) {
        //URL using unicode
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        url = extractURL(String(data), title);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    } else if (SUCCEEDED(dataObject->GetData(urlFormat(), &store))) {
        //URL using ascii
        char* data = (char*)GlobalLock(store.hGlobal);
        url = extractURL(String(data), title);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    } else if (SUCCEEDED(dataObject->GetData(filenameWFormat(), &store))) {
        //file using unicode
        wchar_t* data = (wchar_t*)GlobalLock(store.hGlobal);        
        if (data && data[0] && (PathFileExists(data) || PathIsUNC(data))) {
            wchar_t fileURL[INTERNET_MAX_URL_LENGTH];
            DWORD fileURLLength = sizeof(fileURL) / sizeof(fileURL[0]);
            if (SUCCEEDED(::UrlCreateFromPathW(data, fileURL, &fileURLLength, 0))) {
                url = String((UChar*)fileURL);
                if (title)
                    *title = url;
            }
        }
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    } else if (SUCCEEDED(dataObject->GetData(filenameFormat(), &store))) {
        //filename using ascii
        char* data = (char*)GlobalLock(store.hGlobal);       
        if (data && data[0] && (PathFileExistsA(data) || PathIsUNCA(data))) {
            char fileURL[INTERNET_MAX_URL_LENGTH];
            DWORD fileURLLength = sizeof(fileURL) / sizeof(fileURL[0]);
            if (SUCCEEDED(::UrlCreateFromPathA(data, fileURL, &fileURLLength, 0))) {
                url = fileURL;
                if (title)
                    *title = url;
            }
        }
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    }
    return url;
}

String getPlainText(IDataObject* dataObject, bool& success)
{
    STGMEDIUM store;
    String text;
    success = false;
    if (SUCCEEDED(dataObject->GetData(plainTextWFormat(), &store))) {
        //unicode text
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        text = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    } else if (SUCCEEDED(dataObject->GetData(plainTextFormat(), &store))) {
        //ascii text
        char* data = (char*)GlobalLock(store.hGlobal);
        text = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        success = true;
    } else {
        //If a file is dropped on the window, it does not provide either of the 
        //plain text formats, so here we try to forcibly get a url.
        text = getURL(dataObject, success);
        success = true;
    }
    return text;
}

PassRefPtr<DocumentFragment> fragmentFromFilenames(Document*, const IDataObject*)
{
    //FIXME: We should be able to create fragments from files
    return 0;
}

bool containsFilenames(const IDataObject*)
{
    //FIXME: We'll want to update this once we can produce fragments from files
    return false;
}

//Convert a String containing CF_HTML formatted text to a DocumentFragment
PassRefPtr<DocumentFragment> fragmentFromCF_HTML(Document* doc, const String& cf_html)
{        
    // obtain baseURL if present
    String srcURLStr("sourceURL:");
    String srcURL;
    unsigned lineStart = cf_html.find(srcURLStr, 0, false);
    if (lineStart != -1) {
        unsigned srcEnd = cf_html.find("\n", lineStart, false);
        unsigned srcStart = lineStart+srcURLStr.length();
        String rawSrcURL = cf_html.substring(srcStart, srcEnd-srcStart);
        replaceNBSPWithSpace(rawSrcURL);
        srcURL = rawSrcURL.stripWhiteSpace();
    }

    // find the markup between "<!--StartFragment -->" and "<!--EndFragment -->", accounting for browser quirks
    unsigned markupStart = cf_html.find("<html", 0, false);
    unsigned tagStart = cf_html.find("startfragment", markupStart, false);
    unsigned fragmentStart = cf_html.find('>', tagStart) + 1;
    unsigned tagEnd = cf_html.find("endfragment", fragmentStart, false);
    unsigned fragmentEnd = cf_html.reverseFind('<', tagEnd);
    String markup = cf_html.substring(fragmentStart, fragmentEnd - fragmentStart).stripWhiteSpace();

    return createFragmentFromMarkup(doc, markup, srcURL).releaseRef();
}


PassRefPtr<DocumentFragment> fragmentFromHTML(Document* doc, IDataObject* data) 
{
    if (!doc || !data)
        return 0;

    STGMEDIUM store;
    String html;
    String srcURL;
    if (SUCCEEDED(data->GetData(htmlFormat(), &store))) {
        //MS HTML Format parsing
        char* data = (char*)GlobalLock(store.hGlobal);
        SIZE_T dataSize = ::GlobalSize(store.hGlobal);
        String cf_html(UTF8Encoding().decode(data, dataSize));         
        GlobalUnlock(store.hGlobal);
        ReleaseStgMedium(&store); 
        if (PassRefPtr<DocumentFragment> fragment = fragmentFromCF_HTML(doc, cf_html))
            return fragment;
    } 
    if (SUCCEEDED(data->GetData(texthtmlFormat(), &store))) {
        //raw html
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        html = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        return createFragmentFromMarkup(doc, html, srcURL);
    } 

    return 0;
}

bool containsHTML(IDataObject* data)
{
    return SUCCEEDED(data->QueryGetData(texthtmlFormat())) || SUCCEEDED(data->QueryGetData(htmlFormat()));
}

} // namespace WebCore
