/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "WebInspector.h"

#include "DOMCoreClasses.h"
#include "DOMExtensions.h"
#include "WebFrame.h"
#include "WebKit.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include "WebView.h"

#include <CoreFoundation/CoreFoundation.h>

#pragma warning(push, 0)
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebCore/BString.h>
#include <WebCore/Document.h>
#include <WebCore/Page.h>
#include <WebCore/kjs_dom.h>
#include <WebCore/kjs_proxy.h>
#pragma warning(pop)

#include <tchar.h>
#include <windowsx.h>
#include <wtf/Vector.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

static const int inspectorMinWidth = 280;
static const int inspectorMinHeight = 450;
static const IntSize optionsMenuOffset(-8, 9);

static const LRESULT kNotHandledResult = -1;

static LPCTSTR kWebInspectorWindowClassName = TEXT("WebInspectorWindowClass");
static ATOM registerWindowClass();
static LPCTSTR kWebInspectorPointerProp = TEXT("WebInspectorPointer");

enum OptionsMenuIdentifier {
    IgnoreWhitespaceIdentifier,
    ShowUserAgentStylesIdentifier
};

class WebInspectorPrivate {
public:
    WebInspectorPrivate()
        : hWnd(0)
        , webViewLoaded(false)
        , preventHighlight(false)
        , isSharedInspector(false)
        , isResizing(false)
        , isDragging(false)
        , originalWebViewWndProc(0)
    {
    }

    HWND hWnd;
    COMPtr<WebView> webView;
    COMPtr<IDOMHTMLDocument> domDocument;
    COMPtr<WebFrame> webFrame;
    COMPtr<IDOMNode> rootNode;
    COMPtr<IDOMNode> focusedNode;
    String searchQuery;
    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    WebNodeHighlight *currentHighlight;
#endif
    bool webViewLoaded;
    bool preventHighlight;
    bool isSharedInspector;

    bool isResizing;
    bool isDragging;
    IntRect windowRectAtButtonDown;
    // This is in client coordinates while moving the window, and screen coordinates while resizing.
    IntPoint buttonDownPoint;

    WNDPROC originalWebViewWndProc;
};

WebInspector* WebInspector::sharedWebInspector()
{
    static WebInspector* sharedWebInspector;
    if (!sharedWebInspector) {
        sharedWebInspector = new WebInspector;
        sharedWebInspector->m_private->isSharedInspector = true;
    }
    return sharedWebInspector;
}

WebInspector::WebInspector()
    : m_private(new WebInspectorPrivate)
{
    registerWindowClass();

    // We set our size/position after creating the WebView so that it will be positioned as well.
    m_private->hWnd = ::CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_LAYERED, kWebInspectorWindowClassName, 0, WS_POPUP,
                                       0, 0, 0, 0,
                                       0, 0, 0, 0);
    if (!m_private->hWnd)
        return;

#pragma warning(disable: 4244)
    ::SetWindowLongPtr(m_private->hWnd, 0, reinterpret_cast<LONG_PTR>(this));
    ::SetLayeredWindowAttributes(m_private->hWnd, 0, (255 * 95) / 100, LWA_ALPHA);

    // FIXME: Add a shadow around the window

    m_private->webView.adoptRef(WebView::createInstance());
    if (FAILED(m_private->webView->setHostWindow((OLE_HANDLE)(ULONG64)m_private->hWnd)))
        return;

    if (FAILED(m_private->webView->setFrameLoadDelegate(this)))
        return;

    if (FAILED(m_private->webView->setUIDelegate(this)))
        return;

    RECT rect = {0};
    if (FAILED(m_private->webView->initWithFrame(rect, 0, 0)))
        return;

    // FIXME: Not implemented yet
#if 0
    if (FAILED(m_private->webView->setDrawsBackground(FALSE)))
        return;
#endif

    HWND viewWindow;
    if (FAILED(m_private->webView->viewWindow((OLE_HANDLE*)&viewWindow)))
        return;

    ::SetProp(viewWindow, kWebInspectorPointerProp, (HANDLE)this);

#pragma warning(disable: 4244 4312)
    m_private->originalWebViewWndProc = (WNDPROC)::SetWindowLongPtr(viewWindow, GWLP_WNDPROC, (LONG_PTR)SubclassedWebViewWndProc);

    IntRect frame(IntPoint(60, 200), IntSize(350, 550));
    ::SetWindowPos(m_private->hWnd, HWND_TOPMOST, frame.x(), frame.y(), frame.width(), frame.height(), 0);
    ::ShowWindow(viewWindow, SW_SHOWNORMAL);

    // FIXME: We need WebPreferences::initWithIdentifier to work here
#if 0
    // Keep preferences separate from the rest of the client, making sure we are using expected preference values.
    // One reason this is good is that it keeps the inspector out of history via "private browsing".
    COMPtr<WebPreferences> tempPreferences;
    tempPreferences.adoptRef(WebPreferences::createInstance());

    COMPtr<IWebPreferences> preferences;
    if (FAILED(tempPreferences->initWithIdentifier(BString("WebInspectorPreferences"), &preferences)))
    
    preferences->setPrivateBrowsingEnabled(TRUE);
    preferences->setLoadsImagesAutomatically(TRUE);
    preferences->setPlugInsEnabled(TRUE);
    preferences->setJavaScriptEnabled(TRUE);
    preferences->setUserStyleSheetEnabled(FALSE);
    preferences->setTabsToLinks(FALSE);
    preferences->setMinimumFontSize(0);
    preferences->setMinimumLogicalFontSize(9);

    if (FAILED(m_private->webView->setPreferences(preferences.get())))
        return;
#endif

#if 0
        [_private->webView setProhibitsMainFrameScrolling:YES];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows to:YES];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse to:YES];
#endif

    COMPtr<IWebFrame> mainFrame;
    if (FAILED(m_private->webView->mainFrame(&mainFrame)))
        return;

    COMPtr<WebMutableURLRequest> request;
    request.adoptRef(WebMutableURLRequest::createInstance());

    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit")), CFSTR("inspector"), CFSTR("html"), CFSTR("webInspector")));
    if (!htmlURLRef)
        return;

    CFStringRef urlStringRef = ::CFURLGetString(htmlURLRef.get());
    if (FAILED(request->initWithURL(BString(urlStringRef), WebURLRequestUseProtocolCachePolicy, 60)))
        return;

    mainFrame->loadRequest(request.get());

    // FIXME: Implement window size/position save/restore
#if 0
    [self setWindowFrameAutosaveName:@"Web Inspector"];
#endif
}

WebInspector::~WebInspector()
{
}

HRESULT STDMETHODCALLTYPE WebInspector::webViewClose( 
    /* [in] */ IWebView*)
{
    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebNodeHighlightExpiredNotification object:nil];
#endif

#if 0
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewFrameDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSSystemColorsDidChangeNotification object:nil];
#endif

    setWebFrame(0);

    if (m_private->isSharedInspector)
        ::ShowWindow(m_private->hWnd, SW_HIDE);
    else
        ::PostMessage(m_private->hWnd, WM_CLOSE, 0, 0);

    return S_OK;
}

void WebInspector::show()
{
    if (!m_private->hWnd)
        return;

    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_nodeHighlightExpired:) name:WebNodeHighlightExpiredNotification object:nil];
#endif

#if 0
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateSystemColors) name:NSSystemColorsDidChangeNotification object:nil];
#endif

    update();
    updateSystemColors();

    ::SetWindowPos(m_private->hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);

    HWND viewWindow;
    if (FAILED(m_private->webView->viewWindow((OLE_HANDLE*)&viewWindow)))
        return;

    ::SetFocus(viewWindow);
}

LRESULT WebInspector::onDestroy(WPARAM, LPARAM)
{
    HWND viewWindow;
    if (SUCCEEDED(m_private->webView->viewWindow((OLE_HANDLE*)&viewWindow))) {
        ::SetWindowLongPtr(viewWindow, GWLP_WNDPROC, (LONG_PTR)m_private->originalWebViewWndProc);
        ::RemoveProp(viewWindow, kWebInspectorPointerProp);
    }

    delete this;

    return 0;
}

LRESULT WebInspector::onSize(WPARAM, LPARAM)
{
    if (!m_private->webView)
        return 0;

    RECT clientRect = {0};
    ::GetClientRect(m_private->hWnd, &clientRect);

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(m_private->webView->QueryInterface(IID_IWebViewPrivate, (void**)&viewPrivate)))
        return 0;

    HWND viewWindow;
    if (FAILED(viewPrivate->viewWindow((OLE_HANDLE*)&viewWindow)))
        return 0;

    ::SetWindowPos(viewWindow, 0, clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, SWP_NOZORDER);
    return 0;
}

LRESULT WebInspector::handleMessageSentToWebView(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_LBUTTONDOWN:
            return onLButtonDown(wParam, lParam);
        case WM_LBUTTONUP:
            return onLButtonUp(wParam, lParam);
        case WM_MOUSEMOVE:
            return onMouseMove(wParam, lParam);
    }

    return kNotHandledResult;
}

LRESULT WebInspector::onLButtonDown(WPARAM, LPARAM lParam)
{
    ASSERT(!m_private->isResizing && !m_private->isDragging);

    IntPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    POINT screenPoint = point;
    ::ClientToScreen(m_private->hWnd, &screenPoint);

    Document* document = m_private->webView->page()->mainFrame()->document();
    if (!document)
        return kNotHandledResult;

    const Vector<DashboardRegionValue>& regions = document->dashboardRegions();
    const size_t regionsCount = regions.size();

    for (unsigned i = 0; i < regionsCount; ++i) {
        if (regions[i].clip.contains(point)) {
            if (regions[i].label == "resize") {
                ::SetCapture(m_private->hWnd);
                m_private->isResizing = true;
                m_private->buttonDownPoint = screenPoint;
                RECT windowRect;
                ::GetWindowRect(m_private->hWnd, &windowRect);
                m_private->windowRectAtButtonDown = windowRect;
                return 0;
            }
            if (regions[i].label == "control")
                return kNotHandledResult;
        }
    }

    // The click wasn't in any controls or in the resizer, so we must be dragging the window
    ::SetCapture(m_private->hWnd);
    m_private->isDragging = true;
    m_private->buttonDownPoint = point;
    return 0;
}

LRESULT WebInspector::onLButtonUp(WPARAM, LPARAM)
{
    if (!m_private->isResizing && !m_private->isDragging)
        return kNotHandledResult;

    ::ReleaseCapture();

    m_private->isResizing = false;
    m_private->isDragging = false;

    return 0;
}

LRESULT WebInspector::onMouseMove(WPARAM, LPARAM lParam)
{
    if (!m_private->isResizing && !m_private->isDragging)
        return kNotHandledResult;

    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    if (m_private->isResizing)
        ::ClientToScreen(m_private->hWnd, &pt);
    IntPoint point(pt);

    if (point == m_private->buttonDownPoint)
        return 0;
    
    IntSize delta(point.x() - m_private->buttonDownPoint.x(), point.y() - m_private->buttonDownPoint.y());

    RECT rect;
    ::GetWindowRect(m_private->hWnd, &rect);
    IntRect windowRect(rect);

    if (m_private->isResizing) {
        IntSize newSize = m_private->windowRectAtButtonDown.size() + delta;
        newSize.setWidth(max(newSize.width(), inspectorMinWidth));
        newSize.setHeight(max(newSize.height(), inspectorMinHeight));
        windowRect.setSize(newSize);
    } else
        windowRect.move(delta);

    ::SetWindowPos(m_private->hWnd, 0, windowRect.x(), windowRect.y(), windowRect.width(), windowRect.height(), SWP_NOZORDER);
    return 0;
}

HRESULT STDMETHODCALLTYPE WebInspector::setFrame( 
    /* [in] */ IWebView*,
    /* [in] */ RECT* lpRect)
{
    if (!lpRect) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    ::SetWindowPos(m_private->hWnd, 0, lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top, SWP_NOZORDER);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebInspector::webViewFrame( 
    /* [in] */ IWebView*,
    /* [in] */ RECT* lpRect)
{
    if (!lpRect) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    ::GetWindowRect(m_private->hWnd, lpRect);
    return S_OK;
}

void WebInspector::setWebFrame(WebFrame* webFrame)
{
    if (webFrame == m_private->webFrame.get())
        return;

    if (m_private->webFrame) {
        COMPtr<IWebView> webView;
        if (SUCCEEDED(m_private->webFrame->webView(&webView))) {
            COMPtr<IUnknown> webViewUnknown;
            if (SUCCEEDED(webView->QueryInterface(IID_IUnknown, (void**)&webViewUnknown)))
                WebNotificationCenter::defaultCenterInternal()->removeObserver(this, WebViewProgressFinishedNotification, webViewUnknown.get());
        }
        m_private->webFrame->removeInspector(this);
    }

    m_private->webFrame = webFrame;

    if (m_private->webFrame) {
        COMPtr<IWebView> webView;
        if (SUCCEEDED(m_private->webFrame->webView(&webView))) {
            COMPtr<IUnknown> webViewUnknown;
            if (SUCCEEDED(webView->QueryInterface(IID_IUnknown, (void**)&webViewUnknown)))
                WebNotificationCenter::defaultCenterInternal()->addObserver(this, WebViewProgressFinishedNotification, webViewUnknown.get());
        }
        m_private->webFrame->addInspector(this);

        m_private->preventHighlight = true;
        COMPtr<IDOMDocument> iDOMDoc;
        if (SUCCEEDED(m_private->webFrame->DOMDocument(&iDOMDoc))) {
            COMPtr<IDOMNode> node;
            if (SUCCEEDED(iDOMDoc->QueryInterface(IID_IDOMNode, (void**)&node)))
                setFocusedDOMNode(node.get());
        }
        m_private->preventHighlight = false;
    }
}

WebFrame* WebInspector::webFrame() const
{
    return m_private->webFrame.get();
}

void WebInspector::setRootDOMNode(IDOMNode* node)
{
    if (node == m_private->rootNode.get())
        return;

    m_private->rootNode = node;

    updateRoot();
}

IDOMNode* WebInspector::rootDOMNode() const
{
    return m_private->rootNode.get();
}

static bool isAncestor(IDOMNode* parent, IDOMNode* child)
{
    ASSERT(parent);
    ASSERT(child);

    COMPtr<IDOMNode> currentNode;
    if (FAILED(child->parentNode(&currentNode)))
        return false;

    while (currentNode) {
        BOOL same = FALSE;
        if (SUCCEEDED(parent->isSameNode(currentNode.get(), &same)) && same)
            return true;
        COMPtr<IDOMNode> nextAncestor;
        if (FAILED(currentNode->parentNode(&nextAncestor)))
            return false;
        currentNode = nextAncestor;
    }
    return false;
}

void WebInspector::setFocusedDOMNode(IDOMNode* node)
{
    if (node == m_private->focusedNode.get())
        return;

    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    [[NSRunLoop currentRunLoop] cancelPerformSelector:@selector(_highlightNode:) target:self argument:_private->focusedNode];
#endif

    m_private->focusedNode = node;

    IDOMNode* root = m_private->rootNode.get();
    if (!root || (root != node && !isAncestor(root, node)))
        setRootDOMNode(node);

    if (!m_private->webViewLoaded)
        return;

    update();

    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    if (!_private->preventHighlight) {
        NSRect bounds = [node boundingBox];
        if (!NSIsEmptyRect(bounds)) {
            NSRect visible = [[[_private->webFrame frameView] documentView] visibleRect];
            BOOL needsScroll = !NSContainsRect(visible, bounds) && !NSContainsRect(bounds, visible);

            // only scroll if the bounds isn't in the visible rect and dosen't contain the visible rect
            if (needsScroll) {
                // scroll to the parent element if we aren't focused on an element
                DOMElement *element = (DOMElement *)_private->focusedNode;
                if (![element isKindOfClass:[DOMElement class]])
                    element = (DOMElement *)[element parentNode];

                if ([element isKindOfClass:[DOMElement class]])
                    [element scrollIntoViewIfNeeded:YES];

                // give time for the scroll to happen
                [self performSelector:@selector(_highlightNode:) withObject:node afterDelay:0.25];
            } else
                [self _highlightNode:node];
        } else
            [_private->currentHighlight expire];
    }
#endif
}

IDOMNode* WebInspector::focusedDOMNode() const
{
    return m_private->focusedNode.get();
}

void WebInspector::setSearchQuery(const String& /*query*/)
{
    // FIXME: Searching should be done entirely within inspector.js
#if 0
    if (_private->webViewLoaded) {
        if (!query)
            query = @"";
        [[_private->webView windowScriptObject] callWebScriptMethod:@"performSearch" withArguments:[NSArray arrayWithObject:query]];
        DOMHTMLInputElement *search = (DOMHTMLInputElement *)[_private->domDocument getElementById:@"search"];
        [search setValue:query];
    } else {
        [query retain];
        [_private->searchQuery release];
        _private->searchQuery = query;
    }
#endif
}

const String& WebInspector::searchQuery() const
{
    return m_private->searchQuery;
}

static KJS::ScriptInterpreter* interpreterForFrame(Frame* frame)
{
    ASSERT(frame);
    return frame->scriptProxy()->interpreter();
}

void WebInspector::showOptionsMenu()
{
    bool ignoreWhitespace;
    bool showUserAgentStyles;

    {
        KJS::JSLock lock;

        KJS::ScriptInterpreter* interpreter = interpreterForFrame(m_private->webView->page()->mainFrame());
        KJS::JSObject* windowObject = interpreter->globalObject();
        KJS::ExecState* globalExec = interpreter->globalExec();

        KJS::JSValue* ignoreWhitespaceValue = windowObject->get(globalExec, KJS::Identifier("ignoreWhitespace"));
        ASSERT(ignoreWhitespaceValue->isBoolean());
        ignoreWhitespace = ignoreWhitespaceValue->getBoolean();

        KJS::JSValue* showUserAgentStylesValue = windowObject->get(globalExec, KJS::Identifier("showUserAgentStyles"));
        ASSERT(ignoreWhitespaceValue->isBoolean());
        showUserAgentStyles = showUserAgentStylesValue->getBoolean();
    }

    HMENU popupMenu = ::CreatePopupMenu();
    if (!popupMenu)
        return;

    MENUITEMINFO info = {0};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
    info.fType = MFT_STRING;

    info.dwTypeData = _tcsdup(TEXT("Ignore Whitespace"));
    info.fState = ignoreWhitespace ? MFS_CHECKED : MFS_UNCHECKED;
    info.wID = IgnoreWhitespaceIdentifier;

    ::InsertMenuItem(popupMenu, (UINT)-1, true, &info);

    info.dwTypeData = _tcsdup(TEXT("Show User Agent Styles"));
    info.fState = showUserAgentStyles ? MFS_CHECKED : MFS_UNCHECKED;
    info.wID = ShowUserAgentStylesIdentifier;

    ::InsertMenuItem(popupMenu, (UINT)-1, true, &info);

    RECT windowRect;
    ::GetWindowRect(m_private->hWnd, &windowRect);

    // The Mac inspector shows the menu at the location of the mouse click. That's not terribly easy to get
    // to here, so we just show it to the right of the arrow in the button.
    ::TrackPopupMenu(popupMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NOANIMATION, windowRect.right + optionsMenuOffset.width(), windowRect.top + optionsMenuOffset.height(), 0, m_private->hWnd, 0);
}

LRESULT WebInspector::onCommand(WPARAM wParam, LPARAM)
{
    switch (LOWORD(wParam)) {
        case IgnoreWhitespaceIdentifier:
            toggleIgnoreWhitespace();
            break;
        case ShowUserAgentStylesIdentifier:
            toggleShowUserAgentStyles();
            break;
    }

    return 0;
}

void WebInspector::toggleIgnoreWhitespace()
{
    if (!m_private->webViewLoaded)
        return;

    // FIXME: We should use callWebScriptMethod instead of stringByEvaluatingJavaScriptFromString
    BSTR result;
    m_private->webView->stringByEvaluatingJavaScriptFromString(BString("toggleIgnoreWhitespace()"), &result);
    ::SysFreeString(result);
}

void WebInspector::toggleShowUserAgentStyles()
{
    if (!m_private->webViewLoaded)
        return;

    // FIXME: We should use callWebScriptMethod instead of stringByEvaluatingJavaScriptFromString
    BSTR result;
    m_private->webView->stringByEvaluatingJavaScriptFromString(BString("toggleShowUserAgentStyles()"), &result);
    ::SysFreeString(result);
}

void WebInspector::highlightNode(IDOMNode*)
{
    // FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
    if (_private->currentHighlight) {
        [_private->currentHighlight expire];
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }

    NSView *view = [[_private->webFrame frameView] documentView];
    NSRect bounds = NSIntersectionRect([node boundingBox], [view visibleRect]);
    if (!NSIsEmptyRect(bounds)) {
        NSArray *rects = nil;
        if ([node isKindOfClass:[DOMElement class]]) {
            DOMCSSStyleDeclaration *style = [_private->domDocument getComputedStyle:(DOMElement *)node pseudoElement:@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                rects = [node lineBoxRects];
        } else if ([node isKindOfClass:[DOMText class]]
#if ENABLE(SVG)
                   && ![[node parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")]
#endif
                  )
            rects = [node lineBoxRects];

        if (![rects count])
            rects = [NSArray arrayWithObject:[NSValue valueWithRect:bounds]];

        if ([view window]) // skip the highlight if we have no window (e.g. hidden tab)
            _private->currentHighlight = [[WebNodeHighlight alloc] initWithBounds:bounds andRects:rects forView:view];
    }
#endif
}

// FIXME: <rdar://problem/5119802> Web Inspector doesn't highlight the inspected node
#if 0
- (void)_nodeHighlightExpired:(NSNotification *)notification
{
    if (_private->currentHighlight == [notification object]) {
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }
}
#endif

void WebInspector::update()
{
    if (!m_private->webViewLoaded)
        return;

    // FIXME: We should use callWebScriptMethod instead of stringByEvaluatingJavaScriptFromString
    BSTR result;
    m_private->webView->stringByEvaluatingJavaScriptFromString(BString("updatePanes()"), &result);
    SysFreeString(result);
}

void WebInspector::updateRoot()
{
    if (!m_private->webViewLoaded)
        return;

    // FIXME: We should use callWebScriptMethod instead of stringByEvaluatingJavaScriptFromString
    BSTR result;
    m_private->webView->stringByEvaluatingJavaScriptFromString(BString("updateTreeOutline()"), &result);
    SysFreeString(result);
}

void WebInspector::updateSystemColors()
{
    if (!m_private->webViewLoaded)
        return;

    // FIXME: On the Mac we follow the system's color settings here, but it's not clear we want to do that
    // on Windows, since so much of WebCore doesn't obey the system's settings.
#if 0
    CGFloat red = 0.0f, green = 0.0f, blue = 0.0f;
    NSColor *color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    [color getRed:&red green:&green blue:&blue alpha:NULL];

    NSString *colorText = [NSString stringWithFormat:@"rgba(%d,%d,%d,0.4) !important", (int)(red * 255), (int)(green * 255), (int)(blue * 255)];
    NSString *styleText = [NSString stringWithFormat:@".focused .selected { background-color: %1$@ } .blured .selected { border-color: %1$@ }", colorText];
    DOMElement *style = [_private->domDocument getElementById:@"systemColors"];
    if (!style) {
        style = [_private->domDocument createElement:@"style"];
        [[[_private->domDocument getElementsByTagName:@"head"] item:0] appendChild:style];
    }

    [style setAttribute:@"id" value:@"systemColors"];
    [style setTextContent:styleText];
#endif
}

LRESULT WebInspector::onActivateApp(WPARAM wParam, LPARAM)
{
    if (!::IsWindowVisible(m_private->hWnd))
        return 0;

    ::SetWindowPos(m_private->hWnd, wParam ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return 0;
}

void WebInspector::webFrameDetached(WebFrame*)
{
    setFocusedDOMNode(0);
    setWebFrame(0);
    update();
    updateRoot();
}

void WebInspector::inspectedWebViewProgressFinished(IWebNotification* notification)
{
    ASSERT(notification);

    COMPtr<IUnknown> object;
    if (FAILED(notification->getObject(&object)))
        return;

    COMPtr<IWebView> notificationWebView;
    if (FAILED(object->QueryInterface(IID_IWebView, (void**)&notificationWebView)))
        return;

    COMPtr<IWebView> webView;
    if (FAILED(m_private->webFrame->webView(&webView)))
        return;

    if (notificationWebView != webView)
        return;

    COMPtr<IDOMDocument> iDOMDoc;
    if (SUCCEEDED(m_private->webFrame->DOMDocument(&iDOMDoc))) {
        COMPtr<IDOMNode> node;
        if (SUCCEEDED(iDOMDoc->QueryInterface(IID_IDOMNode, (void**)&node)))
            setFocusedDOMNode(node.get());
    }

    update();
    updateRoot();
    highlightNode(focusedDOMNode());
}

// FIXME: All of these callbacks only work with the sharedWebInspector, but should instead work on any arbitrary inspector.
// It should be possible to use the thisObject parameter to get back to the inspector.

static JSValueRef setFocusedDOMNodeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    WebInspector* inspector = WebInspector::sharedWebInspector();

    KJS::JSLock lock;

    Node* node = toNode(toJS(arguments[0]));
    if (!node) {
        inspector->setFocusedDOMNode(0);
        return undefined;
    }

    COMPtr<IDOMNode> domNode;
    domNode.adoptRef(DOMNode::createInstance(node));
    inspector->setFocusedDOMNode(domNode.get());
    return undefined;
}

static JSValueRef focusedDOMNodeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    WebInspector* inspector = WebInspector::sharedWebInspector();

    IDOMNode* iDOMNode = inspector->focusedDOMNode();
    if (!iDOMNode)
        return undefined;

    COMPtr<DOMNode> domNode;
    if (FAILED(iDOMNode->QueryInterface(IID_DOMNode, (void**)&domNode)))
        return undefined;

    Node* node = domNode->node();
    if (!node)
        return undefined;

    KJS::JSLock lock;

    return toRef(toJS(toJS(context), node));
}

static JSValueRef setRootDOMNodeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    if (argumentCount < 1)
        return undefined;

    WebInspector* inspector = WebInspector::sharedWebInspector();

    KJS::JSLock lock;

    Node* node = toNode(toJS(arguments[0]));
    if (!node) {
        inspector->setRootDOMNode(0);
        return undefined;
    }

    COMPtr<IDOMNode> domNode;
    domNode.adoptRef(DOMNode::createInstance(node));
    inspector->setRootDOMNode(domNode.get());
    return undefined;
}

static JSValueRef rootDOMNodeCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    WebInspector* inspector = WebInspector::sharedWebInspector();

    IDOMNode* iDOMNode = inspector->rootDOMNode();
    if (!iDOMNode)
        return undefined;

    COMPtr<DOMNode> domNode;
    if (FAILED(iDOMNode->QueryInterface(IID_DOMNode, (void**)&domNode)))
        return undefined;

    Node* node = domNode->node();
    if (!node)
        return undefined;

    KJS::JSLock lock;

    return toRef(toJS(toJS(context), node));
}

static JSValueRef showOptionsMenuCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    WebInspector::sharedWebInspector()->showOptionsMenu();
    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "setFocusedDOMNode", setFocusedDOMNodeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "focusedDOMNode", focusedDOMNodeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "setRootDOMNode", setRootDOMNodeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "rootDOMNode", rootDOMNodeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "showOptionsMenu", showOptionsMenuCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef) {
    static JSClassRef inspectorClass = 0;

    if (!inspectorClass) {
        JSClassDefinition classDefinition = kJSClassDefinitionEmpty;
        classDefinition.staticFunctions = staticFunctions;

        inspectorClass = JSClassCreate(&classDefinition);
    }

    return inspectorClass;
}

static JSObjectRef makeInspectorObject(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}

HRESULT STDMETHODCALLTYPE WebInspector::didFinishLoadForFrame( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebFrame* frame)
{
    // note: this is the Inspector's own WebView, not the one being inspected
    m_private->webViewLoaded = true;

    COMPtr<IWebFrame> mainFrame;
    HRESULT hr = webView->mainFrame(&mainFrame);
    if (FAILED(hr))
        return hr;

    if (mainFrame.get() != frame)
        return S_OK;

    COMPtr<IDOMDocument> document;
    hr = mainFrame->DOMDocument(&document);
    if (FAILED(hr))
        return hr;

    COMPtr<IDOMHTMLDocument> htmlDocument;
    hr = document->QueryInterface(IID_IDOMHTMLDocument, (void**)&htmlDocument);
    if (FAILED(hr))
        return hr;

    m_private->domDocument = htmlDocument;

    //[[self window] invalidateShadow];

    update();
    updateRoot();
    updateSystemColors();

    if (!searchQuery().isEmpty())
        setSearchQuery(searchQuery());

    highlightNode(focusedDOMNode());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebInspector::windowScriptObjectAvailable( 
    /* [in] */ IWebView*,
    /* [in] */ JSContextRef context,
    /* [in] */ JSObjectRef windowObject)
{
    JSStringRef inspectorStr = JSStringCreateWithUTF8CString("Inspector");
    JSValueRef inspectorObject = makeInspectorObject(context);
    JSObjectSetProperty(context, windowObject, inspectorStr, inspectorObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(inspectorStr);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebInspector::runJavaScriptAlertPanelWithMessage( 
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

// IUnknown ------------------------------
HRESULT STDMETHODCALLTYPE WebInspector::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationObserver))
        *ppvObject = static_cast<IWebNotificationObserver*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebInspector::AddRef()
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 2;
}

ULONG STDMETHODCALLTYPE WebInspector::Release()
{
    // COM ref-counting isn't useful to us because we're in charge of the lifetime of the WebView.
    return 2;
}

// IWebNotificationObserver -----------------------------
HRESULT STDMETHODCALLTYPE WebInspector::onNotify(IWebNotification* notification)
{
    ASSERT(notification);

    BSTR nameBSTR;
    HRESULT hr = notification->name(&nameBSTR);
    BString name;
    name.adoptBSTR(nameBSTR);
    if (FAILED(hr))
        return hr;

    if (!wcscmp(name, WebViewProgressFinishedNotification))
        inspectedWebViewProgressFinished(notification);

    return S_OK;
}

static ATOM registerWindowClass()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = 0;
    wcex.lpfnWndProc    = WebInspectorWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(WebInspector*);
    wcex.hInstance      = 0;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebInspectorWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return ::RegisterClassEx(&wcex);
}

LRESULT CALLBACK WebInspectorWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    WebInspector* inspector = reinterpret_cast<WebInspector*>(longPtr);

    switch (message) {
        case WM_SIZE:
            return inspector ? inspector->onSize(wParam, lParam) : 0;
        case WM_DESTROY:
            return inspector->onDestroy(wParam, lParam);
        case WM_ACTIVATEAPP:
            return inspector->onActivateApp(wParam, lParam);
        case WM_LBUTTONDOWN:
            return inspector->onLButtonDown(wParam, lParam);
        case WM_LBUTTONUP:
            return inspector->onLButtonUp(wParam, lParam);
        case WM_MOUSEMOVE:
            return inspector->onMouseMove(wParam, lParam);
        case WM_COMMAND:
            return inspector->onCommand(wParam, lParam);
    }

    return ::DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK SubclassedWebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WebInspector* inspector = reinterpret_cast<WebInspector*>(::GetProp(hWnd, kWebInspectorPointerProp));
    // We subclass the WebView after calling SetProp, so GetProp should never fail here.
    ASSERT(inspector);

    LRESULT lResult = inspector->handleMessageSentToWebView(message, wParam, lParam);
    if (lResult != kNotHandledResult)
        return lResult;

    return ::CallWindowProc(inspector->m_private->originalWebViewWndProc, hWnd, message, wParam, lParam);
}
