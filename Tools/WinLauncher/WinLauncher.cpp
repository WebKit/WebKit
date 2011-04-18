/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Brent Fulgha.  All rights reserved.
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

#include "stdafx.h"
#include "WinLauncher.h"
#include <WebKit/WebKitCOMAPI.h>

#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wininet.h>

#include "PrintWebUIDelegate.h"

#define MAX_LOADSTRING 100
#define URLBAR_HEIGHT  24

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hMainWnd;
HWND hURLBarWnd;
long DefEditProc;
IWebView* gWebView = 0;
HWND gViewWindow = 0;
WinLauncherWebHost* gWebHost = 0;
PrintWebUIDelegate* gPrintDelegate = 0;
TCHAR szTitle[MAX_LOADSTRING];                    // The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    MyEditProc(HWND, UINT, WPARAM, LPARAM);

static void loadURL(BSTR urlBStr);

HRESULT WinLauncherWebHost::updateAddressBar(IWebView* webView)
{
    IWebFrame* mainFrame = 0;
    IWebDataSource* dataSource = 0;
    IWebMutableURLRequest* request = 0;
    BSTR frameURL = 0;

    HRESULT hr = S_OK;

    hr = webView->mainFrame(&mainFrame);
    if (FAILED(hr))
        goto exit;

    hr = mainFrame->dataSource(&dataSource);
    if (FAILED(hr) || !dataSource)
        hr = mainFrame->provisionalDataSource(&dataSource);
    if (FAILED(hr) || !dataSource)
        goto exit;

    hr = dataSource->request(&request);
    if (FAILED(hr) || !request)
        goto exit;

    hr = request->mainDocumentURL(&frameURL);
    if (FAILED(hr))
        goto exit;

    SendMessage(hURLBarWnd, (UINT)WM_SETTEXT, 0, (LPARAM)frameURL);

exit:
    if (mainFrame)
        mainFrame->Release();
    if (dataSource)
        dataSource->Release();
    if (request)
        request->Release();
    SysFreeString(frameURL);
    return 0;
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

static void resizeSubViews()
{
    RECT rcClient;
    GetClientRect(hMainWnd, &rcClient);
    MoveWindow(hURLBarWnd, 0, 0, rcClient.right, URLBAR_HEIGHT, TRUE);
    MoveWindow(gViewWindow, 0, URLBAR_HEIGHT, rcClient.right, rcClient.bottom - URLBAR_HEIGHT, TRUE);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

     // TODO: Place code here.
    MSG msg = {0};
    HACCEL hAccelTable;

    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = 0x00004000; //ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&InitCtrlEx);

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_WINLAUNCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
        return FALSE;

    // Init COM
    OleInitialize(NULL);

    hURLBarWnd = CreateWindow(L"EDIT", 0,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOVSCROLL, 
                        0, 0, 0, 0,
                        hMainWnd,
                        0,
                        hInstance, 0);

    DefEditProc = GetWindowLong(hURLBarWnd, GWL_WNDPROC);
    SetWindowLong(hURLBarWnd, GWL_WNDPROC,(long)MyEditProc);
    SetFocus(hURLBarWnd);

    HRESULT hr = WebKitCreateInstance(CLSID_WebView, 0, IID_IWebView, (void**)&gWebView);
    if (FAILED(hr))
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

    hr = gWebView->setHostWindow((OLE_HANDLE) hMainWnd);
    if (FAILED(hr))
        goto exit;

    RECT clientRect;
    GetClientRect(hMainWnd, &clientRect);
    hr = gWebView->initWithFrame(clientRect, 0, 0);
    if (FAILED(hr))
        goto exit;

    IWebFrame* frame;
    hr = gWebView->mainFrame(&frame);
    if (FAILED(hr))
        goto exit;

    static BSTR defaultHTML = SysAllocString(TEXT("<p style=\"background-color: #00FF00\">Testing</p><img src=\"http://webkit.org/images/icon-gold.png\" alt=\"Face\"><div style=\"border: solid blue\" contenteditable=\"true\">div with blue border</div><ul><li>foo<li>bar<li>baz</ul>"));
    frame->loadHTMLString(defaultHTML, 0);
    frame->Release();

    IWebViewPrivate* viewExt;
    hr = gWebView->QueryInterface(IID_IWebViewPrivate, (void**)&viewExt);
    if (FAILED(hr))
        goto exit;

    hr = viewExt->viewWindow((OLE_HANDLE*) &gViewWindow);
    viewExt->Release();
    if (FAILED(hr) || !gViewWindow)
        goto exit;

    resizeSubViews();

    ShowWindow(gViewWindow, nCmdShow);
    UpdateWindow(gViewWindow);

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINLAUNCHER));

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

exit:
    gPrintDelegate->Release();
    gWebView->Release();
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

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   hMainWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hMainWnd)
      return FALSE;

   ShowWindow(hMainWnd, nCmdShow);
   UpdateWindow(hMainWnd);

   return TRUE;
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

    IWebFrame* frame = 0;
    IWebFramePrivate* framePrivate = 0;
    if (FAILED(gWebView->mainFrame(&frame)))
        goto exit;

    if (FAILED(frame->QueryInterface(&framePrivate)))
        goto exit;

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

exit:
    if (frame)
        frame->Release();
    if (framePrivate)
        framePrivate->Release();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;

    switch (message) {
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
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
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (!gWebView)
            break;
        resizeSubViews();
        break; 
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


#define MAX_URL_LENGTH  1024

LRESULT CALLBACK MyEditProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_CHAR:
            if (wParam == 13) { // Enter Key
                wchar_t strPtr[MAX_URL_LENGTH];
                *((LPWORD)strPtr) = MAX_URL_LENGTH; 
                int strLen = SendMessage(hDlg, EM_GETLINE, 0, (LPARAM)strPtr);

                BSTR bstr = SysAllocStringLen(strPtr, strLen);
                loadURL(bstr);
                SysFreeString(bstr);

                return 0;
            } else
                return (LRESULT)CallWindowProc((WNDPROC)DefEditProc,hDlg,message,wParam,lParam);
            break;
        default:
             return (LRESULT)CallWindowProc((WNDPROC)DefEditProc,hDlg,message,wParam,lParam);
        break;
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

static void loadURL(BSTR urlBStr)
{
    IWebFrame* frame = 0;
    IWebMutableURLRequest* request = 0;

    static BSTR methodBStr = SysAllocString(TEXT("GET"));

    if (urlBStr && urlBStr[0] && (PathFileExists(urlBStr) || PathIsUNC(urlBStr))) {
        TCHAR fileURL[INTERNET_MAX_URL_LENGTH];
        DWORD fileURLLength = sizeof(fileURL)/sizeof(fileURL[0]);

        if (SUCCEEDED(UrlCreateFromPath(urlBStr, fileURL, &fileURLLength, 0)))
            SysReAllocString(&urlBStr, fileURL);
    }

    HRESULT hr = gWebView->mainFrame(&frame);
    if (FAILED(hr))
        goto exit;

    hr = WebKitCreateInstance(CLSID_WebMutableURLRequest, 0, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        goto exit;

    hr = request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 60);
    if (FAILED(hr))
        goto exit;

    hr = request->setHTTPMethod(methodBStr);
    if (FAILED(hr))
        goto exit;

    hr = frame->loadRequest(request);
    if (FAILED(hr))
        goto exit;

    SetFocus(gViewWindow);

exit:
    if (frame)
        frame->Release();
    if (request)
        request->Release();
}
