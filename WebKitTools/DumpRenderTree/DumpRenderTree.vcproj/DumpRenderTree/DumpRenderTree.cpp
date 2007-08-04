/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "DumpRenderTree.h"

#include <wtf/Vector.h>

#include <WebCore/COMPtr.h>
#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <math.h>
#include <pthread.h>
#include <string>
#include <tchar.h>
#include <WebKit/DOMPrivate.h>
#include <WebKit/IWebFramePrivate.h>
#include <WebKit/IWebHistoryItem.h>
#include <WebKit/IWebHistoryItemPrivate.h>
#include <WebKit/IWebURLResponse.h>
#include <WebKit/IWebViewPrivate.h>
#include <WebKit/WebKit.h>
#include <windows.h>

#include "EditingDelegate.h"
#include "WaitUntilDoneDelegate.h"
#include "WorkQueueItem.h"
#include "WorkQueue.h"

using std::wstring;

#ifdef _DEBUG
const LPWSTR TestPluginDir = L"TestNetscapePlugin_Debug";
#else
const LPWSTR TestPluginDir = L"TestNetscapePlugin";
#endif

#define USE_MAC_FONTS

const LPCWSTR kDumpRenderTreeClassName = L"DumpRenderTreeWindow";

static bool dumpTree = true;
static bool printSeparators;
static bool leakChecking = false;
static bool timedOut = false;
static bool threaded = false;

static const char* currentTest;

bool done;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
IWebFrame* topLoadingFrame;     // !nil iff a load is in progress
static COMPtr<IWebHistoryItem> prevTestBFItem;  // current b/f item at the end of the previous test

bool dumpAsText = false;
bool waitToDump = false;
bool shouldDumpEditingCallbacks = false;
bool shouldDumpTitleChanges = false;
bool shouldDumpChildFrameScrollPositions = false;
bool shouldDumpChildFramesAsText = false;
bool shouldDumpBackForwardList = false;
bool testRepaint = false;
bool repaintSweepHorizontally = false;

IWebFrame* frame;
HWND webViewWindow;
static HWND hostWindow;

static const unsigned timeoutValue = 60000;
static const unsigned timeoutId = 10;

const unsigned maxViewWidth = 800;
const unsigned maxViewHeight = 600;

static LRESULT CALLBACK DumpRenderTreeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_TIMER:
            // The test ran long enough to time out
            timedOut = true;
            PostQuitMessage(0);
            return 0;
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

extern "C" BOOL InitializeCoreGraphics();

static wstring initialize(HMODULE hModule)
{
    static LPCTSTR fontsToInstall[] = {
        TEXT("AHEM____.ttf"),
        TEXT("Apple Chancery.ttf"),
        TEXT("Courier Bold.ttf"),
        TEXT("Courier.ttf"),
        TEXT("Helvetica Bold.ttf"),
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
        TEXT("Times Roman.ttf")
    };

    TCHAR buffer[MAX_PATH];
    GetModuleFileName(hModule, buffer, ARRAYSIZE(buffer));
    wstring exePath(buffer);
    int lastSlash = exePath.rfind('\\');
    if (lastSlash != -1 && lastSlash + 1< exePath.length())
        exePath = exePath.substr(0, lastSlash + 1);
    
    wstring resourcesPath(exePath + TEXT("DumpRenderTree.resources\\"));

    for (int i = 0; i < ARRAYSIZE(fontsToInstall); ++i)
        AddFontResourceEx(wstring(resourcesPath + fontsToInstall[i]).c_str(), FR_PRIVATE, 0);

    // Init COM
    OleInitialize(0);

    // Initialize CG
    InitializeCoreGraphics();

    // Register a host window
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DumpRenderTreeWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hModule;
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = kDumpRenderTreeClassName;
    wcex.hIconSm       = 0;

    RegisterClassEx(&wcex);

    hostWindow = CreateWindowEx(WS_EX_TOOLWINDOW, kDumpRenderTreeClassName, TEXT("DumpRenderTree"), WS_POPUP,
      -maxViewWidth, -maxViewHeight, maxViewWidth, maxViewHeight, 0, 0, hModule, 0);

    return exePath;
}

#include <stdio.h>

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
            BSTR name;
            if (FAILED(frame->name(&name)))
                return;
            printf("frame '%S' ", name ? name : L"");
            SysFreeString(name);
        }
        printf("scrolled to %.f,%.f\n", (double)scrollPosition.cx, (double)scrollPosition.cy);
    }

    if (shouldDumpChildFrameScrollPositions) {
        COMPtr<IEnumVARIANT> enumKids;
        if (FAILED(frame->childFrames(&enumKids)))
            return;
        VARIANT var;
        VariantInit(&var);
        while (enumKids->Next(1, &var, 0) == S_OK) {
            ASSERT(V_VT(&var) == VT_UNKNOWN);
            COMPtr<IWebFrame> framePtr;
            V_UNKNOWN(&var)->QueryInterface(IID_IWebFrame, (void**)&framePtr);
            dumpFrameScrollPosition(framePtr.get());
            VariantClear(&var);
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
        BSTR name = L"";
        if (FAILED(frame->name(&name)))
            return L"";

        result.append(L"\n--------\nFrame: '");
        result.append(name ? name : L"");
        result.append(L"'\n--------\n");

        SysFreeString(name);
    }

    BSTR innerText = 0;
    COMPtr<IDOMElementPrivate> docPrivate;
    if (SUCCEEDED(documentElement->QueryInterface(&docPrivate)))
        docPrivate->innerText(&innerText);

    result.append(innerText ? innerText : L"");
    result.append(L"\n");

    SysFreeString(innerText);

    if (shouldDumpChildFramesAsText) {
        COMPtr<IEnumVARIANT> enumKids;
        if (FAILED(frame->childFrames(&enumKids)))
            return L"";
        VARIANT var;
        VariantInit(&var);
        while (enumKids->Next(1, &var, 0) == S_OK) {
            ASSERT(V_VT(&var) == VT_UNKNOWN);
            COMPtr<IWebFrame> framePtr;
            V_UNKNOWN(&var)->QueryInterface(IID_IWebFrame, (void**)&framePtr);
            result.append(dumpFramesAsText(framePtr.get()));
            VariantClear(&var);
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

    BSTR targetA;
    if (FAILED(itemA->target(&targetA)))
        return 0;

    BSTR targetB;
    if (FAILED(itemB->target(&targetB))) {
        SysFreeString(targetA);
        return 0;
    }

    int result = wcsicmp(wstring(targetA, SysStringLen(targetA)).c_str(), wstring(targetB, SysStringLen(targetB)).c_str());
    SysFreeString(targetA);
    SysFreeString(targetB);
    return result;
}

static void dumpHistoryItem(IWebHistoryItem* item, int indent, bool current)
{
    assert(item);

    int start = 0;
    if (current) {
        printf("curr->");
        start = 6;
    }
    for (int i = start; i < indent; i++)
        putchar(' ');

    BSTR url;
    if (FAILED(item->URLString(&url)))
        return;
    printf("%S", url ? url : L"");
    SysFreeString(url);

    COMPtr<IWebHistoryItemPrivate> itemPrivate;
    if (FAILED(item->QueryInterface(&itemPrivate)))
        return;

    BSTR target;
    if (FAILED(itemPrivate->target(&target)))
        return;
    if (SysStringLen(target))
        printf(" (in frame \"%S\")", target);
    SysFreeString(target);
    BOOL isTargetItem = FALSE;
    if (FAILED(itemPrivate->isTargetItem(&isTargetItem)))
        return;
    if (isTargetItem)
        printf("  **nav target**");
    putchar('\n');

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

static void dumpBackForwardList(IWebFrame* frame)
{
    assert(frame);

    printf("\n============== Back Forward List ==============\n");
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

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
        assert(item != prevTestBFItem);
        COMPtr<IUnknown> itemUnknown;
        item->QueryInterface(&itemUnknown);
        itemsToPrint.append(itemUnknown);
    }
    
    COMPtr<IWebHistoryItem> currentItem;
    if (FAILED(bfList->currentItem(&currentItem)))
        return;

    assert(currentItem != prevTestBFItem);
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

    printf("===============================================\n");
}

void dump()
{
    COMPtr<IWebDataSource> dataSource;
    if (SUCCEEDED(frame->dataSource(&dataSource))) {
        COMPtr<IWebURLResponse> response;
        if (SUCCEEDED(dataSource->response(&response))) {
            BSTR mimeType;
            if (SUCCEEDED(response->MIMEType(&mimeType)))
                dumpAsText |= !_tcscmp(mimeType, TEXT("text/plain"));
            SysFreeString(mimeType);
        }
    }

    BSTR resultString = 0;

    if (dumpTree) {
        if (dumpAsText) {
            ::InvalidateRect(webViewWindow, 0, TRUE);
            ::SendMessage(webViewWindow, WM_PAINT, 0, 0);
            wstring result = dumpFramesAsText(frame);
            resultString = SysAllocStringLen(result.data(), result.size());
        } else {
            bool isSVGW3CTest = strstr(currentTest, "svg\\W3C-SVG-1.1");
            unsigned width;
            unsigned height;
            if (isSVGW3CTest) {
                width = 480;
                height = 360;
            } else {
                width = maxViewWidth;
                height = maxViewHeight;
            }

            ::SetWindowPos(webViewWindow, 0, 0, 0, width, height, SWP_NOMOVE);
            ::InvalidateRect(webViewWindow, 0, TRUE);
            ::SendMessage(webViewWindow, WM_PAINT, 0, 0);

            COMPtr<IWebFramePrivate> framePrivate;
            if (FAILED(frame->QueryInterface(&framePrivate)))
                goto fail;
            framePrivate->renderTreeAsExternalRepresentation(&resultString);
        }
        
        if (!resultString)
            printf("ERROR: nil result from %s", dumpAsText ? "IDOMElement::innerText" : "IFrameViewPrivate::renderTreeAsExternalRepresentation");
        else {
            unsigned stringLength = SysStringLen(resultString);
            int bufferSize = ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, 0, 0, 0, 0);
            char* buffer = (char*)malloc(bufferSize + 1);
            int result = ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, buffer, bufferSize + 1, 0, 0);
            buffer[bufferSize] = '\0';
            printf("%s", buffer);
            free(buffer);
            if (!dumpAsText)
                dumpFrameScrollPosition(frame);
        }
        if (shouldDumpBackForwardList)
            dumpBackForwardList(frame);
    }

    if (printSeparators)
        puts("#EOF");
fail:
    SysFreeString(resultString);
    // This will exit from our message loop
    PostQuitMessage(0);
    done = true;
}

static void runTest(const char* pathOrURL)
{
    static BSTR methodBStr = SysAllocString(TEXT("GET"));

    BSTR urlBStr;
 
    CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, pathOrURL, kCFStringEncodingWindowsLatin1);
    CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, str, 0);

    if (!url)
        url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str, kCFURLWindowsPathStyle, false);

    CFRelease(str);

    str = CFURLGetString(url);

    CFIndex length = CFStringGetLength(str);
    UniChar* buffer = new UniChar[length];

    CFStringGetCharacters(str, CFRangeMake(0, length), buffer);
    urlBStr = SysAllocStringLen((OLECHAR*)buffer, length);
    delete[] buffer;

    CFRelease(url);

    currentTest = pathOrURL;

    done = false;
    topLoadingFrame = 0;
    waitToDump = false;
    dumpAsText = false;
    shouldDumpEditingCallbacks = false;
    shouldDumpTitleChanges = false;
    shouldDumpChildFrameScrollPositions = false;
    shouldDumpChildFramesAsText = false;
    shouldDumpBackForwardList = false;
    testRepaint = false;
    repaintSweepHorizontally = false;
    timedOut = false;

    prevTestBFItem = 0;
    COMPtr<IWebView> webView;
    if (SUCCEEDED(frame->webView(&webView))) {
        COMPtr<IWebBackForwardList> bfList;
        if (SUCCEEDED(webView->backForwardList(&bfList)))
            bfList->currentItem(&prevTestBFItem);
    }

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    // Set the test timeout timer
    SetTimer(hostWindow, timeoutId, timeoutValue, 0);

    COMPtr<IWebMutableURLRequest> request;
    HRESULT hr = CoCreateInstance(CLSID_WebMutableURLRequest, 0, CLSCTX_ALL, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        goto exit;

    request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);

    request->setHTTPMethod(methodBStr);
    frame->loadRequest(request.get());

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    KillTimer(hostWindow, timeoutId);

    if (timedOut) {
        fprintf(stderr, "ERROR: Timed out running %s\n", pathOrURL);
        printf("ERROR: Timed out loading page\n");

        if (printSeparators)
            puts("#EOF");
    }
exit:
    SysFreeString(urlBStr);
    return;
}

static void initializePreferences(IWebPreferences* preferences)
{
#ifdef USE_MAC_FONTS
    BSTR standardFamily = SysAllocString(TEXT("Times"));
    BSTR fixedFamily = SysAllocString(TEXT("Courier"));
    BSTR sansSerifFamily = SysAllocString(TEXT("Helvetica"));
    BSTR cursiveFamily = SysAllocString(TEXT("Apple Chancery"));
    BSTR fantasyFamily = SysAllocString(TEXT("Papyrus"));
#else
    BSTR standardFamily = SysAllocString(TEXT("Times New Roman"));
    BSTR fixedFamily = SysAllocString(TEXT("Courier New"));
    BSTR sansSerifFamily = SysAllocString(TEXT("Arial"));
    BSTR cursiveFamily = SysAllocString(TEXT("Comic Sans MS")); // Not actually cursive, but it's what IE and Firefox use.
    BSTR fantasyFamily = SysAllocString(TEXT("Times New Roman"));
#endif

    preferences->setStandardFontFamily(standardFamily);
    preferences->setFixedFontFamily(fixedFamily);
    preferences->setSerifFontFamily(standardFamily);
    preferences->setSansSerifFontFamily(sansSerifFamily);
    preferences->setCursiveFontFamily(cursiveFamily);
    preferences->setFantasyFontFamily(fantasyFamily);

    preferences->setAutosaves(FALSE);
    preferences->setJavaEnabled(FALSE);
    preferences->setPlugInsEnabled(TRUE);
    preferences->setDOMPasteAllowed(TRUE);
    preferences->setEditableLinkBehavior(WebKitEditableLinkOnlyLiveWithShiftKey);

    SysFreeString(standardFamily);
    SysFreeString(fixedFamily);
    SysFreeString(sansSerifFamily);
    SysFreeString(cursiveFamily);
    SysFreeString(fantasyFamily);
}

static Boolean pthreadEqualCallback(const void* value1, const void* value2)
{
    return (Boolean)pthread_equal(*(pthread_t*)value1, *(pthread_t*)value2);
}

static CFDictionaryKeyCallBacks pthreadKeyCallbacks = { 0, 0, 0, 0, pthreadEqualCallback, 0 };

static pthread_mutex_t javaScriptThreadsMutex = PTHREAD_MUTEX_INITIALIZER;
static bool javaScriptThreadsShouldTerminate;

static const int javaScriptThreadsCount = 4;
static CFMutableDictionaryRef javaScriptThreads()
{
    assert(pthread_mutex_trylock(&javaScriptThreadsMutex) == EBUSY);
    static CFMutableDictionaryRef staticJavaScriptThreads;
    if (!staticJavaScriptThreads)
        staticJavaScriptThreads = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &pthreadKeyCallbacks, 0);
    return staticJavaScriptThreads;
}

// Loops forever, running a script and randomly respawning, until 
// javaScriptThreadsShouldTerminate becomes true.
void* runJavaScriptThread(void* arg)
{
    const char* const script =
    " \
    var array = []; \
    for (var i = 0; i < 10; i++) { \
        array.push(String(i)); \
    } \
    ";

    while (true) {
        JSGlobalContextRef ctx = JSGlobalContextCreate(0);
        JSStringRef scriptRef = JSStringCreateWithUTF8CString(script);

        JSValueRef exception = 0;
        JSEvaluateScript(ctx, scriptRef, 0, 0, 0, &exception);
        assert(!exception);
        
        JSGlobalContextRelease(ctx);
        JSStringRelease(scriptRef);
        
        JSGarbageCollect(ctx);

        pthread_mutex_lock(&javaScriptThreadsMutex);

        // Check for cancellation.
        if (javaScriptThreadsShouldTerminate) {
            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        // Respawn probabilistically.
        if (rand() % 5 == 0) {
            pthread_t pthread;
            pthread_create(&pthread, 0, &runJavaScriptThread, 0);
            pthread_detach(pthread);

            pthread_t self = pthread_self();
            CFDictionaryRemoveValue(javaScriptThreads(), self.p);
            CFDictionaryAddValue(javaScriptThreads(), pthread.p, 0);

            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        pthread_mutex_unlock(&javaScriptThreadsMutex);
    }
}

static void startJavaScriptThreads(void)
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    for (int i = 0; i < javaScriptThreadsCount; i++) {
        pthread_t pthread;
        pthread_create(&pthread, 0, &runJavaScriptThread, 0);
        pthread_detach(pthread);
        CFDictionaryAddValue(javaScriptThreads(), pthread.p, 0);
    }

    pthread_mutex_unlock(&javaScriptThreadsMutex);
}

static void stopJavaScriptThreads(void)
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    javaScriptThreadsShouldTerminate = true;

    pthread_t* pthreads[javaScriptThreadsCount] = {0};
    int threadDictCount = CFDictionaryGetCount(javaScriptThreads());
    assert(threadDictCount == javaScriptThreadsCount);
    CFDictionaryGetKeysAndValues(javaScriptThreads(), (const void**)pthreads, 0);

    pthread_mutex_unlock(&javaScriptThreadsMutex);

    for (int i = 0; i < javaScriptThreadsCount; i++) {
        pthread_t* pthread = pthreads[i];
        pthread_join(*pthread, 0);
        free(pthread);
    }
}

int main(int argc, char* argv[])
{
    leakChecking = false;

    wstring exePath = initialize(GetModuleHandle(0));

    // FIXME: options

    COMPtr<IWebView> webView;
    HRESULT hr = CoCreateInstance(CLSID_WebView, 0, CLSCTX_ALL, IID_IWebView, (void**)&webView);
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to create CLSID_WebView instance, error 0x%x\n", hr);
        return -1;
    }

    if (FAILED(webView->setHostWindow((OLE_HANDLE)(ULONG64)hostWindow)))
        return -1;

    RECT clientRect;
    clientRect.bottom = clientRect.left = clientRect.top = clientRect.right = 0;
    if (FAILED(webView->initWithFrame(clientRect, 0, 0)))
        return -1;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return -1;
    webView->Release();


    BSTR pluginPath = SysAllocStringLen(0, exePath.length() + _tcslen(TestPluginDir));
    _tcscpy(pluginPath, exePath.c_str());
    _tcscat(pluginPath, TestPluginDir);
    bool failed = FAILED(viewPrivate->addAdditionalPluginPath(pluginPath));
    SysFreeString(pluginPath);
    if (failed)
        return -1;

    if (FAILED(viewPrivate->viewWindow((OLE_HANDLE*)&webViewWindow)))
        return -1;

    SetWindowPos(webViewWindow, 0, 0, 0, maxViewWidth, maxViewHeight, 0);
    ShowWindow(hostWindow, SW_SHOW);

    COMPtr<WaitUntilDoneDelegate> waitDelegate;
    waitDelegate.adoptRef(new WaitUntilDoneDelegate);
    if (FAILED(webView->setFrameLoadDelegate(waitDelegate.get())))
        return -1;

    if (FAILED(webView->setUIDelegate(waitDelegate.get())))
        return -1;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return -1;
    webView->Release();

    COMPtr<EditingDelegate> editingDelegate;
    editingDelegate.adoptRef(new EditingDelegate);

    if (FAILED(viewEditing->setEditingDelegate(editingDelegate.get())))
        return -1;

    COMPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return -1;

    initializePreferences(preferences.get());

    COMPtr<IWebIconDatabase> iconDatabase;
    COMPtr<IWebIconDatabase> tmpIconDatabase;
    if (FAILED(CoCreateInstance(CLSID_WebIconDatabase, 0, CLSCTX_ALL, IID_IWebIconDatabase, (void**)&tmpIconDatabase)))
        return -1;
    if (FAILED(tmpIconDatabase->sharedIconDatabase(&iconDatabase)))
        return -1;
        
    if (FAILED(webView->mainFrame(&frame)))
        return -1;

    _CrtMemState entryToMainMemCheckpoint;
    if (leakChecking)
        _CrtMemCheckpoint(&entryToMainMemCheckpoint);

    for (int i = 0; i < argc; ++i)
        if (!stricmp(argv[i], "--threaded")) {
            argv[i] = argv[argc - 1];
            argc--;
            threaded = true;
            break;
        }

    if (threaded)
        startJavaScriptThreads();

    if (argc == 2 && strcmp(argv[1], "-") == 0) {
        char filenameBuffer[2048];
        printSeparators = true;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char* newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';
            
            if (strlen(filenameBuffer) == 0)
                continue;

            runTest(filenameBuffer);
            fflush(stdout);
        }
    } else {
        printSeparators = argc > 2;
        for (int i = 1; i != argc; i++)
            runTest(argv[i]);
    }

    if (threaded)
        stopJavaScriptThreads();
    
    frame->Release();

    if (leakChecking) {
        // dump leaks to stderr
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtMemDumpAllObjectsSince(&entryToMainMemCheckpoint);
    }

    return 0;
}
