/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
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
#include "config.h"
#include "DebuggerClient.h"

#include "DebuggerDocument.h"
#include "Drosera.h"
#include "ServerConnection.h"

#include <WebKit/ForEachCoClass.h>
#include <WebKit/IWebView.h>
#include <WebKit/IWebViewPrivate.h>
#include <WebKit/WebKit.h>
#include <JavaScriptCore/JSContextRef.h>

static LPCTSTR kConsoleTitle = _T("Console");
static LPCTSTR kConsoleClassName = _T("DroseraConsoleWindowClass");

static LRESULT CALLBACK consoleWndProc(HWND, UINT, WPARAM, LPARAM);

void registerConsoleClass(HINSTANCE hInstance)
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass) {
        haveRegisteredWindowClass = true;
        return;
    }

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = 0;
    wcex.lpfnWndProc   = consoleWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = sizeof(DebuggerClient*);
    wcex.hInstance     = hInstance;
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = kConsoleClassName;
    wcex.hIconSm       = 0;

    RegisterClassEx(&wcex);
}

static LRESULT CALLBACK consoleWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    DebuggerClient* client = reinterpret_cast<DebuggerClient*>(longPtr);

    switch (message) {
        case WM_SIZE:
            if (!client)
                return 0;
            return client->onSize(wParam, lParam);
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT DebuggerClient::onSize(WPARAM, LPARAM)
{
    if (!m_webViewPrivate)
        return 0;

    RECT clientRect = {0};
    if (!GetClientRect(m_consoleWindow, &clientRect))
        return 0;

    HWND viewWindow;
    if (SUCCEEDED(m_webViewPrivate->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        SetWindowPos(viewWindow, 0, clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, SWP_NOZORDER);

    return 0;
}

DebuggerClient::DebuggerClient()
    : m_webViewLoaded(false)
    , m_debuggerDocument(new DebuggerDocument(new ServerConnection()))
    , m_globalContext(0)
{
}

DebuggerClient::~DebuggerClient()
{
    if (m_globalContext)
        JSGlobalContextRelease(m_globalContext);
}

// IUnknown ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE DebuggerClient::AddRef()
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 1;
}

ULONG STDMETHODCALLTYPE DebuggerClient::Release()
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 1;
}

// IWebFrameLoadDelegate ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::didFinishLoadForFrame(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebFrame*)
{
    HRESULT ret = S_OK;

    m_webViewLoaded = true;

    COMPtr<IWebFrame> mainFrame;
    ret = webView->mainFrame(&mainFrame);
    if (FAILED(ret))
        return ret;

    if (!m_globalContext) {
        JSGlobalContextRef context = mainFrame->globalContext();
        if (!context)
            return E_FAIL;

        m_globalContext = JSGlobalContextRetain(context);
    }

    if (serverConnected())
        m_debuggerDocument->server()->setGlobalContext(m_globalContext);

    return ret;
}

HRESULT STDMETHODCALLTYPE DebuggerClient::windowScriptObjectAvailable( 
    /* [in] */ IWebView*,
    /* [in] */ JSContextRef context,
    /* [in] */ JSObjectRef windowObject)
{
    JSValueRef exception = 0;
    if (m_debuggerDocument)
        m_debuggerDocument->windowScriptObjectAvailable(context, windowObject, &exception);

    if (exception)
        return E_FAIL;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DebuggerClient::createWebViewWithRequest( 
        /* [in] */ IWebView*,
        /* [in] */ IWebURLRequest* request,
        /* [retval][out] */ IWebView** newWebView)
{
    HRESULT ret = S_OK;

    if (!newWebView)
        return E_POINTER;

    *newWebView = 0;

    HINSTANCE instance = Drosera::getInst();

    registerConsoleClass(instance);

    m_consoleWindow = CreateWindow(kConsoleClassName, kConsoleTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 500, 350, 0, 0, instance, 0);

    if (!m_consoleWindow)
        return HRESULT_FROM_WIN32(GetLastError());

    SetLastError(0);
    SetWindowLongPtr(m_consoleWindow, 0, reinterpret_cast<LONG_PTR>(this));
    ret = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(ret))
        return ret;

    CLSID clsid = CLSID_NULL;
    ret = CLSIDFromProgID(PROGID(WebView), &clsid);
    if (FAILED(ret))
        return ret;

    COMPtr<IWebView> view;
    ret = CoCreateInstance(clsid, 0, CLSCTX_ALL, IID_IWebView, (void**)&view);
    if (FAILED(ret))
        return ret;

    m_webViewPrivate.query(view);
    if (!m_webViewPrivate)
        return E_FAIL;

    ret = view->setHostWindow(reinterpret_cast<OLE_HANDLE>(m_consoleWindow));
    if (FAILED(ret))
        return ret;

    RECT clientRect = {0};
    GetClientRect(m_consoleWindow, &clientRect);
    ret = view->initWithFrame(clientRect, 0, 0);
    if (FAILED(ret))
        return ret;

    ret = view->setUIDelegate(this);
    if (FAILED(ret))
        return ret;

    ret = view->setFrameLoadDelegate(this);
    if (FAILED(ret))
        return ret;

    if (request) {
        BOOL requestIsEmpty = FALSE;
        ret = request->isEmpty(&requestIsEmpty);
        if (FAILED(ret))
            return ret;

        if (!requestIsEmpty) {
            COMPtr<IWebFrame> mainFrame;
            ret = view->mainFrame(&mainFrame);
            if (FAILED(ret))
                return ret;

            ret = mainFrame->loadRequest(request);
            if (FAILED(ret))
                return ret;
        }
    }

    ShowWindow(m_consoleWindow, SW_SHOW);
    UpdateWindow(m_consoleWindow);

    *newWebView = view.releaseRef();

    return S_OK;
}

// IWebUIDelegate ------------------------------
HRESULT STDMETHODCALLTYPE DebuggerClient::runJavaScriptAlertPanelWithMessage(  // For debugging purposes
    /* [in] */ IWebView*,
    /* [in] */ BSTR message)
{
#ifndef NDEBUG
    fwprintf(stderr, L"%s\n", message ? message : L"");
#else
    (void)message;
#endif
    return S_OK;
}

// Pause & Step -------------------------------
void DebuggerClient::resume()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "resume", 0, 0);
}

void DebuggerClient::pause()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "pause", 0, 0);
}

void DebuggerClient::stepInto()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "stepInto", 0, 0);
}

void DebuggerClient::stepOver()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "stepOver", 0, 0);
}

void DebuggerClient::stepOut()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "stepOut", 0, 0);
}

void DebuggerClient::showConsole()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "showConsoleWindow", 0, 0);
}

void DebuggerClient::closeCurrentFile()
{
    DebuggerDocument::callGlobalFunction(m_globalContext, "closeCurrentFile", 0, 0);
}


// Server Connection Functions ----------------
bool DebuggerClient::serverConnected() const
{
    return m_debuggerDocument->server()->serverConnected();
}

void DebuggerClient::attemptToCreateServerConnection()
{
    m_debuggerDocument->server()->attemptToCreateServerConnection(m_globalContext);
}
