/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
#include "Pasteboard.h"

#include "CString.h"
#include "ClipboardUtilitiesWin.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "Image.h"
#include "KURL.h"
#include "Page.h"
#include "Range.h"
#include "RenderImage.h"
#include "TextEncoding.h"
#include "markup.h"

namespace WebCore {

static UINT HTMLClipboardFormat = 0;
static UINT BookmarkClipboardFormat = 0;
static UINT WebSmartPasteFormat = 0;

extern HDC hScreenDC;

static LRESULT CALLBACK PasteboardOwnerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lresult = 0;
    LONG longPtr = GetWindowLong(hWnd, 0);

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
    default:
        lresult = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }
    return lresult;
}

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
{
    // make a dummy HWND to be the Windows clipboard's owner
    WNDCLASS wc = {0};
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc    = PasteboardOwnerWndProc;
    wc.hInstance      = Page::instanceHandle();
    wc.lpszClassName  = L"PasteboardOwnerWindowClass";
    ::RegisterClass(&wc);

    m_owner = ::CreateWindow(L"PasteboardOwnerWindowClass", L"PasteboardOwnerWindow", 0, 0, 0, 0, 0,
        HWND_MESSAGE, 0, 0, 0);

    HTMLClipboardFormat = ::RegisterClipboardFormat(L"HTML Format");
    BookmarkClipboardFormat = ::RegisterClipboardFormat(L"UniformResourceLocatorW");
    WebSmartPasteFormat = ::RegisterClipboardFormat(L"WebKit Smart Paste Format");
}

void Pasteboard::clear()
{
    if (::OpenClipboard(m_owner)) {
        ::EmptyClipboard();
        ::CloseClipboard();
    }
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    clear();

    // Put CF_HTML format on the pasteboard
    if (::OpenClipboard(m_owner)) {
        ExceptionCode ec = 0;
        Vector<char> data;
        markupToCF_HTML(createMarkup(selectedRange, 0, AnnotateForInterchange), selectedRange->startContainer(ec)->document()->url(), data);
        HGLOBAL cbData = createGlobalData(data);
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    String str = frame->selectedText();
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
}

void Pasteboard::writePlainText(const String& text)
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
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    ASSERT(!url.isEmpty());

    clear();

    String title(titleStr);
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    // write to clipboard in format com.apple.safari.bookmarkdata to be able to paste into the bookmarks view with appropriate title
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(url, title);
        if (!::SetClipboardData(BookmarkClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // write to clipboard in format CF_HTML to be able to paste into contenteditable areas as a link
    if (::OpenClipboard(m_owner)) {
        Vector<char> data;
        markupToCF_HTML(urlToMarkup(url, title), "", data);
        HGLOBAL cbData = createGlobalData(data);
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // bare-bones CF_UNICODETEXT support
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(url.string());
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }
}

void Pasteboard::writeImage(Node* node, const KURL&, const String&)
{
    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = static_cast<RenderImage*>(node->renderer());
    CachedImage* cachedImage = static_cast<CachedImage*>(renderer->cachedImage());
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    clear();

    RefPtr<SharedBitmap> sourceBmp = image->nativeImageForCurrentFrame();
    if (!sourceBmp)
        return;

    IntRect rect(0, 0, sourceBmp->width(), sourceBmp->height());
    BitmapInfo bmpInfo;
    void* pixels;
    HBITMAP resultBitmap = sourceBmp->clipBitmap(rect, true, bmpInfo, pixels);
    if (!resultBitmap)
        return;

    if (::OpenClipboard(m_owner)) {
        ::SetClipboardData(CF_BITMAP, resultBitmap);
        ::CloseClipboard();
    } else
        DeleteObject(resultBitmap);
}

bool Pasteboard::canSmartReplace()
{
    return ::IsClipboardFormatAvailable(WebSmartPasteFormat);
}

String Pasteboard::plainText(Frame* frame)
{
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT) && ::OpenClipboard(m_owner)) {
        HANDLE cbData = ::GetClipboardData(CF_UNICODETEXT);
        if (cbData) {
            UChar* buffer = (UChar*)GlobalLock(cbData);
            String fromClipboard(buffer);
            GlobalUnlock(cbData);
            CloseClipboard();
            return fromClipboard;
        } else
            CloseClipboard();
    }

    if (::IsClipboardFormatAvailable(CF_TEXT) && ::OpenClipboard(m_owner)) {
        HANDLE cbData = ::GetClipboardData(CF_TEXT);
        if (cbData) {
            char* buffer = (char*)GlobalLock(cbData);
            String fromClipboard(buffer);
            GlobalUnlock(cbData);
            CloseClipboard();
            return fromClipboard;
        } else
            CloseClipboard();
    }

    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    if (::IsClipboardFormatAvailable(HTMLClipboardFormat) && ::OpenClipboard(m_owner)) {
        // get data off of clipboard
        HANDLE cbData = ::GetClipboardData(HTMLClipboardFormat);
        if (cbData) {
            SIZE_T dataSize = ::GlobalSize(cbData);
            String cf_html(UTF8Encoding().decode((char*)GlobalLock(cbData), dataSize));
            GlobalUnlock(cbData);
            CloseClipboard();

            PassRefPtr<DocumentFragment> fragment = fragmentFromCF_HTML(frame->document(), cf_html);
            if (fragment)
                return fragment;
        } else
            CloseClipboard();
    }

    if (allowPlainText && IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        chosePlainText = true;
        if (OpenClipboard(m_owner)) {
            HANDLE cbData = GetClipboardData(CF_UNICODETEXT);
            if (cbData) {
                UChar* buffer = (UChar*)GlobalLock(cbData);
                String str(buffer);
                GlobalUnlock(cbData);
                CloseClipboard();
                RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), str);
                if (fragment)
                    return fragment.release();
            } else
                CloseClipboard();
        }
    }

    if (allowPlainText && ::IsClipboardFormatAvailable(CF_TEXT)) {
        chosePlainText = true;
        if (::OpenClipboard(m_owner)) {
            HANDLE cbData = ::GetClipboardData(CF_TEXT);
            if (cbData) {
                char* buffer = (char*)GlobalLock(cbData);
                String str(buffer);
                GlobalUnlock(cbData);
                CloseClipboard();
                RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), str);
                if (fragment)
                    return fragment.release();
            } else
                CloseClipboard();
        }
    }

    return 0;
}

bool Pasteboard::hasData()
{
    return hasDataInFormat(CF_UNICODETEXT) || hasDataInFormat(CF_TEXT);
}

bool Pasteboard::hasDataInFormat(unsigned int format)
{
    return ::IsClipboardFormatAvailable(format);
}

} // namespace WebCore
