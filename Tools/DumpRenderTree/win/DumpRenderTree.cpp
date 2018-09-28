/*
 * Copyright (C) 2005-2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DumpRenderTree.h"

#include "EditingDelegate.h"
#include "FrameLoadDelegate.h"
#include "GCController.h"
#include "HistoryDelegate.h"
#include "JavaScriptThreading.h"
#include "PixelDumpSupport.h"
#include "PolicyDelegate.h"
#include "ResourceLoadDelegate.h"
#include "TestOptions.h"
#include "TestRunner.h"
#include "UIDelegate.h"
#include "WebCoreTestSupport.h"
#include "WorkQueueItem.h"
#include "WorkQueue.h"

#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/TestRunnerUtils.h>
#include <WebCore/FileSystem.h>
#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <comutil.h>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <math.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <windows.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if USE(CFURLCONNECTION)
#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CFNetwork/CFURLCachePriv.h>
#endif

using namespace std;

#ifdef DEBUG_ALL
const _bstr_t TestPluginDir = L"TestNetscapePlugin_Debug";
#else
const _bstr_t TestPluginDir = L"TestNetscapePlugin";
#endif

static LPCWSTR fontsEnvironmentVariable = L"WEBKIT_TESTFONTS";
static LPCWSTR dumpRenderTreeTemp = L"DUMPRENDERTREE_TEMP";
#define USE_MAC_FONTS

static CFStringRef WebDatabaseDirectoryDefaultsKey = CFSTR("WebDatabaseDirectory");
static CFStringRef WebKitLocalCacheDefaultsKey = CFSTR("WebKitLocalCache");
static CFStringRef WebStorageDirectoryDefaultsKey = CFSTR("WebKitLocalStorageDatabasePathPreferenceKey");

const LPCWSTR kDumpRenderTreeClassName = L"DumpRenderTreeWindow";

static bool dumpPixelsForAllTests = false;
static bool dumpPixelsForCurrentTest = false;
static bool threaded = false;
static bool dumpTree = true;
static bool useTimeoutWatchdog = true;
static bool forceComplexText = false;
static bool dumpAllPixels;
static bool useAcceleratedDrawing = true; // Not used
// FIXME: gcBetweenTests should initially be false, but we currently set it to true, to make sure
// deallocations are not performed in one of the following tests which might cause flakiness.
static bool gcBetweenTests = true; 
static bool printSeparators = false;
static bool leakChecking = false;
static bool printSupportedFeatures = false;
static bool showWebView = false;
static RetainPtr<CFStringRef> persistentUserStyleSheetLocation;

volatile bool done;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
IWebFrame* topLoadingFrame;     // !nil iff a load is in progress
static COMPtr<IWebHistoryItem> prevTestBFItem;  // current b/f item at the end of the previous test
PolicyDelegate* policyDelegate; 
COMPtr<FrameLoadDelegate> sharedFrameLoadDelegate;
COMPtr<UIDelegate> sharedUIDelegate;
COMPtr<EditingDelegate> sharedEditingDelegate;
COMPtr<ResourceLoadDelegate> resourceLoadDelegate;
COMPtr<HistoryDelegate> sharedHistoryDelegate;

IWebFrame* frame;
HWND webViewWindow;

RefPtr<TestRunner> gTestRunner;

UINT_PTR waitToDumpWatchdog = 0;

void setPersistentUserStyleSheetLocation(CFStringRef url)
{
    persistentUserStyleSheetLocation = url;
}

bool setAlwaysAcceptCookies(bool)
{
    // FIXME: Implement this by making the Windows port use the testing network storage session and
    // modify its cookie storage policy.
    return false;
}

static RetainPtr<CFStringRef> substringFromIndex(CFStringRef string, CFIndex index)
{
    return adoptCF(CFStringCreateWithSubstring(kCFAllocatorDefault, string, CFRangeMake(index, CFStringGetLength(string) - index)));
}

static wstring lastPathComponentAsWString(CFURLRef url)
{
    RetainPtr<CFStringRef> lastPathComponent = adoptCF(CFURLCopyLastPathComponent(url));
    return cfStringRefToWString(lastPathComponent.get());
}

wstring urlSuitableForTestResult(const wstring& urlString)
{
    if (urlString.empty())
        return urlString;

    RetainPtr<CFURLRef> url = adoptCF(CFURLCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(urlString.c_str()), urlString.length() * sizeof(wstring::value_type), kCFStringEncodingUTF16, 0));

    RetainPtr<CFStringRef> scheme = adoptCF(CFURLCopyScheme(url.get()));
    if (scheme && CFStringCompare(scheme.get(), CFSTR("file"), kCFCompareCaseInsensitive) != kCFCompareEqualTo)
        return urlString;

    COMPtr<IWebDataSource> dataSource;
    if (FAILED(frame->dataSource(&dataSource))) {
        if (FAILED(frame->provisionalDataSource(&dataSource)))
            return lastPathComponentAsWString(url.get());
    }

    COMPtr<IWebMutableURLRequest> request;
    if (FAILED(dataSource->request(&request)))
        return lastPathComponentAsWString(url.get());

    _bstr_t requestURLString;
    if (FAILED(request->URL(requestURLString.GetAddress())))
        return lastPathComponentAsWString(url.get());

    RetainPtr<CFURLRef> requestURL = adoptCF(CFURLCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(requestURLString.GetBSTR()), requestURLString.length() * sizeof(OLECHAR), kCFStringEncodingUTF16, 0));
    RetainPtr<CFURLRef> baseURL = adoptCF(CFURLCreateCopyDeletingLastPathComponent(kCFAllocatorDefault, requestURL.get()));

    RetainPtr<CFStringRef> basePath = adoptCF(CFURLCopyPath(baseURL.get()));
    RetainPtr<CFStringRef> path = adoptCF(CFURLCopyPath(url.get()));

    if (basePath.get() && CFStringHasPrefix(path.get(), basePath.get()))
        return cfStringRefToWString(substringFromIndex(path.get(), CFStringGetLength(basePath.get())).get());

    return lastPathComponentAsWString(url.get());
}

wstring lastPathComponent(const wstring& urlString)
{
    if (urlString.empty())
        return urlString;

    RetainPtr<CFURLRef> url = adoptCF(CFURLCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(urlString.c_str()), urlString.length() * sizeof(wstring::value_type), kCFStringEncodingUTF16, 0));
    RetainPtr<CFStringRef> lastPathComponent = adoptCF(CFURLCopyLastPathComponent(url.get()));

    return cfStringRefToWString(lastPathComponent.get());
}

static string toUTF8(const wchar_t* wideString, size_t length)
{
    int result = WideCharToMultiByte(CP_UTF8, 0, wideString, length + 1, 0, 0, 0, 0);
    Vector<char> utf8Vector(result);
    result = WideCharToMultiByte(CP_UTF8, 0, wideString, length + 1, utf8Vector.data(), result, 0, 0);
    if (!result)
        return string();

    return string(utf8Vector.data(), utf8Vector.size() - 1);
}

#if USE(CF)
static String libraryPathForDumpRenderTree()
{
    DWORD size = ::GetEnvironmentVariable(dumpRenderTreeTemp, 0, 0);
    Vector<TCHAR> buffer(size);
    if (::GetEnvironmentVariable(dumpRenderTreeTemp, buffer.data(), buffer.size())) {
        wstring path = buffer.data();
        if (!path.empty() && (path[path.length() - 1] != L'\\'))
            path.append(L"\\");
        return String (path.data(), path.length());
    }

    return WebCore::FileSystem::localUserSpecificStorageDirectory();
}
#endif

string toUTF8(BSTR bstr)
{
    return toUTF8(bstr, SysStringLen(bstr));
}

string toUTF8(const wstring& wideString)
{
    return toUTF8(wideString.c_str(), wideString.length());
}

wstring cfStringRefToWString(CFStringRef cfStr)
{
    Vector<wchar_t> v(CFStringGetLength(cfStr));
    CFStringGetCharacters(cfStr, CFRangeMake(0, CFStringGetLength(cfStr)), (UniChar *)v.data());

    return wstring(v.data(), v.size());
}

static LRESULT CALLBACK DumpRenderTreeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_DESTROY:
            for (long i = openWindows().size() - 1; i >= 0; --i) {
                if (openWindows()[i] == hWnd) {
                    openWindows().remove(i);
                    windowToWebViewMap().remove(hWnd);
                    break;
                }
            }
            return 0;
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

static const wstring& exePath()
{
    static wstring path;
    static bool initialized;

    if (initialized)
        return path;
    initialized = true;

    TCHAR buffer[MAX_PATH];
    GetModuleFileName(GetModuleHandle(0), buffer, ARRAYSIZE(buffer));
    path = buffer;
    int lastSlash = path.rfind('\\');
    if (lastSlash != -1 && lastSlash + 1 < path.length())
        path = path.substr(0, lastSlash + 1);

    return path;
}

static const wstring& fontsPath()
{
    static wstring path;
    static bool initialized;

    if (initialized)
        return path;
    initialized = true;

    DWORD size = GetEnvironmentVariable(fontsEnvironmentVariable, 0, 0);
    Vector<TCHAR> buffer(size);
    if (GetEnvironmentVariable(fontsEnvironmentVariable, buffer.data(), buffer.size())) {
        path = buffer.data();
        if (path[path.length() - 1] != '\\')
            path.append(L"\\");
        return path;
    }

    path = exePath() + TEXT("DumpRenderTree.resources\\");
    return path;
}

#ifdef DEBUG_ALL
#define WEBKITDLL TEXT("WebKit_debug.dll")
#else
#define WEBKITDLL TEXT("WebKit.dll")
#endif

static void initialize()
{
    if (HMODULE webKitModule = LoadLibrary(WEBKITDLL))
        if (FARPROC dllRegisterServer = GetProcAddress(webKitModule, "DllRegisterServer"))
            dllRegisterServer();

    // Init COM
    OleInitialize(nullptr);

    static LPCTSTR fontsToInstall[] = {
        TEXT("AHEM____.ttf"),
        TEXT("Apple Chancery.ttf"),
        TEXT("Courier Bold.ttf"),
        TEXT("Courier.ttf"),
        TEXT("Helvetica Bold Oblique.ttf"),
        TEXT("Helvetica Bold.ttf"),
        TEXT("Helvetica Oblique.ttf"),
        TEXT("Helvetica.ttf"),
        TEXT("Helvetica Neue Bold Italic.ttf"),
        TEXT("Helvetica Neue Bold.ttf"),
        TEXT("Helvetica Neue Condensed Black.ttf"),
        TEXT("Helvetica Neue Condensed Bold.ttf"),
        TEXT("Helvetica Neue Italic.ttf"),
        TEXT("Helvetica Neue Light Italic.ttf"),
        TEXT("Helvetica Neue Light.ttf"),
        TEXT("Helvetica Neue UltraLight Italic.ttf"),
        TEXT("Helvetica Neue UltraLight.ttf"),
        TEXT("Helvetica Neue.ttf"),
        TEXT("Lucida Grande.ttf"),
        TEXT("Lucida Grande Bold.ttf"),
        TEXT("Monaco.ttf"),
        TEXT("Papyrus.ttf"),
        TEXT("Times Bold Italic.ttf"),
        TEXT("Times Bold.ttf"),
        TEXT("Times Italic.ttf"),
        TEXT("Times Roman.ttf"),
        TEXT("WebKit Layout Tests 2.ttf"),
        TEXT("WebKit Layout Tests.ttf"),
        TEXT("WebKitWeightWatcher100.ttf"),
        TEXT("WebKitWeightWatcher200.ttf"),
        TEXT("WebKitWeightWatcher300.ttf"),
        TEXT("WebKitWeightWatcher400.ttf"),
        TEXT("WebKitWeightWatcher500.ttf"),
        TEXT("WebKitWeightWatcher600.ttf"),
        TEXT("WebKitWeightWatcher700.ttf"),
        TEXT("WebKitWeightWatcher800.ttf"),
        TEXT("WebKitWeightWatcher900.ttf")
    };

    wstring resourcesPath = fontsPath();

    COMPtr<IWebTextRenderer> textRenderer;
    if (SUCCEEDED(WebKitCreateInstance(CLSID_WebTextRenderer, 0, IID_IWebTextRenderer, (void**)&textRenderer)))
        for (int i = 0; i < ARRAYSIZE(fontsToInstall); ++i)
            textRenderer->registerPrivateFont(wstring(resourcesPath + fontsToInstall[i]).c_str());

    // Register a host window
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DumpRenderTreeWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = GetModuleHandle(0);
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = kDumpRenderTreeClassName;
    wcex.hIconSm       = 0;

    RegisterClassEx(&wcex);
}

void displayWebView()
{
    ::InvalidateRect(webViewWindow, 0, TRUE);
    ::SendMessage(webViewWindow, WM_PAINT, 0, 0);
}

void dumpFrameScrollPosition(IWebFrame* frame)
{
    if (!frame)
        return;

    COMPtr<IWebFramePrivate> framePrivate;
    if (FAILED(frame->QueryInterface(&framePrivate)))
        return;

    SIZE scrollPosition;
    if (FAILED(framePrivate->scrollOffset(&scrollPosition)))
        return;

    if (abs(scrollPosition.cx) > 0.00000001 || abs(scrollPosition.cy) > 0.00000001) {
        COMPtr<IWebFrame> parent;
        if (FAILED(frame->parentFrame(&parent)))
            return;
        if (parent) {
            _bstr_t name;
            if (FAILED(frame->name(&name.GetBSTR())))
                return;
            fprintf(testResult, "frame '%S' ", static_cast<wchar_t*>(name));
        }
        fprintf(testResult, "scrolled to %.f,%.f\n", (double)scrollPosition.cx, (double)scrollPosition.cy);
    }

    if (::gTestRunner->dumpChildFrameScrollPositions()) {
        COMPtr<IEnumVARIANT> enumKids;
        if (FAILED(frame->childFrames(&enumKids)))
            return;
        _variant_t var;
        while (enumKids->Next(1, &var.GetVARIANT(), nullptr) == S_OK) {
            ASSERT(V_VT(&var) == VT_UNKNOWN);
            COMPtr<IWebFrame> framePtr;
            V_UNKNOWN(&var)->QueryInterface(IID_IWebFrame, (void**)&framePtr);
            dumpFrameScrollPosition(framePtr.get());
        }
    }
}

static wstring dumpFramesAsText(IWebFrame* frame)
{
    if (!frame)
        return L"";

    COMPtr<IDOMDocument> document;
    if (FAILED(frame->DOMDocument(&document)))
        return L"";

    COMPtr<IDOMElement> documentElement;
    if (FAILED(document->documentElement(&documentElement)))
        return L"";

    wstring result;

    // Add header for all but the main frame.
    COMPtr<IWebFrame> parent;
    if (FAILED(frame->parentFrame(&parent)))
        return L"";

    if (parent) {
        _bstr_t name;
        if (FAILED(frame->name(&name.GetBSTR())))
            return L"";

        result.append(L"\n--------\nFrame: '");
        result.append(static_cast<wchar_t*>(name), name.length());
        result.append(L"'\n--------\n");
    }

    _bstr_t innerText;
    COMPtr<IDOMElementPrivate> docPrivate;
    if (SUCCEEDED(documentElement->QueryInterface(&docPrivate)))
        docPrivate->innerText(&innerText.GetBSTR());

    result.append(static_cast<wchar_t*>(innerText), innerText.length());
    result.append(L"\n");

    if (::gTestRunner->dumpChildFramesAsText()) {
        COMPtr<IEnumVARIANT> enumKids;
        if (FAILED(frame->childFrames(&enumKids)))
            return L"";

        _variant_t var;
        while (enumKids->Next(1, &var.GetVARIANT(), nullptr) == S_OK) {
            ASSERT(V_VT(&var) == VT_UNKNOWN);
            COMPtr<IWebFrame> framePtr;
            V_UNKNOWN(&var)->QueryInterface(IID_IWebFrame, (void**)&framePtr);
            result.append(dumpFramesAsText(framePtr.get()));
        }
    }

    return result;
}

static int compareHistoryItems(const void* item1, const void* item2)
{
    COMPtr<IWebHistoryItemPrivate> itemA;
    if (FAILED((*(COMPtr<IUnknown>*)item1)->QueryInterface(&itemA)))
        return 0;

    COMPtr<IWebHistoryItemPrivate> itemB;
    if (FAILED((*(COMPtr<IUnknown>*)item2)->QueryInterface(&itemB)))
        return 0;

    _bstr_t targetA;
    if (FAILED(itemA->target(&targetA.GetBSTR())))
        return 0;

    _bstr_t targetB;
    if (FAILED(itemB->target(&targetB.GetBSTR())))
        return 0;

    return wcsicmp(static_cast<wchar_t*>(targetA), static_cast<wchar_t*>(targetB));
}

static void dumpHistoryItem(IWebHistoryItem* item, int indent, bool current)
{
    ASSERT(item);

    int start = 0;
    if (current) {
        fprintf(testResult, "curr->");
        start = 6;
    }
    for (int i = start; i < indent; i++)
        fputc(' ', testResult);

    _bstr_t url;
    if (FAILED(item->URLString(&url.GetBSTR())))
        return;

    if (wcsstr(static_cast<wchar_t*>(url), L"file:/") == static_cast<wchar_t*>(url)) {
        static wchar_t* layoutTestsStringUnixPath = L"/LayoutTests/";
        static wchar_t* layoutTestsStringDOSPath = L"\\LayoutTests\\";
        
        wchar_t* result = wcsstr(static_cast<wchar_t*>(url), layoutTestsStringUnixPath);
        if (!result)
            result = wcsstr(static_cast<wchar_t*>(url), layoutTestsStringDOSPath);
        if (!result)
            return;

        wchar_t* start = result + wcslen(layoutTestsStringUnixPath);

        url = _bstr_t(L"(file test):") + _bstr_t(start);
    }

    fprintf(testResult, "%S", static_cast<wchar_t*>(url));

    COMPtr<IWebHistoryItemPrivate> itemPrivate;
    if (FAILED(item->QueryInterface(&itemPrivate)))
        return;

    _bstr_t target;
    if (FAILED(itemPrivate->target(&target.GetBSTR())))
        return;
    if (target.length())
        fprintf(testResult, " (in frame \"%S\")", static_cast<wchar_t*>(target));
    BOOL isTargetItem = FALSE;
    if (FAILED(itemPrivate->isTargetItem(&isTargetItem)))
        return;
    if (isTargetItem)
        fprintf(testResult, "  **nav target**");
    fputc('\n', testResult);

    unsigned kidsCount;
    SAFEARRAY* arrPtr;
    if (FAILED(itemPrivate->children(&kidsCount, &arrPtr)) || !kidsCount)
        return;

    Vector<COMPtr<IUnknown> > kidsVector;

    LONG lowerBound;
    if (FAILED(::SafeArrayGetLBound(arrPtr, 1, &lowerBound)))
        goto exit;

    LONG upperBound;
    if (FAILED(::SafeArrayGetUBound(arrPtr, 1, &upperBound)))
        goto exit;

    LONG length = upperBound - lowerBound + 1;
    if (!length)
        goto exit;
    ASSERT(length == kidsCount);

    IUnknown** safeArrayData;
    if (FAILED(::SafeArrayAccessData(arrPtr, (void**)&safeArrayData)))
        goto exit;

    for (int i = 0; i < length; ++i)
        kidsVector.append(safeArrayData[i]);
    ::SafeArrayUnaccessData(arrPtr);

    // must sort to eliminate arbitrary result ordering which defeats reproducible testing
    qsort(kidsVector.data(), kidsCount, sizeof(kidsVector[0]), compareHistoryItems);

    for (unsigned i = 0; i < kidsCount; ++i) {
        COMPtr<IWebHistoryItem> item;
        kidsVector[i]->QueryInterface(&item);
        dumpHistoryItem(item.get(), indent + 4, false);
    }

exit:
    if (arrPtr && SUCCEEDED(::SafeArrayUnlock(arrPtr)))
        ::SafeArrayDestroy(arrPtr);
}

static void dumpBackForwardList(IWebView* webView)
{
    ASSERT(webView);

    fprintf(testResult, "\n============== Back Forward List ==============\n");

    COMPtr<IWebBackForwardList> bfList;
    if (FAILED(webView->backForwardList(&bfList)))
        return;

    // Print out all items in the list after prevTestBFItem, which was from the previous test
    // Gather items from the end of the list, the print them out from oldest to newest

    Vector<COMPtr<IUnknown> > itemsToPrint;

    int forwardListCount;
    if (FAILED(bfList->forwardListCount(&forwardListCount)))
        return;

    for (int i = forwardListCount; i > 0; --i) {
        COMPtr<IWebHistoryItem> item;
        if (FAILED(bfList->itemAtIndex(i, &item)))
            return;
        // something is wrong if the item from the last test is in the forward part of the b/f list
        ASSERT(item != prevTestBFItem);
        COMPtr<IUnknown> itemUnknown;
        item->QueryInterface(&itemUnknown);
        itemsToPrint.append(itemUnknown);
    }

    COMPtr<IWebHistoryItem> currentItem;
    if (FAILED(bfList->currentItem(&currentItem)))
        return;

    ASSERT(currentItem != prevTestBFItem);
    COMPtr<IUnknown> currentItemUnknown;
    currentItem->QueryInterface(&currentItemUnknown);
    itemsToPrint.append(currentItemUnknown);
    int currentItemIndex = itemsToPrint.size() - 1;

    int backListCount;
    if (FAILED(bfList->backListCount(&backListCount)))
        return;

    for (int i = -1; i >= -backListCount; --i) {
        COMPtr<IWebHistoryItem> item;
        if (FAILED(bfList->itemAtIndex(i, &item)))
            return;
        if (item == prevTestBFItem)
            break;
        COMPtr<IUnknown> itemUnknown;
        item->QueryInterface(&itemUnknown);
        itemsToPrint.append(itemUnknown);
    }

    for (int i = itemsToPrint.size() - 1; i >= 0; --i) {
        COMPtr<IWebHistoryItem> historyItemToPrint;
        itemsToPrint[i]->QueryInterface(&historyItemToPrint);
        dumpHistoryItem(historyItemToPrint.get(), 8, i == currentItemIndex);
    }

    fprintf(testResult, "===============================================\n");
}

static void dumpBackForwardListForAllWindows()
{
    unsigned count = openWindows().size();
    for (unsigned i = 0; i < count; i++) {
        HWND window = openWindows()[i];
        IWebView* webView = windowToWebViewMap().get(window);
        dumpBackForwardList(webView);
    }
}

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (!waitToDumpWatchdog)
        return;

    KillTimer(0, waitToDumpWatchdog);
    waitToDumpWatchdog = 0;
}

void dump()
{
    if (done) {
        fprintf(stderr, "dump() has already been called!\n");
        return;
    }

    ::InvalidateRect(webViewWindow, 0, TRUE);
    ::SendMessage(webViewWindow, WM_PAINT, 0, 0);

    invalidateAnyPreviousWaitToDumpWatchdog();
    ASSERT(!::gTestRunner->hasPendingWebNotificationClick());

    if (dumpTree) {
        _bstr_t resultString;

        COMPtr<IWebDataSource> dataSource;
        if (SUCCEEDED(frame->dataSource(&dataSource))) {
            COMPtr<IWebURLResponse> response;
            if (SUCCEEDED(dataSource->response(&response)) && response) {
                _bstr_t mimeType;
                if (SUCCEEDED(response->MIMEType(&mimeType.GetBSTR())) && !_tcscmp(static_cast<TCHAR*>(mimeType), TEXT("text/plain"))) {
                    ::gTestRunner->setDumpAsText(true);
                    ::gTestRunner->setGeneratePixelResults(false);
                }
            }
        }

        if (::gTestRunner->dumpAsText()) {
            resultString = dumpFramesAsText(frame).data();
        } else {
            COMPtr<IWebFramePrivate> framePrivate;
            if (FAILED(frame->QueryInterface(&framePrivate)))
                goto fail;
            framePrivate->renderTreeAsExternalRepresentation(::gTestRunner->isPrinting(), &resultString.GetBSTR());
        }

        if (resultString.length()) {
            unsigned stringLength = resultString.length();
            int bufferSize = ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, 0, 0, 0, 0);
            char* buffer = (char*)malloc(bufferSize + 1);
            ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, buffer, bufferSize + 1, 0, 0);
            fwrite(buffer, 1, bufferSize, testResult);
            free(buffer);

            if (!::gTestRunner->dumpAsText() && !::gTestRunner->dumpDOMAsWebArchive() && !::gTestRunner->dumpSourceAsWebArchive() && !::gTestRunner->dumpAsAudio())
                dumpFrameScrollPosition(frame);

            if (::gTestRunner->dumpBackForwardList())
                dumpBackForwardListForAllWindows();
        } else
            fprintf(testResult, "ERROR: nil result from %s\n", ::gTestRunner->dumpAsText() ? "IDOMElement::innerText" : "IFrameViewPrivate::renderTreeAsExternalRepresentation");

        if (printSeparators)
            fputs("#EOF\n", testResult); // terminate the content block
    }

    if (dumpPixelsForCurrentTest && ::gTestRunner->generatePixelResults()) {
        // FIXME: when isPrinting is set, dump the image with page separators.
        dumpWebViewAsPixelsAndCompareWithExpected(gTestRunner->expectedPixelHash());
    }

    fputs("#EOF\n", testResult); // terminate the (possibly empty) pixels block
    fflush(testResult);

fail:
    // This will exit from our message loop.
    done = true;
}

static bool shouldLogFrameLoadDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "/loading/") || strstr(pathOrURL, "\\loading\\");
}

static bool shouldLogHistoryDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "/globalhistory/") || strstr(pathOrURL, "\\globalhistory\\");
}

static bool shouldDumpAsText(const char* pathOrURL)
{
    return strstr(pathOrURL, "/dumpAsText/") || strstr(pathOrURL, "\\dumpAsText\\");
}

static bool shouldEnableDeveloperExtras(const char* pathOrURL)
{
    return true;
}

static void enableExperimentalFeatures(IWebPreferences* preferences)
{
    COMPtr<IWebPreferencesPrivate7> prefsPrivate { Query, preferences };

    prefsPrivate->setFetchAPIKeepAliveEnabled(TRUE);
    // FIXME: CSSGridLayout
    // FIXME: SpringTimingFunction
    // FIXME: Gamepads
    prefsPrivate->setLinkPreloadEnabled(TRUE);
    prefsPrivate->setMediaPreloadingEnabled(TRUE);
    // FIXME: ModernMediaControls
    // FIXME: InputEvents
    // FIXME: SubtleCrypto
    prefsPrivate->setVisualViewportAPIEnabled(TRUE);
    prefsPrivate->setCSSOMViewScrollingAPIEnabled(TRUE);
    prefsPrivate->setWebAnimationsEnabled(TRUE);
    prefsPrivate->setServerTimingEnabled(TRUE);
    // FIXME: WebGL2
    // FIXME: WebRTC
}

static void resetWebPreferencesToConsistentValues(IWebPreferences* preferences)
{
    ASSERT(preferences);

    enableExperimentalFeatures(preferences);

    preferences->setAutosaves(FALSE);

    COMPtr<IWebPreferencesPrivate6> prefsPrivate(Query, preferences);
    ASSERT(prefsPrivate);
    prefsPrivate->setFullScreenEnabled(TRUE);

#ifdef USE_MAC_FONTS
    static _bstr_t standardFamily(TEXT("Times"));
    static _bstr_t fixedFamily(TEXT("Courier"));
    static _bstr_t sansSerifFamily(TEXT("Helvetica"));
    static _bstr_t cursiveFamily(TEXT("Apple Chancery"));
    static _bstr_t fantasyFamily(TEXT("Papyrus"));
    static _bstr_t pictographFamily(TEXT("Apple Color Emoji"));
#else
    static _bstr_t standardFamily(TEXT("Times New Roman"));
    static _bstr_t fixedFamily(TEXT("Courier New"));
    static _bstr_t sansSerifFamily(TEXT("Arial"));
    static _bstr_t cursiveFamily(TEXT("Comic Sans MS")); // Not actually cursive, but it's what IE and Firefox use.
    static _bstr_t fantasyFamily(TEXT("Times New Roman"));
    static _bstr_t pictographFamily(TEXT("Segoe UI Symbol"));
#endif

    prefsPrivate->setAllowUniversalAccessFromFileURLs(TRUE);
    prefsPrivate->setAllowFileAccessFromFileURLs(TRUE);
    preferences->setStandardFontFamily(standardFamily);
    preferences->setFixedFontFamily(fixedFamily);
    preferences->setSerifFontFamily(standardFamily);
    preferences->setSansSerifFontFamily(sansSerifFamily);
    preferences->setCursiveFontFamily(cursiveFamily);
    preferences->setFantasyFontFamily(fantasyFamily);
    preferences->setPictographFontFamily(pictographFamily);
    preferences->setDefaultFontSize(16);
    preferences->setDefaultFixedFontSize(13);
    preferences->setMinimumFontSize(0);
    preferences->setDefaultTextEncodingName(L"ISO-8859-1");
    preferences->setJavaEnabled(FALSE);
    preferences->setJavaScriptEnabled(TRUE);
    preferences->setEditableLinkBehavior(WebKitEditableLinkOnlyLiveWithShiftKey);
    preferences->setTabsToLinks(FALSE);
    preferences->setDOMPasteAllowed(TRUE);
    preferences->setShouldPrintBackgrounds(TRUE);
    preferences->setCacheModel(WebCacheModelDocumentBrowser);
    prefsPrivate->setXSSAuditorEnabled(FALSE);
    prefsPrivate->setExperimentalNotificationsEnabled(FALSE);
    preferences->setPlugInsEnabled(TRUE);
    preferences->setTextAreasAreResizable(TRUE);
    preferences->setUsesPageCache(FALSE);

    preferences->setPrivateBrowsingEnabled(FALSE);
    prefsPrivate->setAuthorAndUserStylesEnabled(TRUE);
    // Shrinks standalone images to fit: YES
    preferences->setJavaScriptCanOpenWindowsAutomatically(TRUE);
    prefsPrivate->setJavaScriptCanAccessClipboard(TRUE);
    prefsPrivate->setOfflineWebApplicationCacheEnabled(TRUE);
    prefsPrivate->setDeveloperExtrasEnabled(FALSE);
    prefsPrivate->setJavaScriptRuntimeFlags(WebKitJavaScriptRuntimeFlagsAllEnabled);
    // Set JS experiments enabled: YES
    preferences->setLoadsImagesAutomatically(TRUE);
    prefsPrivate->setLoadsSiteIconsIgnoringImageLoadingPreference(FALSE);
    prefsPrivate->setFrameFlatteningEnabled(FALSE);
    prefsPrivate->setSpatialNavigationEnabled(FALSE);
    if (persistentUserStyleSheetLocation) {
        size_t stringLength = CFStringGetLength(persistentUserStyleSheetLocation.get());
        Vector<UniChar> urlCharacters(stringLength + 1, 0);
        CFStringGetCharacters(persistentUserStyleSheetLocation.get(), CFRangeMake(0, stringLength), urlCharacters.data());
        _bstr_t url(reinterpret_cast<wchar_t*>(urlCharacters.data()));
        preferences->setUserStyleSheetLocation(url);
        preferences->setUserStyleSheetEnabled(TRUE);
    } else
        preferences->setUserStyleSheetEnabled(FALSE);

#if USE(CG)
    prefsPrivate->setAcceleratedCompositingEnabled(TRUE);
#endif
    // Set WebGL Enabled: NO
    preferences->setCSSRegionsEnabled(TRUE);
    // Set uses HTML5 parser quirks: NO
    // Async spellcheck: NO
    prefsPrivate->setMockScrollbarsEnabled(TRUE);

    preferences->setFontSmoothing(FontSmoothingTypeStandard);

    prefsPrivate->setFetchAPIEnabled(TRUE);
    prefsPrivate->setShadowDOMEnabled(TRUE);
    prefsPrivate->setCustomElementsEnabled(TRUE);
    prefsPrivate->setResourceTimingEnabled(TRUE);
    prefsPrivate->setUserTimingEnabled(TRUE);
    prefsPrivate->setDataTransferItemsEnabled(TRUE);
    prefsPrivate->clearNetworkLoaderSession();

    setAlwaysAcceptCookies(false);
}

static void setWebPreferencesForTestOptions(IWebPreferences* preferences, const TestOptions& options)
{
    COMPtr<IWebPreferencesPrivate6> prefsPrivate { Query, preferences };

    prefsPrivate->setMenuItemElementEnabled(options.enableMenuItemElement);
    prefsPrivate->setModernMediaControlsEnabled(options.enableModernMediaControls);
    prefsPrivate->setIsSecureContextAttributeEnabled(options.enableIsSecureContextAttribute);
    prefsPrivate->setInspectorAdditionsEnabled(options.enableInspectorAdditions);
}

static String applicationId()
{
    DWORD processId = ::GetCurrentProcessId();
    return String::format("com.apple.DumpRenderTree.%d", processId);
}

static void setApplicationId()
{
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, (void**)&preferences))) {
        COMPtr<IWebPreferencesPrivate4> prefsPrivate4(Query, preferences);
        ASSERT(prefsPrivate4);
        _bstr_t fileName = applicationId().charactersWithNullTermination().data();
        prefsPrivate4->setApplicationId(fileName);
    }
}

static void setCacheFolder()
{
    String libraryPath = libraryPathForDumpRenderTree();

    COMPtr<IWebCache> webCache;
    if (SUCCEEDED(WebKitCreateInstance(CLSID_WebCache, 0, IID_IWebCache, (void**)&webCache))) {
        _bstr_t cacheFolder = WebCore::FileSystem::pathByAppendingComponent(libraryPath, "LocalCache").utf8().data();
        webCache->setCacheFolder(cacheFolder);
    }
}

// Called once on DumpRenderTree startup.
static void setDefaultsToConsistentValuesForTesting()
{
#if USE(CF)
    // Create separate preferences for each DRT instance
    setApplicationId();

    RetainPtr<CFStringRef> appId = applicationId().createCFString();

    String libraryPath = libraryPathForDumpRenderTree();

    // Set up these values before creating the WebView so that the various initializations will see these preferred values.
    CFPreferencesSetAppValue(WebDatabaseDirectoryDefaultsKey, WebCore::FileSystem::pathByAppendingComponent(libraryPath, "Databases").createCFString().get(), appId.get());
    CFPreferencesSetAppValue(WebStorageDirectoryDefaultsKey, WebCore::FileSystem::pathByAppendingComponent(libraryPath, "LocalStorage").createCFString().get(), appId.get());
    CFPreferencesSetAppValue(WebKitLocalCacheDefaultsKey, WebCore::FileSystem::pathByAppendingComponent(libraryPath, "LocalCache").createCFString().get(), appId.get());
#endif
}

static void resetWebViewToConsistentStateBeforeTesting(const TestOptions& options)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView))) 
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (SUCCEEDED(webView->QueryInterface(&viewEditing)) && viewEditing) {

        viewEditing->setEditable(FALSE);

        COMPtr<IWebEditingDelegate> delegate;
        if (SUCCEEDED(viewEditing->editingDelegate(&delegate)) && delegate) {
            COMPtr<EditingDelegate> editingDelegate(Query, viewEditing.get());
            if (editingDelegate)
                editingDelegate->setAcceptsEditing(TRUE);
        }
    }

    COMPtr<IWebIBActions> webIBActions(Query, webView);
    if (webIBActions) {
        webIBActions->makeTextStandardSize(0);
        webIBActions->resetPageZoom(0);
    }

    COMPtr<IWebViewPrivate2> webViewPrivate(Query, webView);
    if (webViewPrivate) {
        POINT origin = { 0, 0 };
        webViewPrivate->scaleWebView(1.0, origin);
        webViewPrivate->setCustomBackingScaleFactor(0);
        webViewPrivate->setTabKeyCyclesThroughElements(TRUE);
    }

    webView->setPolicyDelegate(nullptr);
    policyDelegate->setPermissive(false);
    policyDelegate->setControllerToNotifyDone(nullptr);
    sharedFrameLoadDelegate->resetToConsistentState();

    if (webViewPrivate) {
        webViewPrivate->clearMainFrameName();
        _bstr_t groupName;
        if (SUCCEEDED(webView->groupName(&groupName.GetBSTR())))
            webViewPrivate->removeAllUserContentFromGroup(groupName);
    }

    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(webView->preferences(&preferences))) {
        resetWebPreferencesToConsistentValues(preferences.get());
        setWebPreferencesForTestOptions(preferences.get(), options);
    }

    TestRunner::setSerializeHTTPLoads(false);

    setlocale(LC_ALL, "");

    if (::gTestRunner) {
        JSGlobalContextRef context = frame->globalContext();
        WebCoreTestSupport::resetInternalsObject(context);
    }

    if (preferences) {
        preferences->setContinuousSpellCheckingEnabled(TRUE);
        // Automatic Quote Subs
        // Automatic Link Detection
        // Autommatic Dash substitution
        // Automatic Spell Check
        preferences->setGrammarCheckingEnabled(TRUE);
        // Use Test Mode Focus Ring
    }

    HWND viewWindow;
    if (webViewPrivate && SUCCEEDED(webViewPrivate->viewWindow(&viewWindow)) && viewWindow)
        ::SetFocus(viewWindow);

    webViewPrivate->resetOriginAccessWhitelists();

    sharedUIDelegate->resetUndoManager();

    COMPtr<IWebFramePrivate> framePrivate;
    if (SUCCEEDED(frame->QueryInterface(&framePrivate)))
        framePrivate->clearOpener();

    COMPtr<IWebViewPrivate5> webViewPrivate5(Query, webView);
    webViewPrivate5->exitFullscreenIfNeeded();
}

static void sizeWebViewForCurrentTest()
{
    bool isSVGW3CTest = (::gTestRunner->testURL().find("svg\\W3C-SVG-1.1") != string::npos)
        || (::gTestRunner->testURL().find("svg/W3C-SVG-1.1") != string::npos);
    unsigned width = isSVGW3CTest ? TestRunner::w3cSVGViewWidth : TestRunner::viewWidth;
    unsigned height = isSVGW3CTest ? TestRunner::w3cSVGViewHeight : TestRunner::viewHeight;

    ::SetWindowPos(webViewWindow, 0, 0, 0, width, height, SWP_NOMOVE);
}

static String findFontFallback(const char* pathOrUrl)
{
    String pathToFontFallback = WebCore::FileSystem::directoryName(pathOrUrl);

    wchar_t fullPath[_MAX_PATH];
    if (!_wfullpath(fullPath, pathToFontFallback.charactersWithNullTermination().data(), _MAX_PATH))
        return emptyString();

    if (!::PathIsDirectoryW(fullPath))
        return emptyString();

    String pathToCheck = fullPath;

    static const String layoutTests = "LayoutTests";

    // Find the layout test root on the current path:
    size_t location = pathToCheck.find(layoutTests);
    if (WTF::notFound == location)
        return emptyString();

    String pathToTest = pathToCheck.substring(location + layoutTests.length() + 1);
    String possiblePathToLogue = WebCore::FileSystem::pathByAppendingComponent(pathToCheck.substring(0, location + layoutTests.length() + 1), "platform\\win");

    Vector<String> possiblePaths;
    possiblePaths.append(WebCore::FileSystem::pathByAppendingComponent(possiblePathToLogue, pathToTest));

    size_t nextCandidateEnd = pathToTest.reverseFind('\\');
    while (nextCandidateEnd && nextCandidateEnd != WTF::notFound) {
        pathToTest = pathToTest.substring(0, nextCandidateEnd);
        possiblePaths.append(WebCore::FileSystem::pathByAppendingComponent(possiblePathToLogue, pathToTest));
        nextCandidateEnd = pathToTest.reverseFind('\\');
    }

    for (Vector<String>::iterator pos = possiblePaths.begin(); pos != possiblePaths.end(); ++pos) {
        pathToFontFallback = WebCore::FileSystem::pathByAppendingComponent(*pos, "resources\\");

        if (::PathIsDirectoryW(pathToFontFallback.charactersWithNullTermination().data()))
            return pathToFontFallback;
    }

    return emptyString();
}

static void addFontFallbackIfPresent(const String& fontFallbackPath)
{
    if (fontFallbackPath.isEmpty())
        return;

    String fontFallback = WebCore::FileSystem::pathByAppendingComponent(fontFallbackPath, "Mac-compatible-font-fallback.css");

    if (!::PathFileExistsW(fontFallback.charactersWithNullTermination().data()))
        return;

    ::setPersistentUserStyleSheetLocation(fontFallback.createCFString().get());
}

static void removeFontFallbackIfPresent(const String& fontFallbackPath)
{
    if (fontFallbackPath.isEmpty())
        return;

    String fontFallback = WebCore::FileSystem::pathByAppendingComponent(fontFallbackPath, "Mac-compatible-font-fallback.css");

    if (!::PathFileExistsW(fontFallback.charactersWithNullTermination().data()))
        return;

    ::setPersistentUserStyleSheetLocation(nullptr);
}

static bool handleControlCommand(const char* command)
{
    if (!strcmp("#CHECK FOR ABANDONED DOCUMENTS", command)) {
        // DumpRenderTree does not support checking for abandonded documents.
        String result("\n");
        printf("Content-Type: text/plain\n");
        printf("Content-Length: %u\n", result.length());
        fwrite(result.utf8().data(), 1, result.length(), stdout);
        printf("#EOF\n");
        fprintf(stderr, "#EOF\n");
        fflush(stdout);
        fflush(stderr);
        return true;
    }
    return false;
}

static void runTest(const string& inputLine)
{
    ASSERT(!inputLine.empty());

    TestCommand command = parseInputLine(inputLine);
    const string& pathOrURL = command.pathOrURL;
    dumpPixelsForCurrentTest = command.shouldDumpPixels || dumpPixelsForAllTests;

    static _bstr_t methodBStr(TEXT("GET"));

    CFStringRef str = CFStringCreateWithCString(0, pathOrURL.c_str(), kCFStringEncodingWindowsLatin1);
    if (!str) {
        fprintf(stderr, "Failed to parse \"%s\" as UTF-8\n", pathOrURL.c_str());
        return;
    }

    CFURLRef url = CFURLCreateWithString(0, str, 0);

    if (!url)
        url = CFURLCreateWithFileSystemPath(0, str, kCFURLWindowsPathStyle, false);

    CFRelease(str);

    if (!url) {
        fprintf(stderr, "Failed to parse \"%s\" as a URL\n", pathOrURL.c_str());
        return;
    }

    String hostName = String(adoptCF(CFURLCopyHostName(url)).get());

    String fallbackPath = findFontFallback(pathOrURL.c_str());

    str = CFURLGetString(url);

    CFIndex length = CFStringGetLength(str);

    Vector<UniChar> buffer(length + 1, 0);
    CFStringGetCharacters(str, CFRangeMake(0, length), buffer.data());

    _bstr_t urlBStr(reinterpret_cast<wchar_t*>(buffer.data()));
    ASSERT(urlBStr.length() == length);

    CFIndex maximumURLLengthAsUTF8 = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    Vector<char> testURL(maximumURLLengthAsUTF8 + 1, 0);
    CFStringGetCString(str, testURL.data(), maximumURLLengthAsUTF8, kCFStringEncodingUTF8);

    CFRelease(url);

    TestOptions options { command.pathOrURL, command.absolutePath };

    resetWebViewToConsistentStateBeforeTesting(options);

    ::gTestRunner = TestRunner::create(testURL.data(), command.expectedPixelHash);
    ::gTestRunner->setCustomTimeout(command.timeout);
    ::gTestRunner->setDumpJSConsoleLogInStdErr(command.dumpJSConsoleLogInStdErr);

    topLoadingFrame = nullptr;
    done = false;

    addFontFallbackIfPresent(fallbackPath);

    sizeWebViewForCurrentTest();
    ::gTestRunner->setIconDatabaseEnabled(false);
    ::gTestRunner->clearAllApplicationCaches();

    if (shouldLogFrameLoadDelegates(pathOrURL.c_str()))
        ::gTestRunner->setDumpFrameLoadCallbacks(true);

    COMPtr<IWebView> webView;
    if (SUCCEEDED(frame->webView(&webView))) {
        COMPtr<IWebViewPrivate2> viewPrivate;
        if (SUCCEEDED(webView->QueryInterface(&viewPrivate))) {
            if (shouldLogHistoryDelegates(pathOrURL.c_str())) {
                ::gTestRunner->setDumpHistoryDelegateCallbacks(true);            
                viewPrivate->setHistoryDelegate(sharedHistoryDelegate.get());
            } else
                viewPrivate->setHistoryDelegate(nullptr);
        }
    }

    if (shouldEnableDeveloperExtras(pathOrURL.c_str())) {
        ::gTestRunner->setDeveloperExtrasEnabled(true);
        if (shouldDumpAsText(pathOrURL.c_str())) {
            ::gTestRunner->setDumpAsText(true);
            ::gTestRunner->setGeneratePixelResults(false);
        }
    }

    COMPtr<IWebHistory> history;
    if (SUCCEEDED(WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(history), reinterpret_cast<void**>(&history))))
        history->setOptionalSharedHistory(nullptr);

    prevTestBFItem = nullptr;
    COMPtr<IWebBackForwardList> bfList;
    if (webView && SUCCEEDED(webView->backForwardList(&bfList)))
        bfList->currentItem(&prevTestBFItem);

    auto& workQueue = WorkQueue::singleton();
    workQueue.clear();
    workQueue.setFrozen(false);

    MSG msg = { 0 };
    HWND hostWindow;
    webView->hostWindow(&hostWindow);

    COMPtr<IWebMutableURLRequest> request, emptyRequest;
    HRESULT hr = WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        goto exit;

    request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 60);
    request->setHTTPMethod(methodBStr);
    if (hostName == "localhost" || hostName == "127.0.0.1")
        request->setAllowsAnyHTTPSCertificate();
    frame->loadRequest(request.get());

    while (!done) {
#if USE(CF)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
        if (!::GetMessage(&msg, 0, 0, 0))
            break;

        // We get spurious WM_MOUSELEAVE events which make event handling machinery think that mouse button
        // is released during dragging (see e.g. fast\dynamic\layer-hit-test-crash.html).
        // Mouse can never leave WebView during normal DumpRenderTree operation, so we just ignore all such events.
        if (msg.message == WM_MOUSELEAVE)
            continue;
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    // EventSendingController clearSavedEvents
    workQueue.clear();

    // If the test page could have possibly opened the Web Inspector frontend,
    // then try to close it in case it was accidentally left open.
    if (shouldEnableDeveloperExtras(pathOrURL.c_str())) {
        ::gTestRunner->closeWebInspector();
        ::gTestRunner->setDeveloperExtrasEnabled(false);
    }

    if (::gTestRunner->closeRemainingWindowsWhenComplete()) {
        Vector<HWND> windows = openWindows();
        unsigned size = windows.size();
        for (unsigned i = 0; i < size; i++) {
            HWND window = windows[i];

            // Don't try to close the main window
            if (window == hostWindow)
                continue;

            ::DestroyWindow(window);
        }
    }

    resetWebViewToConsistentStateBeforeTesting(options);

    // Loading an empty request synchronously replaces the document with a blank one, which is necessary
    // to stop timers, WebSockets and other activity that could otherwise spill output into next test's results.
    if (SUCCEEDED(WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&emptyRequest))) {
        _bstr_t emptyURL(L"");
        emptyRequest->initWithURL(emptyURL.GetBSTR(), WebURLRequestUseProtocolCachePolicy, 60);
        emptyRequest->setHTTPMethod(methodBStr);
        frame->loadRequest(emptyRequest.get());
    }

    // We should only have our main window left open when we're done
    ASSERT(openWindows().size() == 1);
    ASSERT(openWindows()[0] == hostWindow);

exit:
    removeFontFallbackIfPresent(fallbackPath);
    ::gTestRunner->cleanup();
    ::gTestRunner = nullptr;

    if (gcBetweenTests) {
        GCController gcController;
        gcController.collect();
    }
    JSC::waitForVMDestruction();

    fputs("#EOF\n", stderr);
    fflush(stderr);
}

Vector<HWND>& openWindows()
{
    static NeverDestroyed<Vector<HWND>> vector;
    return vector;
}

WindowToWebViewMap& windowToWebViewMap()
{
    static NeverDestroyed<WindowToWebViewMap> map;
    return map;
}

IWebView* createWebViewAndOffscreenWindow(HWND* webViewWindow)
{
    int maxViewWidth = TestRunner::viewWidth;
    int maxViewHeight = TestRunner::viewHeight;
    HWND hostWindow = (showWebView) ? CreateWindowEx(WS_EX_TOOLWINDOW, kDumpRenderTreeClassName, TEXT("DumpRenderTree"), WS_POPUP, 100, 100, maxViewWidth, maxViewHeight, 0, 0, ::GetModuleHandle(0), nullptr)
        : CreateWindowEx(WS_EX_TOOLWINDOW, kDumpRenderTreeClassName, TEXT("DumpRenderTree"), WS_POPUP, -maxViewWidth, -maxViewHeight, maxViewWidth, maxViewHeight, 0, 0, ::GetModuleHandle(0), nullptr);

    IWebView* webView = nullptr;
    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&webView);
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to create CLSID_WebView instance, error 0x%x\n", hr);
        return nullptr;
    }

    if (FAILED(webView->setHostWindow(hostWindow)))
        return nullptr;

    RECT clientRect;
    clientRect.bottom = clientRect.left = clientRect.top = clientRect.right = 0;
    _bstr_t groupName(L"org.webkit.DumpRenderTree");
    if (FAILED(webView->initWithFrame(clientRect, 0, groupName)))
        return nullptr;

    COMPtr<IWebViewPrivate2> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return nullptr;

    viewPrivate->setShouldApplyMacFontAscentHack(TRUE);
    viewPrivate->setAlwaysUsesComplexTextCodePath(forceComplexText);
    viewPrivate->setCustomBackingScaleFactor(1.0);

    _bstr_t pluginPath = _bstr_t(exePath().data()) + TestPluginDir;
    if (FAILED(viewPrivate->addAdditionalPluginDirectory(pluginPath.GetBSTR())))
        return nullptr;

    HWND viewWindow;
    if (FAILED(viewPrivate->viewWindow(&viewWindow)))
        return nullptr;
    if (webViewWindow)
        *webViewWindow = viewWindow;

    ::SetWindowPos(viewWindow, 0, 0, 0, maxViewWidth, maxViewHeight, 0);
    ::ShowWindow(hostWindow, SW_SHOW);

    if (FAILED(webView->setUIDelegate(sharedUIDelegate.get())))
        return nullptr;

    if (FAILED(webView->setFrameLoadDelegate(sharedFrameLoadDelegate.get())))
        return nullptr;

    if (FAILED(viewPrivate->setFrameLoadDelegatePrivate(sharedFrameLoadDelegate.get())))
        return nullptr;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return nullptr;

    if (FAILED(viewEditing->setEditingDelegate(sharedEditingDelegate.get())))
        return nullptr;

    if (FAILED(webView->setResourceLoadDelegate(resourceLoadDelegate.get())))
        return nullptr;

    viewPrivate->setDefersCallbacks(FALSE);

    openWindows().append(hostWindow);
    windowToWebViewMap().set(hostWindow, webView);
    return webView;
}

#if USE(CFURLCONNECTION)
RetainPtr<CFURLCacheRef> sharedCFURLCache()
{
#ifndef DEBUG_ALL
    HMODULE module = GetModuleHandle(TEXT("CFNetwork.dll"));
#else
    HMODULE module = GetModuleHandle(TEXT("CFNetwork_debug.dll"));
#endif
    if (!module)
        return nullptr;

    typedef CFURLCacheRef (*CFURLCacheCopySharedURLCacheProcPtr)(void);
    if (CFURLCacheCopySharedURLCacheProcPtr copyCache = reinterpret_cast<CFURLCacheCopySharedURLCacheProcPtr>(GetProcAddress(module, "CFURLCacheCopySharedURLCache")))
        return adoptCF(copyCache());

    typedef CFURLCacheRef (*CFURLCacheSharedURLCacheProcPtr)(void);
    if (CFURLCacheSharedURLCacheProcPtr sharedCache = reinterpret_cast<CFURLCacheSharedURLCacheProcPtr>(GetProcAddress(module, "CFURLCacheSharedURLCache")))
        return sharedCache();

    return nullptr;
}
#endif

static Vector<const char*> initializeGlobalsFromCommandLineOptions(int argc, const char* argv[])
{
    Vector<const char*> tests;

    for (int i = 1; i < argc; ++i) {
        if (!stricmp(argv[i], "--notree")) {
            dumpTree = false;
            continue;
        }

        if (!stricmp(argv[i], "--pixel-tests")) {
            dumpPixelsForAllTests = true;
            continue;
        }

        if (!stricmp(argv[i], "--tree")) {
            dumpTree = true;
            continue;
        }

        if (!stricmp(argv[i], "--threaded")) {
            threaded = true;
            continue;
        }

        if (!stricmp(argv[i], "--complex-text")) {
            forceComplexText = true;
            continue;
        }

        if (!stricmp(argv[i], "--accelerated-drawing")) {
            useAcceleratedDrawing = true;
            continue;
        }

        if (!stricmp(argv[i], "--gc-between-tests")) {
            gcBetweenTests = true;
            continue;
        }

        if (!stricmp(argv[i], "--no-timeout")) {
            useTimeoutWatchdog = false;
            continue;
        }

        if (!stricmp(argv[i], "--dump-all-pixels")) {
            dumpAllPixels = true;
            continue;
        }

        if (!stricmp(argv[i], "--print-supported-features")) {
            printSupportedFeatures = true;
            continue;
        }

        if (!stricmp(argv[i], "--show-webview")) {
            showWebView = true;
            continue;
        }

        tests.append(argv[i]);
    }

    return tests;
}

static void allocateGlobalControllers()
{
    sharedFrameLoadDelegate.adoptRef(new FrameLoadDelegate);
    sharedUIDelegate.adoptRef(new UIDelegate);
    sharedEditingDelegate.adoptRef(new EditingDelegate);
    resourceLoadDelegate.adoptRef(new ResourceLoadDelegate);
    policyDelegate = new PolicyDelegate();
    sharedHistoryDelegate.adoptRef(new HistoryDelegate);
    // storage delegate
    // policy delegate
}

static void prepareConsistentTestingEnvironment(IWebPreferences* standardPreferences, IWebPreferencesPrivate* standardPreferencesPrivate)
{
    ASSERT(standardPreferences);
    ASSERT(standardPreferencesPrivate);
    standardPreferences->setAutosaves(FALSE);

#if USE(CFURLCONNECTION)
    auto newCache = adoptCF(CFURLCacheCreate(kCFAllocatorDefault, 1024 * 1024, 0, nullptr));
    CFURLCacheSetSharedURLCache(newCache.get());
#endif

    COMPtr<IWebPreferencesPrivate4> prefsPrivate4(Query, standardPreferences);
    prefsPrivate4->switchNetworkLoaderToNewTestingSession();

    standardPreferences->setJavaScriptEnabled(TRUE);
    standardPreferences->setDefaultFontSize(16);
#if USE(CG)
    standardPreferences->setAcceleratedCompositingEnabled(TRUE);
    standardPreferences->setAVFoundationEnabled(TRUE);
#endif

    allocateGlobalControllers();
}

int main(int argc, const char* argv[])
{
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

    leakChecking = false;

    _setmode(1, _O_BINARY);
    _setmode(2, _O_BINARY);

    // Some tests are flaky because certain DLLs are writing to stdout, giving incorrect test results.
    // We work around that here by duplicating and redirecting stdout.
    int fdStdout = _dup(1);
    _setmode(fdStdout, _O_BINARY);
    testResult = fdopen(fdStdout, "a+b");
    // Redirect stdout to stderr.
    int result = _dup2(_fileno(stderr), 1);

    // Tests involving the clipboard are flaky when running with multiple DRTs, since the clipboard is global.
    // We can fix this by assigning each DRT a separate window station (each window station has its own clipboard).
    DWORD processId = ::GetCurrentProcessId();
    String windowStationName = String::format("windowStation%d", processId);
    String desktopName = String::format("desktop%d", processId);
    HDESK desktop = nullptr;

    auto windowsStation = ::CreateWindowStation(windowStationName.charactersWithNullTermination().data(), CWF_CREATE_ONLY, WINSTA_ALL_ACCESS, nullptr);
    if (windowsStation) {
        if (!::SetProcessWindowStation(windowsStation))
            fprintf(stderr, "SetProcessWindowStation failed with error %d\n", ::GetLastError());

        desktop = ::CreateDesktop(desktopName.charactersWithNullTermination().data(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
        if (!desktop)
            fprintf(stderr, "Failed to create desktop with error %d\n", ::GetLastError());
    } else {
        DWORD error = ::GetLastError();
        fprintf(stderr, "Failed to create window station with error %d\n", error);
        if (error == ERROR_ACCESS_DENIED)
            fprintf(stderr, "DumpRenderTree should be run as Administrator!\n");
    }

    initialize();

    setDefaultsToConsistentValuesForTesting();

    Vector<const char*> tests = initializeGlobalsFromCommandLineOptions(argc, argv);

    // FIXME - need to make DRT pass with Windows native controls <http://bugs.webkit.org/show_bug.cgi?id=25592>
    COMPtr<IWebPreferences> tmpPreferences;
    if (FAILED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, reinterpret_cast<void**>(&tmpPreferences))))
        return -1;
    COMPtr<IWebPreferences> standardPreferences;
    if (FAILED(tmpPreferences->standardPreferences(&standardPreferences)))
        return -2;
    COMPtr<IWebPreferencesPrivate> standardPreferencesPrivate;
    if (FAILED(standardPreferences->QueryInterface(&standardPreferencesPrivate)))
        return -3;

    prepareConsistentTestingEnvironment(standardPreferences.get(), standardPreferencesPrivate.get());

    if (printSupportedFeatures) {
        BOOL acceleratedCompositingAvailable = FALSE;
        standardPreferences->acceleratedCompositingEnabled(&acceleratedCompositingAvailable);

#if ENABLE(3D_TRANSFORMS)
        // In theory, we could have a software-based 3D rendering implementation that we use when
        // hardware-acceleration is not available. But we don't have any such software
        // implementation, so 3D rendering is only available when hardware-acceleration is.
        BOOL threeDTransformsAvailable = acceleratedCompositingAvailable;
#else
        BOOL threeDTransformsAvailable = FALSE;
#endif

        fprintf(testResult, "SupportedFeatures:%s %s\n", acceleratedCompositingAvailable ? "AcceleratedCompositing" : "", threeDTransformsAvailable ? "3DTransforms" : "");
        return 0;
    }

    COMPtr<IWebView> webView(AdoptCOM, createWebViewAndOffscreenWindow(&webViewWindow));
    if (!webView)
        return -4;

    if (FAILED(webView->mainFrame(&frame)))
        return -7;

#if USE(CFURLCONNECTION)
    RetainPtr<CFURLCacheRef> urlCache = sharedCFURLCache();
    CFURLCacheRemoveAllCachedResponses(urlCache.get());
#endif

#ifdef _DEBUG
    _CrtMemState entryToMainMemCheckpoint;
    if (leakChecking)
        _CrtMemCheckpoint(&entryToMainMemCheckpoint);
#endif

    if (threaded)
        startJavaScriptThreads();

    if (tests.size() == 1 && !strcmp(tests[0], "-")) {
        char filenameBuffer[2048];
        printSeparators = true;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char* newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';
            
            if (strlen(filenameBuffer) == 0)
                continue;

            if (handleControlCommand(filenameBuffer))
                continue;

            runTest(filenameBuffer);
        }
    } else {
        printSeparators = tests.size() > 1;
        for (int i = 0; i < tests.size(); i++)
            runTest(tests[i]);
    }

    if (threaded)
        stopJavaScriptThreads();
    
    delete policyDelegate;
    frame->Release();

#ifdef _DEBUG
    if (leakChecking) {
        // dump leaks to stderr
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtMemDumpAllObjectsSince(&entryToMainMemCheckpoint);
    }
#endif

    shutDownWebKit();
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    if (desktop)
        ::CloseDesktop(desktop);
    if (windowsStation)
        ::CloseWindowStation(windowsStation);

    ::OleUninitialize();

    return 0;
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, argv);
}
