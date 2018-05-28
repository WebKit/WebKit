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

#include "DOMDefaultImpl.h"
#include "MainWindow.h"
#include "MiniBrowser.h"
#include "MiniBrowserReplace.h"
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#include <WebKitLegacy/CFDictionaryPropertyBag.h>
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

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

typedef _com_ptr_t<_com_IIID<IWebFrame, &__uuidof(IWebFrame)>> IWebFramePtr;
typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;

// Global Variables:
HINSTANCE hInst;
HWND hCacheWnd;
WNDPROC DefEditProc = nullptr;
WNDPROC DefButtonProc = nullptr;

MainWindow* gMainWindow = nullptr;
MiniBrowser* gMiniBrowser = nullptr;

// Support moving the transparent window
POINT s_windowPosition = { 100, 100 };
SIZE s_windowSize = { 500, 200 };

// Forward declarations of functions included in this code module:
INT_PTR CALLBACK AuthDialogProc(HWND, UINT, WPARAM, LPARAM);

static void loadURL(BSTR urlBStr);
static void updateStatistics(HWND hDlg);

namespace WebCore {
float deviceScaleFactorForWindow(HWND);
}

static void computeFullDesktopFrame()
{
    RECT desktop;
    if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, static_cast<void*>(&desktop), 0))
        return;

    float scaleFactor = WebCore::deviceScaleFactorForWindow(nullptr);

    s_windowPosition.x = 0;
    s_windowPosition.y = 0;
    s_windowSize.cx = scaleFactor * (desktop.right - desktop.left);
    s_windowSize.cy = scaleFactor * (desktop.bottom - desktop.top);
}

BOOL WINAPI DllMain(HINSTANCE dllInstance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        hInst = dllInstance;

    return TRUE;
}

bool getAppDataFolder(_bstr_t& directory)
{
    wchar_t appDataDirectory[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory)))
        return false;

    wchar_t executablePath[MAX_PATH];
    if (!::GetModuleFileNameW(0, executablePath, MAX_PATH))
        return false;

    ::PathRemoveExtensionW(executablePath);

    directory = _bstr_t(appDataDirectory) + L"\\" + ::PathFindFileNameW(executablePath);

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

    std::wstring fileName = std::wstring(static_cast<const wchar_t*>(directory)) + L"\\CrashReport.dmp";
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

    IWebFramePtr frame = gMiniBrowser->mainFrame();
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

void ToggleMenuFlag(HWND hWnd, UINT menuID)
{
    HMENU menu = ::GetMenu(hWnd);

    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    if (!::GetMenuItemInfo(menu, menuID, FALSE, &info))
        return;

    BOOL newState = !(info.fState & MFS_CHECKED);
    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;

    ::SetMenuItemInfo(menu, menuID, FALSE, &info);
}

static bool menuItemIsChecked(const MENUITEMINFO& info)
{
    return info.fState & MFS_CHECKED;
}

static void turnOffOtherUserAgents(HMENU menu)
{
    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    // Must unset the other menu items:
    for (UINT menuToClear = IDM_UA_DEFAULT; menuToClear <= IDM_UA_OTHER; ++menuToClear) {
        if (!::GetMenuItemInfo(menu, menuToClear, FALSE, &info))
            continue;
        if (!menuItemIsChecked(info))
            continue;

        info.fState = MFS_UNCHECKED;
        ::SetMenuItemInfo(menu, menuToClear, FALSE, &info);
    }
}

bool ToggleMenuItem(HWND hWnd, UINT menuID)
{
    if (!gMiniBrowser)
        return false;

    HMENU menu = ::GetMenu(hWnd);

    MENUITEMINFO info;
    ::memset(&info, 0x00, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;

    if (!::GetMenuItemInfo(menu, menuID, FALSE, &info))
        return false;

    BOOL newState = !menuItemIsChecked(info);

    if (!gMiniBrowser->standardPreferences() || !gMiniBrowser->privatePreferences())
        return false;

    switch (menuID) {
    case IDM_AVFOUNDATION:
        gMiniBrowser->standardPreferences()->setAVFoundationEnabled(newState);
        break;
    case IDM_ACC_COMPOSITING:
        gMiniBrowser->privatePreferences()->setAcceleratedCompositingEnabled(newState);
        break;
    case IDM_WK_FULLSCREEN:
        gMiniBrowser->privatePreferences()->setFullScreenEnabled(newState);
        break;
    case IDM_COMPOSITING_BORDERS:
        gMiniBrowser->privatePreferences()->setShowDebugBorders(newState);
        gMiniBrowser->privatePreferences()->setShowRepaintCounter(newState);
        break;
    case IDM_DEBUG_INFO_LAYER:
        gMiniBrowser->privatePreferences()->setShowTiledScrollingIndicator(newState);
        break;
    case IDM_INVERT_COLORS:
        gMiniBrowser->privatePreferences()->setShouldInvertColors(newState);
        break;
    case IDM_DISABLE_IMAGES:
        gMiniBrowser->standardPreferences()->setLoadsImagesAutomatically(!newState);
        break;
    case IDM_DISABLE_STYLES:
        gMiniBrowser->privatePreferences()->setAuthorAndUserStylesEnabled(!newState);
        break;
    case IDM_DISABLE_JAVASCRIPT:
        gMiniBrowser->standardPreferences()->setJavaScriptEnabled(!newState);
        break;
    case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
        gMiniBrowser->privatePreferences()->setAllowUniversalAccessFromFileURLs(newState);
        gMiniBrowser->privatePreferences()->setAllowFileAccessFromFileURLs(newState);
        break;
    case IDM_UA_DEFAULT:
    case IDM_UA_SAFARI_8_0:
    case IDM_UA_SAFARI_IOS_8_IPHONE:
    case IDM_UA_SAFARI_IOS_8_IPAD:
    case IDM_UA_IE_11:
    case IDM_UA_CHROME_MAC:
    case IDM_UA_CHROME_WIN:
    case IDM_UA_FIREFOX_MAC:
    case IDM_UA_FIREFOX_WIN:
        gMiniBrowser->setUserAgent(menuID);
        turnOffOtherUserAgents(menu);
        break;
    case IDM_UA_OTHER:
        // The actual user agent string will be set by the custom user agent dialog
        turnOffOtherUserAgents(menu);
        break;
    default:
        return false;
    }

    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;

    ::SetMenuItemInfo(menu, menuID, FALSE, &info);

    return true;
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
        gMiniBrowser->goBack();
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK ForwardButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_LBUTTONUP:
        gMiniBrowser->goForward();
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

INT_PTR CALLBACK CustomUserAgent(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG: {
        HWND edit = ::GetDlgItem(hDlg, IDC_USER_AGENT_INPUT);
        _bstr_t userAgent;
        if (gMiniBrowser)
            userAgent = gMiniBrowser->userAgent();

        ::SetWindowText(edit, static_cast<LPCTSTR>(userAgent));
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            HWND edit = ::GetDlgItem(hDlg, IDC_USER_AGENT_INPUT);

            TCHAR buffer[1024];
            int strLen = ::GetWindowText(edit, buffer, 1024);
            buffer[strLen] = 0;

            _bstr_t bstr(buffer);
            if (bstr.length()) {
                gMiniBrowser->setUserAgent(bstr);
                ::PostMessage(gMainWindow->hwnd(), static_cast<UINT>(WM_COMMAND), MAKELPARAM(IDM_UA_OTHER, 1), 0);
            }
        }

        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            ::EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

HRESULT DisplayAuthDialog(std::wstring& username, std::wstring& password)
{
    auto result = DialogBox(hInst, MAKEINTRESOURCE(IDD_AUTH), gMainWindow->hwnd(), AuthDialogProc);
    if (!result)
        return E_FAIL;

    auto pair = reinterpret_cast<std::pair<std::wstring, std::wstring>*>(result);
    username = pair->first;
    password = pair->second;
    delete pair;

    return S_OK;
}

INT_PTR CALLBACK AuthDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG: {
        HWND edit = ::GetDlgItem(hDlg, IDC_AUTH_USER);
        ::SetWindowText(edit, static_cast<LPCTSTR>(L""));
        
        edit = ::GetDlgItem(hDlg, IDC_AUTH_PASSWORD);
        ::SetWindowText(edit, static_cast<LPCTSTR>(L""));
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            INT_PTR result { };

            if (LOWORD(wParam) == IDOK) {
                TCHAR user[256];
                int strLen = ::GetWindowText(::GetDlgItem(hDlg, IDC_AUTH_USER), user, 256);
                user[strLen] = 0;

                TCHAR pass[256];
                strLen = ::GetWindowText(::GetDlgItem(hDlg, IDC_AUTH_PASSWORD), pass, 256);
                pass[strLen] = 0;

                result = reinterpret_cast<INT_PTR>(new std::pair<std::wstring, std::wstring>(user, pass));
            }

            ::EndDialog(hDlg, result);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void loadURL(BSTR passedURL)
{
    if (FAILED(gMiniBrowser->loadURL(passedURL)))
        return;

    SetFocus(gMiniBrowser->hwnd());
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
    _variant_t var;
    V_VT(&var) = VT_UI8;
    if (FAILED(statistics->Read(key, &var.GetVARIANT(), nullptr)))
        return;

    unsigned long long value = V_UI8(&var);
    String valueStr = WTF::String::number(value);

    setWindowText(dialog, field, _bstr_t(valueStr.utf8().data()));
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
    if (!gMiniBrowser)
        return;

    IWebCoreStatisticsPtr webCoreStatistics = gMiniBrowser->statistics();
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
    IWebCachePtr webCache = gMiniBrowser->webCache();

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

static void parseCommandLine(bool& usesLayeredWebView, bool& useFullDesktop, bool& pageLoadTesting, _bstr_t& requestedURL)
{
    usesLayeredWebView = false;
    useFullDesktop = false;
    pageLoadTesting = false;

    int argc = 0;
    WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        if (!wcsicmp(argv[i], L"--transparent"))
            usesLayeredWebView = true;
        else if (!wcsicmp(argv[i], L"--desktop"))
            useFullDesktop = true;
        else if (!wcsicmp(argv[i], L"--performance"))
            pageLoadTesting = true;
        else if (!wcsicmp(argv[i], L"--highDPI"))
            continue; // ignore
        else if (!requestedURL)
            requestedURL = argv[i];
    }
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    return wWinMain(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
