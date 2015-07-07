/*
 * Copyright (C) 2006, 2008, 2013-2015 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2011 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009, 2010, 2011 Appcelerator, Inc. All rights reserved.
 * Copyright (C) 2013 Alex Christensen. All rights reserved.
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

#include "AccessibilityDelegate.h"
#include "DOMDefaultImpl.h"
#include "PrintWebUIDelegate.h"
#include "WinLauncher.h"
#include "WinLauncherReplace.h"
#include <WebKit/WebKitCOMAPI.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#include <WebKit/CFDictionaryPropertyBag.h>
#endif

#include <cassert>
#include <comip.h>
#include <commctrl.h>
#include <commdlg.h>
#include <comutil.h>
#include <dbghelp.h>
#include <memory>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <wininet.h>

#define MAX_LOADSTRING 100
#define URLBAR_HEIGHT  24
#define CONTROLBUTTON_WIDTH 24

static const int maxHistorySize = 10;

typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

// Global Variables:
HINSTANCE hInst;
HWND hMainWnd;
HWND hURLBarWnd;
HWND hBackButtonWnd;
HWND hForwardButtonWnd;
HWND hCacheWnd;
WNDPROC DefEditProc = nullptr;
WNDPROC DefButtonProc = nullptr;
WNDPROC DefWebKitProc = nullptr;
HWND gViewWindow = 0;
WinLauncher* gWinLauncher = nullptr;
TCHAR szTitle[MAX_LOADSTRING]; // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

// Support moving the transparent window
POINT s_windowPosition = { 100, 100 };
SIZE s_windowSize = { 800, 400 };

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK BackButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ForwardButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ReloadButtonProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Caches(HWND, UINT, WPARAM, LPARAM);

static void loadURL(BSTR urlBStr);
static void updateStatistics(HWND hDlg);

static void resizeSubViews()
{
    if (gWinLauncher->usesLayeredWebView() || !gViewWindow)
        return;

    RECT rcClient;
    GetClientRect(hMainWnd, &rcClient);
    MoveWindow(hBackButtonWnd, 0, 0, CONTROLBUTTON_WIDTH, URLBAR_HEIGHT, TRUE);
    MoveWindow(hForwardButtonWnd, CONTROLBUTTON_WIDTH, 0, CONTROLBUTTON_WIDTH, URLBAR_HEIGHT, TRUE);
    MoveWindow(hURLBarWnd, CONTROLBUTTON_WIDTH * 2, 0, rcClient.right, URLBAR_HEIGHT, TRUE);
    MoveWindow(gViewWindow, 0, URLBAR_HEIGHT, rcClient.right, rcClient.bottom - URLBAR_HEIGHT, TRUE);
}

static void subclassForLayeredWindow()
{
    hMainWnd = gViewWindow;
#if defined _M_AMD64 || defined _WIN64
    DefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hMainWnd, GWLP_WNDPROC));
    ::SetWindowLongPtr(hMainWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
#else
    DefWebKitProc = reinterpret_cast<WNDPROC>(::GetWindowLong(hMainWnd, GWL_WNDPROC));
    ::SetWindowLong(hMainWnd, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
#endif
}

static void computeFullDesktopFrame()
{
    RECT desktop;
    if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, static_cast<void*>(&desktop), 0))
        return;

    s_windowPosition.x = 0;
    s_windowPosition.y = 0;
    s_windowSize.cx = desktop.right - desktop.left;
    s_windowSize.cy = desktop.bottom - desktop.top;
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
#if defined(_M_X64) || defined(__x86_64__)
        // The VS2013 runtime has a bug where it mis-detects AVX-capable processors
        // if the feature has been disabled in firmware. This causes us to crash
        // in some of the math functions. For now, we disable those optimizations
        // because Microsoft is not going to fix the problem in VS2013.
        // FIXME: http://webkit.org/b/141449: Remove this workaround when we switch to VS2015+.
        _set_FMA3_enable(0);
#endif
        hInst = dllInstance;
    }

    return TRUE;
}

static bool getAppDataFolder(_bstr_t& directory)
{
    wchar_t appDataDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory)))
        return false;

    wchar_t executablePath[MAX_PATH];
    ::GetModuleFileNameW(0, executablePath, MAX_PATH);
    ::PathRemoveExtensionW(executablePath);

    directory = _bstr_t(appDataDirectory) + L"\\" + ::PathFindFileNameW(executablePath);

    return true;
}

static bool setCacheFolder()
{
    IWebCachePtr webCache = gWinLauncher->webCache();
    if (!webCache)
        return false;

    _bstr_t appDataFolder;
    if (!getAppDataFolder(appDataFolder))
        return false;

    appDataFolder += L"\\cache";
    webCache->setCacheFolder(appDataFolder);

    return true;
}

void createCrashReport(EXCEPTION_POINTERS* exceptionPointers)
{
    _bstr_t directory;

    if (!getAppDataFolder(directory))
        return;

    if (::SHCreateDirectoryEx(0, directory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return;

    std::wstring fileName = directory + L"\\CrashReport.dmp";
    HANDLE miniDumpFile = ::CreateFile(fileName.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (miniDumpFile && miniDumpFile != INVALID_HANDLE_VALUE) {

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = ::GetCurrentThreadId();
        mdei.ExceptionPointers  = exceptionPointers;
        mdei.ClientPointers = 0;

#ifdef _DEBUG
        MINIDUMP_TYPE dumpType = MiniDumpWithFullMemory;
#else
        MINIDUMP_TYPE dumpType = MiniDumpNormal;
#endif

        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), miniDumpFile, dumpType, &mdei, 0, 0);
        ::CloseHandle(miniDumpFile);
        processCrashReport(fileName.c_str());
    }
}

static BOOL CALLBACK AbortProc(HDC hDC, int Error)
{
    MSG msg;
    while (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return TRUE;
}

static HDC getPrinterDC()
{
    PRINTDLG pdlg;
    memset(&pdlg, 0, sizeof(PRINTDLG));
    pdlg.lStructSize = sizeof(PRINTDLG);
    pdlg.Flags = PD_PRINTSETUP | PD_RETURNDC;

    ::PrintDlg(&pdlg);

    return pdlg.hDC;
}

static void initDocStruct(DOCINFO* di, TCHAR* docname)
{
    memset(di, 0, sizeof(DOCINFO));
    di->cbSize = sizeof(DOCINFO);
    di->lpszDocName = docname;
}

typedef _com_ptr_t<_com_IIID<IWebFramePrivate, &__uuidof(IWebFramePrivate)>> IWebFramePrivatePtr;

void PrintView(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC printDC = getPrinterDC();
    if (!printDC) {
        ::MessageBoxW(0, L"Error creating printing DC", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    if (::SetAbortProc(printDC, AbortProc) == SP_ERROR) {
        ::MessageBoxW(0, L"Error setting up AbortProc", L"Error", MB_APPLMODAL | MB_OK);
        return;
    }

    IWebFramePtr frame = gWinLauncher->mainFrame();
    if (!frame)
        return;

    IWebFramePrivatePtr framePrivate;
    if (FAILED(frame->QueryInterface(&framePrivate.GetInterfacePtr())))
        return;

    framePrivate->setInPrintingMode(TRUE, printDC);

    UINT pageCount = 0;
    framePrivate->getPrintedPageCount(printDC, &pageCount);

    DOCINFO di;
    initDocStruct(&di, L"WebKit Doc");
    ::StartDoc(printDC, &di);

    // FIXME: Need CoreGraphics implementation
    void* graphicsContext = 0;
    for (size_t page = 1; page <= pageCount; ++page) {
        ::StartPage(printDC);
        framePrivate->spoolPages(printDC, page, page, graphicsContext);
        ::EndPage(printDC);
    }

    framePrivate->setInPrintingMode(FALSE, printDC);

    ::EndDoc(printDC);
    ::DeleteDC(printDC);
}

static void ToggleMenuItem(HWND hWnd, UINT menuID)
{
    HMENU menu = ::GetMenu(hWnd);

    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    if (!::GetMenuItemInfo(menu, menuID, FALSE, &info))
        return;

    BOOL newState = !(info.fState & MFS_CHECKED);

    if (!gWinLauncher->standardPreferences() || !gWinLauncher->privatePreferences())
        return;

    switch (menuID) {
    case IDM_AVFOUNDATION:
        gWinLauncher->standardPreferences()->setAVFoundationEnabled(newState);
        break;
    case IDM_ACC_COMPOSITING:
        gWinLauncher->privatePreferences()->setAcceleratedCompositingEnabled(newState);
        break;
    case IDM_WK_FULLSCREEN:
        gWinLauncher->privatePreferences()->setFullScreenEnabled(newState);
        break;
    case IDM_COMPOSITING_BORDERS:
        gWinLauncher->privatePreferences()->setShowDebugBorders(newState);
        gWinLauncher->privatePreferences()->setShowRepaintCounter(newState);
        break;
    case IDM_DISABLE_IMAGES:
        gWinLauncher->standardPreferences()->setLoadsImagesAutomatically(!newState);
        break;
    case IDM_DISABLE_STYLES:
        gWinLauncher->privatePreferences()->setAuthorAndUserStylesEnabled(!newState);
        break;
    case IDM_DISABLE_JAVASCRIPT:
        gWinLauncher->standardPreferences()->setJavaScriptEnabled(!newState);
        break;
    case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
        gWinLauncher->privatePreferences()->setAllowUniversalAccessFromFileURLs(newState);
        gWinLauncher->privatePreferences()->setAllowFileAccessFromFileURLs(newState);
        break;
    }

    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;

    ::SetMenuItemInfo(menu, menuID, FALSE, &info);
}

static const int dragBarHeight = 30;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC parentProc = (gWinLauncher) ? (gWinLauncher->usesLayeredWebView() ? DefWebKitProc : DefWindowProc) : DefWindowProc;

    switch (message) {
    case WM_NCHITTEST:
        if (gWinLauncher && gWinLauncher->usesLayeredWebView()) {
            RECT window;
            ::GetWindowRect(hWnd, &window);
            // For testing our transparent window, we need a region to use as a handle for
            // dragging. The right way to do this would be to query the web view to see what's
            // under the mouse. However, for testing purposes we just use an arbitrary
            // 30 pixel band at the top of the view as an arbitrary gripping location.
            //
            // When we are within this bad, return HT_CAPTION to tell Windows we want to
            // treat this region as if it were the title bar on a normal window.
            int y = HIWORD(lParam);

            if ((y > window.top) && (y < window.top + dragBarHeight))
                return HTCAPTION;
        }
        return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        if (wmId >= IDM_HISTORY_LINK0 && wmId <= IDM_HISTORY_LINK9) {
            if (gWinLauncher)
                gWinLauncher->navigateToHistory(hWnd, wmId);
            break;
        }
        // Parse the menu selections:
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDM_PRINT:
            PrintView(hWnd, message, wParam, lParam);
            break;
        case IDM_WEB_INSPECTOR:
            if (gWinLauncher)
                gWinLauncher->launchInspector();
            break;
        case IDM_CACHES:
            if (!::IsWindow(hCacheWnd)) {
                hCacheWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_CACHES), hWnd, Caches);
                ::ShowWindow(hCacheWnd, SW_SHOW);
            }
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            if (gWinLauncher)
                gWinLauncher->navigateForwardOrBackward(hWnd, wmId);
            break;
        case IDM_AVFOUNDATION:
        case IDM_ACC_COMPOSITING:
        case IDM_WK_FULLSCREEN:
        case IDM_COMPOSITING_BORDERS:
        case IDM_DISABLE_IMAGES:
        case IDM_DISABLE_STYLES:
        case IDM_DISABLE_JAVASCRIPT:
        case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
            ToggleMenuItem(hWnd, wmId);
            break;
        default:
            return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
        }
        }
        break;
    case WM_DESTROY:
#if USE(CF)
        CFRunLoopStop(CFRunLoopGetMain());
#endif
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (!gWinLauncher || !gWinLauncher->hasWebView() || gWinLauncher->usesLayeredWebView())
            return CallWindowProc(parentProc, hWnd, message, wParam, lParam);

        resizeSubViews();
        break;
    default:
        return CallWindowProc(parentProc, hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK EditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CHAR:
        if (wParam == 13) { // Enter Key
            wchar_t strPtr[INTERNET_MAX_URL_LENGTH];
            *((LPWORD)strPtr) = INTERNET_MAX_URL_LENGTH; 
            int strLen = SendMessage(hDlg, EM_GETLINE, 0, (LPARAM)strPtr);

            strPtr[strLen] = 0;
            _bstr_t bstr(strPtr);
            loadURL(bstr.GetBSTR());

            return 0;
        } 
    default:
        return CallWindowProc(DefEditProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK BackButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONUP:
        gWinLauncher->goBack();
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK ForwardButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONUP:
        gWinLauncher->goForward();
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK Caches(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        ::SetTimer(hDlg, IDT_UPDATE_STATS, 1000, nullptr);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            ::KillTimer(hDlg, IDT_UPDATE_STATS);
            ::DestroyWindow(hDlg);
            hCacheWnd = 0;
            return (INT_PTR)TRUE;
        }
        break;

    case IDT_UPDATE_STATS:
        ::InvalidateRect(hDlg, nullptr, FALSE);
        return (INT_PTR)TRUE;

    case WM_PAINT:
        updateStatistics(hDlg);
        break;
    }

    return (INT_PTR)FALSE;
}

static void loadURL(BSTR passedURL)
{
    if (FAILED(gWinLauncher->loadURL(passedURL)))
        return;

    SetFocus(gViewWindow);
}

static void setWindowText(HWND dialog, UINT field, _bstr_t value)
{
    ::SetDlgItemText(dialog, field, value);
}

static void setWindowText(HWND dialog, UINT field, UINT value)
{
    String valueStr = WTF::String::number(value);

    setWindowText(dialog, field, _bstr_t(valueStr.utf8().data()));
}

typedef _com_ptr_t<_com_IIID<IPropertyBag, &__uuidof(IPropertyBag)>> IPropertyBagPtr;

static void setWindowText(HWND dialog, UINT field, IPropertyBagPtr statistics, const _bstr_t& key)
{
    VARIANT var;
    ::VariantInit(&var);
    V_VT(&var) = VT_UI8;
    if (FAILED(statistics->Read(key, &var, 0))) {
        ::VariantClear(&var);
        return;
    }

    unsigned long long value = V_UI8(&var);
    String valueStr = WTF::String::number(value);

    setWindowText(dialog, field, _bstr_t(valueStr.utf8().data()));
    ::VariantClear(&var);
}

static void setWindowText(HWND dialog, UINT field, CFDictionaryRef dictionary, CFStringRef key, UINT& total)
{
    CFNumberRef countNum = static_cast<CFNumberRef>(CFDictionaryGetValue(dictionary, key));
    if (!countNum)
        return;

    int count = 0;
    CFNumberGetValue(countNum, kCFNumberIntType, &count);

    setWindowText(dialog, field, static_cast<UINT>(count));
    total += count;
}

static void updateStatistics(HWND dialog)
{
    if (!gWinLauncher)
        return;

    IWebCoreStatisticsPtr webCoreStatistics = gWinLauncher->statistics();
    if (!webCoreStatistics)
        return;

    IPropertyBagPtr statistics;
    HRESULT hr = webCoreStatistics->memoryStatistics(&statistics.GetInterfacePtr());
    if (FAILED(hr))
        return;

    // FastMalloc.
    setWindowText(dialog, IDC_RESERVED_VM, statistics, "FastMallocReservedVMBytes");
    setWindowText(dialog, IDC_COMMITTED_VM, statistics, "FastMallocCommittedVMBytes");
    setWindowText(dialog, IDC_FREE_LIST_BYTES, statistics, "FastMallocFreeListBytes");

    // WebCore Cache.
#if USE(CF)
    IWebCachePtr webCache = gWinLauncher->webCache();

    int dictCount = 6;
    IPropertyBag* cacheDict[6] = { 0 };
    if (FAILED(webCache->statistics(&dictCount, cacheDict)))
        return;

    COMPtr<CFDictionaryPropertyBag> counts, sizes, liveSizes, decodedSizes, purgableSizes;
    counts.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[0]));
    sizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[1]));
    liveSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[2]));
    decodedSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[3]));
    purgableSizes.adoptRef(reinterpret_cast<CFDictionaryPropertyBag*>(cacheDict[4]));

    static CFStringRef imagesKey = CFSTR("images");
    static CFStringRef stylesheetsKey = CFSTR("style sheets");
    static CFStringRef xslKey = CFSTR("xsl");
    static CFStringRef scriptsKey = CFSTR("scripts");

    if (counts) {
        UINT totalObjects = 0;
        setWindowText(dialog, IDC_IMAGES_OBJECT_COUNT, counts->dictionary(), imagesKey, totalObjects);
        setWindowText(dialog, IDC_CSS_OBJECT_COUNT, counts->dictionary(), stylesheetsKey, totalObjects);
        setWindowText(dialog, IDC_XSL_OBJECT_COUNT, counts->dictionary(), xslKey, totalObjects);
        setWindowText(dialog, IDC_JSC_OBJECT_COUNT, counts->dictionary(), scriptsKey, totalObjects);
        setWindowText(dialog, IDC_TOTAL_OBJECT_COUNT, totalObjects);
    }

    if (sizes) {
        UINT totalBytes = 0;
        setWindowText(dialog, IDC_IMAGES_BYTES, sizes->dictionary(), imagesKey, totalBytes);
        setWindowText(dialog, IDC_CSS_BYTES, sizes->dictionary(), stylesheetsKey, totalBytes);
        setWindowText(dialog, IDC_XSL_BYTES, sizes->dictionary(), xslKey, totalBytes);
        setWindowText(dialog, IDC_JSC_BYTES, sizes->dictionary(), scriptsKey, totalBytes);
        setWindowText(dialog, IDC_TOTAL_BYTES, totalBytes);
    }

    if (liveSizes) {
        UINT totalLiveBytes = 0;
        setWindowText(dialog, IDC_IMAGES_LIVE_COUNT, liveSizes->dictionary(), imagesKey, totalLiveBytes);
        setWindowText(dialog, IDC_CSS_LIVE_COUNT, liveSizes->dictionary(), stylesheetsKey, totalLiveBytes);
        setWindowText(dialog, IDC_XSL_LIVE_COUNT, liveSizes->dictionary(), xslKey, totalLiveBytes);
        setWindowText(dialog, IDC_JSC_LIVE_COUNT, liveSizes->dictionary(), scriptsKey, totalLiveBytes);
        setWindowText(dialog, IDC_TOTAL_LIVE_COUNT, totalLiveBytes);
    }

    if (decodedSizes) {
        UINT totalDecoded = 0;
        setWindowText(dialog, IDC_IMAGES_DECODED_COUNT, decodedSizes->dictionary(), imagesKey, totalDecoded);
        setWindowText(dialog, IDC_CSS_DECODED_COUNT, decodedSizes->dictionary(), stylesheetsKey, totalDecoded);
        setWindowText(dialog, IDC_XSL_DECODED_COUNT, decodedSizes->dictionary(), xslKey, totalDecoded);
        setWindowText(dialog, IDC_JSC_DECODED_COUNT, decodedSizes->dictionary(), scriptsKey, totalDecoded);
        setWindowText(dialog, IDC_TOTAL_DECODED, totalDecoded);
    }

    if (purgableSizes) {
        UINT totalPurgable = 0;
        setWindowText(dialog, IDC_IMAGES_PURGEABLE_COUNT, purgableSizes->dictionary(), imagesKey, totalPurgable);
        setWindowText(dialog, IDC_CSS_PURGEABLE_COUNT, purgableSizes->dictionary(), stylesheetsKey, totalPurgable);
        setWindowText(dialog, IDC_XSL_PURGEABLE_COUNT, purgableSizes->dictionary(), xslKey, totalPurgable);
        setWindowText(dialog, IDC_JSC_PURGEABLE_COUNT, purgableSizes->dictionary(), scriptsKey, totalPurgable);
        setWindowText(dialog, IDC_TOTAL_PURGEABLE, totalPurgable);
    }
#endif

    // JavaScript Heap.
    setWindowText(dialog, IDC_JSC_HEAP_SIZE, statistics, "JavaScriptHeapSize");
    setWindowText(dialog, IDC_JSC_HEAP_FREE, statistics, "JavaScriptFreeSize");

    UINT count;
    if (SUCCEEDED(webCoreStatistics->javaScriptObjectsCount(&count)))
        setWindowText(dialog, IDC_TOTAL_JSC_HEAP_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->javaScriptGlobalObjectsCount(&count)))
        setWindowText(dialog, IDC_GLOBAL_JSC_HEAP_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->javaScriptProtectedObjectsCount(&count)))
        setWindowText(dialog, IDC_PROTECTED_JSC_HEAP_OBJECTS, count);

    // Font and Glyph Caches.
    if (SUCCEEDED(webCoreStatistics->cachedFontDataCount(&count)))
        setWindowText(dialog, IDC_TOTAL_FONT_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->cachedFontDataInactiveCount(&count)))
        setWindowText(dialog, IDC_INACTIVE_FONT_OBJECTS, count);
    if (SUCCEEDED(webCoreStatistics->glyphPageCount(&count)))
        setWindowText(dialog, IDC_GLYPH_PAGES, count);

    // Site Icon Database.
    if (SUCCEEDED(webCoreStatistics->iconPageURLMappingCount(&count)))
        setWindowText(dialog, IDC_PAGE_URL_MAPPINGS, count);
    if (SUCCEEDED(webCoreStatistics->iconRetainedPageURLCount(&count)))
        setWindowText(dialog, IDC_RETAINED_PAGE_URLS, count);
    if (SUCCEEDED(webCoreStatistics->iconRecordCount(&count)))
        setWindowText(dialog, IDC_SITE_ICON_RECORDS, count);
    if (SUCCEEDED(webCoreStatistics->iconsWithDataCount(&count)))
        setWindowText(dialog, IDC_SITE_ICONS_WITH_DATA, count);
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    return wWinMain(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
