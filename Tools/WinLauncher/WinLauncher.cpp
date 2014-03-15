/*
 * Copyright (C) 2006, 2008, 2013 Apple Inc.  All rights reserved.
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

#include "stdafx.h"
#include "WinLauncher.h"

#include "AccessibilityDelegate.h"
#include "DOMDefaultImpl.h"
#include "PrintWebUIDelegate.h"
#include "WinLauncherLibResource.h"
#include "WinLauncherReplace.h"
#include <WebKit/WebKitCOMAPI.h>
#include <wtf/ExportMacros.h>
#include <wtf/Platform.h>

#if USE(CF)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if USE(GLIB)
#include <glib.h>
#endif

#include <algorithm>
#include <assert.h>
#include <comip.h>
#include <commctrl.h>
#include <commdlg.h>
#include <comutil.h>
#include <dbghelp.h>
#include <functional>
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
typedef _com_ptr_t<_com_IIID<IWebHistory, &__uuidof(IWebHistory)>> IWebHistoryPtr;
typedef _com_ptr_t<_com_IIID<IWebHistoryItem, &__uuidof(IWebHistoryItem)>> IWebHistoryItemPtr;
typedef _com_ptr_t<_com_IIID<IWebInspector, &__uuidof(IWebInspector)>> IWebInspectorPtr;
typedef _com_ptr_t<_com_IIID<IWebMutableURLRequest, &__uuidof(IWebMutableURLRequest)>> IWebMutableURLRequestPtr;
typedef _com_ptr_t<_com_IIID<IWebPreferences, &__uuidof(IWebPreferences)>> IWebPreferencesPtr;
typedef _com_ptr_t<_com_IIID<IWebPreferencesPrivate, &__uuidof(IWebPreferencesPrivate)>> IWebPreferencesPrivatePtr;
typedef _com_ptr_t<_com_IIID<IWebView, &__uuidof(IWebView)>> IWebViewPtr;
typedef _com_ptr_t<_com_IIID<IWebViewPrivate, &__uuidof(IWebViewPrivate)>> IWebViewPrivatePtr;
typedef _com_ptr_t<_com_IIID<IWebCache, &__uuidof(IWebCache)>> IWebCachePtr;

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hMainWnd;
HWND hURLBarWnd;
HWND hBackButtonWnd;
HWND hForwardButtonWnd;
WNDPROC DefEditProc = 0;
WNDPROC DefButtonProc = 0;
WNDPROC DefWebKitProc = 0;
IWebInspectorPtr gInspector;
IWebViewPtr gWebView;
IWebViewPrivatePtr gWebViewPrivate;
IWebPreferencesPtr gStandardPreferences;
IWebPreferencesPrivatePtr gPrefsPrivate;
HWND gViewWindow = 0;
WinLauncherWebHost* gWebHost = 0;
PrintWebUIDelegate* gPrintDelegate = 0;
AccessibilityDelegate* gAccessibilityDelegate = 0;
IWebHistoryPtr gWebHistory;
std::vector<IWebHistoryItemPtr> gHistoryItems;
TCHAR szTitle[MAX_LOADSTRING];                    // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Support moving the transparent window
POINT s_windowPosition = { 100, 100 };
SIZE s_windowSize = { 800, 400 };
bool s_usesLayeredWebView = false;
bool s_fullDesktop = false;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    EditProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    BackButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ForwardButtonProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    ReloadButtonProc(HWND, UINT, WPARAM, LPARAM);

static void loadURL(BSTR urlBStr);

static bool usesLayeredWebView()
{
    return s_usesLayeredWebView;
}

static bool shouldUseFullDesktop()
{
    return s_fullDesktop;
}

class SimpleEventListener : public DOMEventListener {
public:
    SimpleEventListener(LPWSTR type)
    {
        wcsncpy_s(m_eventType, 100, type, 100);
        m_eventType[99] = 0;
    }

    virtual HRESULT STDMETHODCALLTYPE handleEvent(IDOMEvent* evt)
    {
        wchar_t message[255];
        wcscpy_s(message, 255, m_eventType);
        wcscat_s(message, 255, L" event fired!");
        ::MessageBox(0, message, L"Event Handler", MB_OK);
        return S_OK;
    }

private:
    wchar_t m_eventType[100];
};

typedef _com_ptr_t<_com_IIID<IWebDataSource, &__uuidof(IWebDataSource)>> IWebDataSourcePtr;

HRESULT WinLauncherWebHost::updateAddressBar(IWebView* webView)
{
    IWebFramePtr mainFrame;
    HRESULT hr = webView->mainFrame(&mainFrame.GetInterfacePtr());
    if (FAILED(hr))
        return 0;

    IWebDataSourcePtr dataSource;
    hr = mainFrame->dataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        hr = mainFrame->provisionalDataSource(&dataSource.GetInterfacePtr());
    if (FAILED(hr) || !dataSource)
        return 0;

    IWebMutableURLRequestPtr request;
    hr = dataSource->request(&request.GetInterfacePtr());
    if (FAILED(hr) || !request)
        return 0;

    _bstr_t frameURL;
    hr = request->mainDocumentURL(frameURL.GetAddress());
    if (FAILED(hr))
        return 0;

    ::SendMessage(hURLBarWnd, static_cast<UINT>(WM_SETTEXT), 0, reinterpret_cast<LPARAM>(frameURL.GetBSTR()));

    return 0;
}

HRESULT WinLauncherWebHost::didFailProvisionalLoadWithError(IWebView*, IWebError *error, IWebFrame*)
{
    _bstr_t errorDescription;
    HRESULT hr = error->localizedDescription(errorDescription.GetAddress());
    if (FAILED(hr))
        errorDescription = L"Failed to load page and to localize error description.";

    if (_wcsicmp(errorDescription, L"Cancelled"))
        ::MessageBoxW(0, static_cast<LPCWSTR>(errorDescription), L"Error", MB_APPLMODAL | MB_OK);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WinLauncherWebHost::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WinLauncherWebHost::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WinLauncherWebHost::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

static void updateMenuItemForHistoryItem(HMENU menu, IWebHistoryItem& historyItem, int currentHistoryItem)
{
    UINT menuID = IDM_HISTORY_LINK0 + currentHistoryItem;

    MENUITEMINFO menuItemInfo = {0};
    menuItemInfo.cbSize = sizeof(MENUITEMINFO);
    menuItemInfo.fMask = MIIM_TYPE;
    menuItemInfo.fType = MFT_STRING;

    _bstr_t title;
    historyItem.title(title.GetAddress());
    menuItemInfo.dwTypeData = static_cast<LPWSTR>(title);

    ::SetMenuItemInfo(menu, menuID, FALSE, &menuItemInfo);
    ::EnableMenuItem(menu, menuID, MF_BYCOMMAND | MF_ENABLED);
}

static void showLastVisitedSites(IWebView& webView)
{
    HMENU menu = ::GetMenu(hMainWnd);

    _com_ptr_t<_com_IIID<IWebBackForwardList, &__uuidof(IWebBackForwardList)>> backForwardList;
    HRESULT hr = webView.backForwardList(&backForwardList.GetInterfacePtr());
    if (FAILED(hr))
        return;

    int capacity = 0;
    hr = backForwardList->capacity(&capacity);
    if (FAILED(hr))
        return;

    int backCount = 0;
    hr = backForwardList->backListCount(&backCount);
    if (FAILED(hr))
        return;

    UINT backSetting = MF_BYCOMMAND | (backCount) ? MF_ENABLED : MF_DISABLED;
    ::EnableMenuItem(menu, IDM_HISTORY_BACKWARD, backSetting);

    int forwardCount = 0;
    hr = backForwardList->forwardListCount(&forwardCount);
    if (FAILED(hr))
        return;

    UINT forwardSetting = MF_BYCOMMAND | (forwardCount) ? MF_ENABLED : MF_DISABLED;
    ::EnableMenuItem(menu, IDM_HISTORY_FORWARD, forwardSetting);

    IWebHistoryItemPtr currentItem;
    hr = backForwardList->currentItem(&currentItem.GetInterfacePtr());
    if (FAILED(hr))
        return;

    hr = gWebHistory->addItems(1, &currentItem.GetInterfacePtr());
    if (FAILED(hr))
        return;

    _com_ptr_t<_com_IIID<IWebHistoryPrivate, &__uuidof(IWebHistoryPrivate)>> webHistory;
    hr = gWebHistory->QueryInterface(IID_IWebHistoryPrivate, reinterpret_cast<void**>(&webHistory.GetInterfacePtr()));
    if (FAILED(hr))
        return;

    int totalListCount = 0;
    hr = webHistory->allItems(&totalListCount, 0);
    if (FAILED(hr))
        return;

    gHistoryItems.resize(totalListCount);

    std::vector<IWebHistoryItem*> historyToLoad(totalListCount);
    hr = webHistory->allItems(&totalListCount, historyToLoad.data());
    if (FAILED(hr))
        return;

    size_t i = 0;
    for (auto cur = historyToLoad.begin(); cur != historyToLoad.end(); ++cur) {
        gHistoryItems[i].Attach(*cur);
        ++i;
    }

    int allItemsOffset = 0;
    if (totalListCount > maxHistorySize)
        allItemsOffset = totalListCount - maxHistorySize;

    int currentHistoryItem = 0;
    for (int i = 0; i < totalListCount; ++i) {
        updateMenuItemForHistoryItem(menu, *(gHistoryItems[allItemsOffset + currentHistoryItem]), currentHistoryItem);
        ++currentHistoryItem;
    }

    // Hide any history we aren't using yet.
    for (int i = currentHistoryItem; i < maxHistorySize; ++i)
        ::EnableMenuItem(menu, IDM_HISTORY_LINK0 + i, MF_BYCOMMAND | MF_DISABLED);
}

typedef _com_ptr_t<_com_IIID<IDOMDocument, &__uuidof(IDOMDocument)>> IDOMDocumentPtr;
typedef _com_ptr_t<_com_IIID<IDOMElement, &__uuidof(IDOMElement)>> IDOMElementPtr;
typedef _com_ptr_t<_com_IIID<IDOMEventTarget, &__uuidof(IDOMEventTarget)>> IDOMEventTargetPtr;

HRESULT WinLauncherWebHost::didFinishLoadForFrame(IWebView* webView, IWebFrame* frame)
{
    IDOMDocumentPtr doc;
    frame->DOMDocument(&doc.GetInterfacePtr());

    IDOMElementPtr element;
    IDOMEventTargetPtr target;

    showLastVisitedSites(*webView);

    // The following is for the test page:
    HRESULT hr = doc->getElementById(L"webkit logo", &element.GetInterfacePtr());
    if (!SUCCEEDED(hr))
        return hr;

    hr = element->QueryInterface(IID_IDOMEventTarget, reinterpret_cast<void**>(&target.GetInterfacePtr()));
    if (!SUCCEEDED(hr))
        return hr;

    hr = target->addEventListener(L"click", new SimpleEventListener (L"webkit logo click"), FALSE);
    if (!SUCCEEDED(hr))
        return hr;

    return hr;
}

static void resizeSubViews()
{
    if (usesLayeredWebView() || !gViewWindow)
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
    if (reason == DLL_PROCESS_ATTACH)
        hInst = dllInstance;

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

static bool setToDefaultPreferences()
{
    HRESULT hr = gStandardPreferences->QueryInterface(IID_IWebPreferencesPrivate, reinterpret_cast<void**>(&gPrefsPrivate.GetInterfacePtr()));
    if (!SUCCEEDED(hr))
        return false;

#if USE(CG)
    gStandardPreferences->setAVFoundationEnabled(TRUE);
    gPrefsPrivate->setAcceleratedCompositingEnabled(TRUE);
#endif

    gPrefsPrivate->setFullScreenEnabled(TRUE);
    gPrefsPrivate->setShowDebugBorders(FALSE);
    gPrefsPrivate->setShowRepaintCounter(FALSE);

    gStandardPreferences->setLoadsImagesAutomatically(TRUE);
    gPrefsPrivate->setAuthorAndUserStylesEnabled(TRUE);
    gStandardPreferences->setJavaScriptEnabled(TRUE);
    gPrefsPrivate->setAllowUniversalAccessFromFileURLs(FALSE);
    gPrefsPrivate->setAllowFileAccessFromFileURLs(TRUE);

    gPrefsPrivate->setDeveloperExtrasEnabled(TRUE);

    return true;
}

static bool setCacheFolder()
{
    IWebCachePtr webCache;

    HRESULT hr = WebKitCreateInstance(CLSID_WebCache, 0, __uuidof(webCache), reinterpret_cast<void**>(&webCache.GetInterfacePtr()));
    if (FAILED(hr))
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow)
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

     // TODO: Place code here.
    MSG msg = {0};
    HACCEL hAccelTable;

    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = 0x00004000; //ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&InitCtrlEx);

    _bstr_t requestedURL;
    int argc = 0;
    WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        if (!wcsicmp(argv[i], L"--transparent"))
            s_usesLayeredWebView = true;
        else if (!wcsicmp(argv[i], L"--desktop"))
            s_fullDesktop = true;
        else if (!requestedURL)
            requestedURL = argv[i];
    }

    // Initialize global strings
    LoadString(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInst, IDC_WINLAUNCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInst);

    if (shouldUseFullDesktop())
        computeFullDesktopFrame();

    // Init COM
    OleInitialize(NULL);

    if (usesLayeredWebView()) {
        hURLBarWnd = CreateWindow(L"EDIT", L"Type URL Here",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, 
                    s_windowPosition.x, s_windowPosition.y + s_windowSize.cy, s_windowSize.cx, URLBAR_HEIGHT,
                    0,
                    0,
                    hInst, 0);
    } else {
        hMainWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInst, 0);

        if (!hMainWnd)
            return FALSE;

        hBackButtonWnd = CreateWindow(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE  | BS_TEXT, 0, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hForwardButtonWnd = CreateWindow(L"BUTTON", L">", WS_CHILD | WS_VISIBLE  | BS_TEXT, CONTROLBUTTON_WIDTH, 0, 0, 0, hMainWnd, 0, hInst, 0);
        hURLBarWnd = CreateWindow(L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, CONTROLBUTTON_WIDTH * 2, 0, 0, 0, hMainWnd, 0, hInst, 0);

        ShowWindow(hMainWnd, nCmdShow);
        UpdateWindow(hMainWnd);
    }

    DefEditProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC));
    DefButtonProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC));
    SetWindowLongPtr(hURLBarWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
    SetWindowLongPtr(hBackButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BackButtonProc));
    SetWindowLongPtr(hForwardButtonWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ForwardButtonProc));

    SetFocus(hURLBarWnd);

    RECT clientRect = { s_windowPosition.x, s_windowPosition.y, s_windowPosition.x + s_windowSize.cx, s_windowPosition.y + s_windowSize.cy };

    IWebPreferencesPtr tmpPreferences;
    if (FAILED(WebKitCreateInstance(CLSID_WebPreferences, 0, IID_IWebPreferences, reinterpret_cast<void**>(&tmpPreferences.GetInterfacePtr()))))
        goto exit;

    if (FAILED(tmpPreferences->standardPreferences(&gStandardPreferences.GetInterfacePtr())))
        goto exit;

    if (!setToDefaultPreferences())
        goto exit;

    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, reinterpret_cast<void**>(&gWebView.GetInterfacePtr()));
    if (FAILED(hr))
        goto exit;

    hr = gWebView->QueryInterface(IID_IWebViewPrivate, reinterpret_cast<void**>(&gWebViewPrivate.GetInterfacePtr()));
    if (FAILED(hr))
        goto exit;

    hr = WebKitCreateInstance(CLSID_WebHistory, 0, __uuidof(gWebHistory), reinterpret_cast<void**>(&gWebHistory.GetInterfacePtr()));
    if (FAILED(hr))
        goto exit;

    if (!setCacheFolder())
        goto exit;

    gWebHost = new WinLauncherWebHost();
    gWebHost->AddRef();
    hr = gWebView->setFrameLoadDelegate(gWebHost);
    if (FAILED(hr))
        goto exit;

    gPrintDelegate = new PrintWebUIDelegate;
    gPrintDelegate->AddRef();
    hr = gWebView->setUIDelegate(gPrintDelegate);
    if (FAILED (hr))
        goto exit;

    gAccessibilityDelegate = new AccessibilityDelegate;
    gAccessibilityDelegate->AddRef();
    hr = gWebView->setAccessibilityDelegate(gAccessibilityDelegate);
    if (FAILED (hr))
        goto exit;

    hr = gWebView->setHostWindow(reinterpret_cast<OLE_HANDLE>(hMainWnd));
    if (FAILED(hr))
        goto exit;

    hr = gWebView->initWithFrame(clientRect, 0, 0);
    if (FAILED(hr))
        goto exit;

    if (!requestedURL) {
        IWebFramePtr frame;
        hr = gWebView->mainFrame(&frame.GetInterfacePtr());
        if (FAILED(hr))
            goto exit;

        frame->loadHTMLString(_bstr_t(defaultHTML).GetBSTR(), 0);
    }

    hr = gWebViewPrivate->setTransparent(usesLayeredWebView());
    if (FAILED(hr))
        goto exit;

    hr = gWebViewPrivate->setUsesLayeredWindow(usesLayeredWebView());
    if (FAILED(hr))
        goto exit;

    hr = gWebViewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&gViewWindow));
    if (FAILED(hr) || !gViewWindow)
        goto exit;

    if (usesLayeredWebView())
        subclassForLayeredWindow();

    resizeSubViews();

    ShowWindow(gViewWindow, nCmdShow);
    UpdateWindow(gViewWindow);

    hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_WINLAUNCHER));

    if (requestedURL.length())
        loadURL(requestedURL.GetBSTR());

#pragma warning(disable:4509)

    // Main message loop:
    __try {
        while (GetMessage(&msg, 0, 0, 0)) {
#if USE(CF)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
#endif
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
#if USE(GLIB)
            g_main_context_iteration(0, false);
#endif
        }
    } __except(createCrashReport(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) { }

exit:
    gPrintDelegate->Release();

    shutDownWebKit();
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // Shut down COM.
    OleUninitialize();
    
    return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINLAUNCHER));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_WINLAUNCHER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
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

    IWebFramePtr frame;
    IWebFramePrivatePtr framePrivate;
    if (FAILED(gWebView->mainFrame(&frame.GetInterfacePtr())))
        return;

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

    if (!gStandardPreferences || !gPrefsPrivate)
        return;

    switch (menuID) {
    case IDM_AVFOUNDATION:
        gStandardPreferences->setAVFoundationEnabled(newState);
        break;
    case IDM_ACC_COMPOSITING:
        gPrefsPrivate->setAcceleratedCompositingEnabled(newState);
        break;
    case IDM_WK_FULLSCREEN:
        gPrefsPrivate->setFullScreenEnabled(newState);
        break;
    case IDM_COMPOSITING_BORDERS:
        gPrefsPrivate->setShowDebugBorders(newState);
        gPrefsPrivate->setShowRepaintCounter(newState);
        break;
    case IDM_DISABLE_IMAGES:
        gStandardPreferences->setLoadsImagesAutomatically(!newState);
        break;
    case IDM_DISABLE_STYLES:
        gPrefsPrivate->setAuthorAndUserStylesEnabled(!newState);
        break;
    case IDM_DISABLE_JAVASCRIPT:
        gStandardPreferences->setJavaScriptEnabled(!newState);
        break;
    case IDM_DISABLE_LOCAL_FILE_RESTRICTIONS:
        gPrefsPrivate->setAllowUniversalAccessFromFileURLs(newState);
        gPrefsPrivate->setAllowFileAccessFromFileURLs(newState);
        break;
    }

    info.fState = (newState) ? MFS_CHECKED : MFS_UNCHECKED;

    ::SetMenuItemInfo(menu, menuID, FALSE, &info);
}

static void LaunchInspector(HWND hwnd)
{
    if (!gWebViewPrivate)
        return;

    if (!SUCCEEDED(gWebViewPrivate->inspector(&gInspector.GetInterfacePtr())))
        return;

    gInspector->show();
}

static void NavigateForwardOrBackward(HWND hWnd, UINT menuID)
{
    if (!gWebView)
        return;

    BOOL wentBackOrForward = FALSE;
    if (IDM_HISTORY_FORWARD == menuID)
        gWebView->goForward(&wentBackOrForward);
    else
        gWebView->goBack(&wentBackOrForward);
}

static void NavigateToHistory(HWND hWnd, UINT menuID)
{
    if (!gWebView)
        return;

    int historyEntry = menuID - IDM_HISTORY_LINK0;
    if (historyEntry > gHistoryItems.size())
        return;

    IWebHistoryItemPtr desiredHistoryItem = gHistoryItems[historyEntry];
    if (!desiredHistoryItem)
        return;

    BOOL succeeded = FALSE;
    gWebView->goToBackForwardItem(desiredHistoryItem, &succeeded);

    _bstr_t frameURL;
    desiredHistoryItem->URLString(frameURL.GetAddress());

    ::SendMessage(hURLBarWnd, (UINT)WM_SETTEXT, 0, (LPARAM)frameURL.GetBSTR());
}

static const int dragBarHeight = 30;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC parentProc = usesLayeredWebView() ? DefWebKitProc : DefWindowProc;

    switch (message) {
    case WM_NCHITTEST:
        if (usesLayeredWebView()) {
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
            NavigateToHistory(hWnd, wmId);
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
            LaunchInspector(hWnd);
            break;
        case IDM_HISTORY_BACKWARD:
        case IDM_HISTORY_FORWARD:
            NavigateForwardOrBackward(hWnd, wmId);
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
        if (!gWebView || usesLayeredWebView())
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
    BOOL wentBack = FALSE;
    switch (message) {
    case WM_LBUTTONUP:
        gWebView->goBack(&wentBack);
    default:
        return CallWindowProc(DefButtonProc, hDlg, message, wParam, lParam);
    }
}

LRESULT CALLBACK ForwardButtonProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BOOL wentForward = FALSE;
    switch (message) {
    case WM_LBUTTONUP:
        gWebView->goForward(&wentForward);
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

static void loadURL(BSTR passedURL)
{
    _bstr_t urlBStr(passedURL);
    if (!!urlBStr && (::PathFileExists(urlBStr) || ::PathIsUNC(urlBStr))) {
        TCHAR fileURL[INTERNET_MAX_URL_LENGTH];
        DWORD fileURLLength = sizeof(fileURL)/sizeof(fileURL[0]);

        if (SUCCEEDED(::UrlCreateFromPath(urlBStr, fileURL, &fileURLLength, 0)))
            urlBStr = fileURL;
    }

    IWebFramePtr frame;
    HRESULT hr = gWebView->mainFrame(&frame.GetInterfacePtr());
    if (FAILED(hr))
        return;

    IWebMutableURLRequestPtr request;
    hr = WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        return;

    hr = request->initWithURL(wcsstr(static_cast<wchar_t*>(urlBStr), L"://") ? urlBStr : _bstr_t(L"http://") + urlBStr, WebURLRequestUseProtocolCachePolicy, 60);
    if (FAILED(hr))
        return;

    _bstr_t methodBStr(L"GET");
    hr = request->setHTTPMethod(methodBStr);
    if (FAILED(hr))
        return;

    hr = frame->loadRequest(request);
    if (FAILED(hr))
        return;

    SetFocus(gViewWindow);
}

extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpstrCmdLine, int nCmdShow)
{
    return wWinMain(hInstance, hPrevInstance, lpstrCmdLine, nCmdShow);
}
