/*
 * Copyright (C) 2007 Kevin Ollivier  All rights reserved.
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
#include "WebView.h"

#include "ContextMenu.h"
#include "ContextMenuController.h"
#include "ContextMenuItem.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "EmptyClients.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "InitializeLogging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ResourceHandleManager.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "SubstituteData.h"
#include "Threading.h"
#include "markup.h"
#if __WXMSW__
#include "WebCoreInstanceHandle.h"
#endif

#include "ChromeClientWx.h"
#include "ContextMenuClientWx.h"
#include "DragClientWx.h"
#include "EditorClientWx.h"
#include "FrameLoaderClientWx.h"
#include "InspectorClientWx.h"

#include "ScriptController.h"
#include "JSDOMBinding.h"
#include <runtime/InitializeThreading.h>
#include <runtime/JSValue.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SQL_DATABASE)
#include "AbstractDatabase.h"
#include "DatabaseTracker.h"
#endif

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebDOMElement.h"
#include "WebDOMNode.h"

#include "WebFrame.h"
#include "WebViewPrivate.h"

#include <wx/defs.h>
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>

#if defined(_MSC_VER)
int rint(double val)
{
    return (int)(val < 0 ? val - 0.5 : val + 0.5);
}
#endif

#if OS(DARWIN)
// prototype - function is in WebSystemInterface.mm
void InitWebCoreSystemInterface(void);
#endif

// ----------------------------------------------------------------------------
// WebView Events
// ----------------------------------------------------------------------------

namespace WebKit {

IMPLEMENT_DYNAMIC_CLASS(WebViewLoadEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_LOAD)

WebViewLoadEvent::WebViewLoadEvent(wxWindow* win)
{
    SetEventType( wxEVT_WEBVIEW_LOAD);
    SetEventObject( win );
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewBeforeLoadEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_BEFORE_LOAD)

WebViewBeforeLoadEvent::WebViewBeforeLoadEvent(wxWindow* win)
{
    m_cancelled = false;
    SetEventType(wxEVT_WEBVIEW_BEFORE_LOAD);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewNewWindowEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_NEW_WINDOW)

WebViewNewWindowEvent::WebViewNewWindowEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_NEW_WINDOW);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewRightClickEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_RIGHT_CLICK)

WebViewRightClickEvent::WebViewRightClickEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_RIGHT_CLICK);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewConsoleMessageEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_CONSOLE_MESSAGE)

WebViewConsoleMessageEvent::WebViewConsoleMessageEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_CONSOLE_MESSAGE);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewAlertEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_ALERT)

WebViewAlertEvent::WebViewAlertEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_ALERT);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewConfirmEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_CONFIRM)

WebViewConfirmEvent::WebViewConfirmEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_CONFIRM);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewPromptEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_PROMPT)

WebViewPromptEvent::WebViewPromptEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_PROMPT);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewReceivedTitleEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_RECEIVED_TITLE)

WebViewReceivedTitleEvent::WebViewReceivedTitleEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_RECEIVED_TITLE);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewWindowObjectClearedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED)

WebViewWindowObjectClearedEvent::WebViewWindowObjectClearedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewContentsChangedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_CONTENTS_CHANGED)

WebViewContentsChangedEvent::WebViewContentsChangedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_CONTENTS_CHANGED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewSelectionChangedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_SELECTION_CHANGED)

WebViewSelectionChangedEvent::WebViewSelectionChangedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_SELECTION_CHANGED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(WebViewPrintFrameEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_PRINT_FRAME)

WebViewPrintFrameEvent::WebViewPrintFrameEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_PRINT_FRAME);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

//---------------------------------------------------------
// DOM Element info data type
//---------------------------------------------------------

WebViewDOMElementInfo::WebViewDOMElementInfo() :
    m_isSelected(false),
    m_text(wxEmptyString),
    m_imageSrc(wxEmptyString),
    m_link(wxEmptyString),
    m_urlElement(0),
    m_innerNode(0)
{
}

static WebViewCachePolicy gs_cachePolicy;

/* static */
void WebView::SetCachePolicy(const WebViewCachePolicy& cachePolicy)
{
    WebCore::MemoryCache* globalCache = WebCore::memoryCache();
    globalCache->setCapacities(cachePolicy.GetMinDeadCapacity(),
                               cachePolicy.GetMaxDeadCapacity(),
                               cachePolicy.GetCapacity());

    // store a copy since there is no getter for MemoryCache values
    gs_cachePolicy = cachePolicy;
}

/* static */
WebViewCachePolicy WebView::GetCachePolicy()
{
    return gs_cachePolicy;
}

WebViewDOMElementInfo::WebViewDOMElementInfo(const WebViewDOMElementInfo& other)
{
    m_isSelected = other.m_isSelected;
    m_text = other.m_text;
    m_imageSrc = other.m_imageSrc;
    m_link = other.m_link;
    m_innerNode = other.m_innerNode;
    m_urlElement = other.m_urlElement;
}

WebViewDOMElementInfo::~WebViewDOMElementInfo() 
{
    if (m_innerNode)
        delete m_innerNode;
        
    if (m_urlElement)
        delete m_urlElement;
}

BEGIN_EVENT_TABLE(WebView, wxWindow)
    EVT_PAINT(WebView::OnPaint)
    EVT_SIZE(WebView::OnSize)
    EVT_MOUSE_EVENTS(WebView::OnMouseEvents)
    EVT_CONTEXT_MENU(WebView::OnContextMenuEvents)
    EVT_KEY_DOWN(WebView::OnKeyEvents)
    EVT_KEY_UP(WebView::OnKeyEvents)
    EVT_CHAR(WebView::OnKeyEvents)
    EVT_SET_FOCUS(WebView::OnSetFocus)
    EVT_KILL_FOCUS(WebView::OnKillFocus)
    EVT_MOUSE_CAPTURE_LOST(WebView::OnMouseCaptureLost)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(WebView, wxWindow)

const wxChar* WebViewNameStr = wxT("webView");

WebView::WebView() :
    m_textMagnifier(1.0),
    m_isInitialized(false),
    m_beingDestroyed(false),
    m_mouseWheelZooms(false),
    m_title(wxEmptyString)
{
}

WebView::WebView(wxWindow* parent, const wxString& url, int id, const wxPoint& position, 
                     const wxSize& size, long style, const wxString& name) :
    m_textMagnifier(1.0),
    m_isInitialized(false),
    m_beingDestroyed(false),
    m_mouseWheelZooms(false),
    m_title(wxEmptyString)
{
    Create(parent, url, id, position, size, style, name);
}

bool WebView::Create(wxWindow* parent, const wxString& url, int id, const wxPoint& position, 
                       const wxSize& size, long style, const wxString& name)
{
#if OS(DARWIN)
    InitWebCoreSystemInterface();
#endif

    if ( (style & wxBORDER_MASK) == 0)
        style |= wxBORDER_NONE;
    
    if (!wxWindow::Create(parent, id, position, size, style, name))
        return false;

    JSC::initializeThreading();
    WTF::initializeMainThread();

// This is necessary because we are using SharedTimerWin.cpp on Windows,
// due to a problem with exceptions getting eaten when using the callback
// approach to timers (which wx itself uses).
#if __WXMSW__
    WebCore::setInstanceHandle(wxGetInstance());
#endif

    // this helps reduce flicker on platforms like MSW
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    m_impl = new WebViewPrivate();

#if !LOG_DISABLED
    WebCore::initializeLoggingChannelsIfNecessary();
#endif // !LOG_DISABLED
    WebCore::HTMLFrameOwnerElement* parentFrame = 0;

    WebCore::EditorClientWx* editorClient = new WebCore::EditorClientWx();

    WebCore::Page::PageClients pageClients;
    pageClients.chromeClient = new WebCore::ChromeClientWx(this);
    pageClients.contextMenuClient = new WebCore::ContextMenuClientWx();
    pageClients.editorClient = editorClient;
    pageClients.dragClient = new WebCore::DragClientWx();
    pageClients.inspectorClient = new WebCore::InspectorClientWx();
    m_impl->page = new WebCore::Page(pageClients);
    editorClient->setPage(m_impl->page);
    
    m_mainFrame = new WebFrame(this);

    // Default settings - we should have WebViewSettings class for this
    // eventually
    WebCore::Settings* settings = m_impl->page->settings();
    settings->setLoadsImagesAutomatically(true);
    settings->setDefaultFixedFontSize(13);
    settings->setDefaultFontSize(16);
    settings->setSerifFontFamily("Times New Roman");
    settings->setFixedFontFamily("Courier New");
    settings->setSansSerifFontFamily("Arial");
    settings->setStandardFontFamily("Times New Roman");
    settings->setScriptEnabled(true);

#if ENABLE(SQL_DATABASE)
    SetDatabasesEnabled(true);
#endif

    // we need to do this so that objects like the focusController are properly
    // initialized so that the activate handler is run properly.
    LoadURL(url);
    
    m_isInitialized = true;

    return true;
}

WebView::~WebView()
{
    m_beingDestroyed = true;
    
    while (HasCapture())
        ReleaseMouse();
    
    if (m_mainFrame && m_mainFrame->GetFrame())
        m_mainFrame->GetFrame()->loader()->detachFromParent();
    
    delete m_impl->page;
    m_impl->page = 0;   
}

// NOTE: binding to this event in the WebView constructor is too early in 
// some cases, but leave the event handler here so that users can bind to it
// at a later time if they have activation state problems.
void WebView::OnTLWActivated(wxActivateEvent& event)
{        
    if (m_impl && m_impl->page && m_impl->page->focusController())
        m_impl->page->focusController()->setActive(event.GetActive());
    
    event.Skip();
    
}

void WebView::Stop()
{
    if (m_mainFrame)
        m_mainFrame->Stop();
}

void WebView::Reload()
{
    if (m_mainFrame)
        m_mainFrame->Reload();
}

wxString WebView::GetPageSource()
{
    if (m_mainFrame)
        return m_mainFrame->GetPageSource();

    return wxEmptyString;
}

void WebView::SetPageSource(const wxString& source, const wxString& baseUrl, const wxString& mimetype)
{
    if (m_mainFrame)
        m_mainFrame->SetPageSource(source, baseUrl, mimetype);
}

wxString WebView::GetInnerText()
{
    if (m_mainFrame)
        return m_mainFrame->GetInnerText();
        
    return wxEmptyString;
}

wxString WebView::GetAsMarkup()
{
    if (m_mainFrame)
        return m_mainFrame->GetAsMarkup();
        
    return wxEmptyString;
}

wxString WebView::GetExternalRepresentation()
{
    if (m_mainFrame)
        return m_mainFrame->GetExternalRepresentation();
        
    return wxEmptyString;
}

WebKitSelection WebView::GetSelection()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelection();
        
    return 0;
}

wxString WebView::GetSelectionAsHTML()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelectionAsHTML();
        
    return wxEmptyString;
}

wxString WebView::GetSelectionAsText()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelectionAsText();
        
    return wxEmptyString;
}

void WebView::SetTransparent(bool transparent)
{
    WebCore::Frame* frame = 0;
    if (m_mainFrame)
        frame = m_mainFrame->GetFrame();
    
    if (!frame || !frame->view())
        return;

    frame->view()->setTransparent(transparent);
}

bool WebView::IsTransparent() const
{
    WebCore::Frame* frame = 0;
    if (m_mainFrame)
        frame = m_mainFrame->GetFrame();

   if (!frame || !frame->view())
        return false;

    return frame->view()->isTransparent();
}

wxString WebView::RunScript(const wxString& javascript)
{
    if (m_mainFrame)
        return m_mainFrame->RunScript(javascript);
    
    return wxEmptyString;
}

bool WebView::ExecuteEditCommand(const wxString& command, const wxString& parameter)
{
    if (m_mainFrame)
        return m_mainFrame->ExecuteEditCommand(command, parameter);
}

EditState WebView::GetEditCommandState(const wxString& command) const
{
    if (m_mainFrame)
        return m_mainFrame->GetEditCommandState(command);
}

wxString WebView::GetEditCommandValue(const wxString& command) const
{
    if (m_mainFrame)
        return m_mainFrame->GetEditCommandValue(command);
 
    return wxEmptyString;
}

void WebView::LoadURL(const wxString& url)
{
    if (m_mainFrame)
        m_mainFrame->LoadURL(url);
}

wxString WebView::GetMainFrameURL() const
{
    if (m_mainFrame)
        return m_mainFrame->GetURL();
    
    return wxEmptyString;
}

bool WebView::GoBack()
{
    if (m_mainFrame)
        return m_mainFrame->GoBack();

    return false;
}

bool WebView::GoForward()
{
    if (m_mainFrame)
        return m_mainFrame->GoForward();

    return false;
}

bool WebView::CanGoBack()
{
    if (m_mainFrame)
        return m_mainFrame->CanGoBack();

    return false;
}

bool WebView::CanGoForward()
{
    if (m_mainFrame)
        return m_mainFrame->CanGoForward();

    return false;
}

bool WebView::CanIncreaseTextSize() const
{
    if (m_mainFrame)
        return m_mainFrame->CanIncreaseTextSize();

    return false;
}

void WebView::IncreaseTextSize()
{
    if (m_mainFrame)
        m_mainFrame->IncreaseTextSize();
}

bool WebView::CanDecreaseTextSize() const
{
    if (m_mainFrame)
        return m_mainFrame->CanDecreaseTextSize();

    return false;
}

void WebView::DecreaseTextSize()
{        
    if (m_mainFrame)
        m_mainFrame->DecreaseTextSize();
}

void WebView::ResetTextSize()
{
    if (m_mainFrame)
        m_mainFrame->ResetTextSize();    
}

void WebView::MakeEditable(bool enable)
{
    if (m_mainFrame)
        m_mainFrame->MakeEditable(enable);
}

bool WebView::IsEditable() const
{
    if (m_mainFrame)
        return m_mainFrame->IsEditable();
    
    return false;
}



/* 
 * Event forwarding functions to send events down to WebCore.
 */

void WebView::OnPaint(wxPaintEvent& event)
{
    if (m_beingDestroyed || !m_mainFrame)
        return;

    WebCore::Frame* frame = m_mainFrame->GetFrame();
    if (!frame || !frame->view())
        return;

    // we can't use wxAutoBufferedPaintDC here because it will not create 
    // a 32-bit bitmap for its buffer.
#if __WXMSW__
    wxPaintDC paintdc(this);
    int width, height;
    paintdc.GetSize(&width, &height);
    wxBitmap bitmap(width, height, 32);
    wxMemoryDC dc(bitmap);
#else
    wxPaintDC dc(this);
#endif

    if (IsShown() && frame->document()) {
#if USE(WXGC)
#if wxCHECK_VERSION(2, 9, 2) && defined(wxUSE_CAIRO) && wxUSE_CAIRO
        wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetCairoRenderer();
        if (!renderer)
            renderer = wxGraphicsRenderer::GetDefaultRenderer();
        wxGraphicsContext* context = renderer->CreateContext(dc);
        wxGCDC gcdc(context);
#else
        wxGCDC gcdc(dc);
#endif
#endif

        if (dc.IsOk()) {
            wxRect paintRect = GetUpdateRegion().GetBox();

#if USE(WXGC)
            WebCore::GraphicsContext gc(&gcdc);
#else
            WebCore::GraphicsContext gc(&dc);
#endif
            if (frame->contentRenderer()) {
                frame->view()->updateLayoutAndStyleIfNeededRecursive();
                frame->view()->paint(&gc, paintRect);
#if __WXMSW__
                dc.SelectObject(wxNullBitmap);
                paintdc.DrawBitmap(bitmap, 0, 0);
#endif
            }
        }
    }
}

bool WebView::FindString(const wxString& string, bool forward, bool caseSensitive, bool wrapSelection, bool startInSelection)
{
    if (m_mainFrame)
        return m_mainFrame->FindString(string, forward, caseSensitive, wrapSelection, startInSelection);

    return false;
}

void WebView::OnSize(wxSizeEvent& event)
{ 
    if (m_isInitialized && m_mainFrame) {
        WebCore::Frame* frame = m_mainFrame->GetFrame();
        frame->view()->resize(event.GetSize());
        frame->view()->adjustViewSize();
    }
      
    event.Skip();
}

static int getDoubleClickTime()
{
#if __WXMSW__
    return ::GetDoubleClickTime();
#else
    return 500;
#endif
}

void WebView::OnMouseEvents(wxMouseEvent& event)
{
    event.Skip();
    
    if (!m_impl->page)
        return; 
        
    WebCore::Frame* frame = m_mainFrame->GetFrame();  
    if (!frame || !frame->view())
        return;
    
    wxPoint globalPoint = ClientToScreen(event.GetPosition());

    wxEventType type = event.GetEventType();
    
    if (type == wxEVT_MOUSEWHEEL) {
        if (m_mouseWheelZooms && event.ControlDown() && !event.AltDown() && !event.ShiftDown()) {
            if (event.GetWheelRotation() < 0)
                DecreaseTextSize();
            else if (event.GetWheelRotation() > 0)
                IncreaseTextSize();
        } else {
            WebCore::PlatformWheelEvent wkEvent(event, globalPoint);
            frame->eventHandler()->handleWheelEvent(wkEvent);
        }

        return;
    }
    
    // If an event, such as a right-click event, leads to a focus change (e.g. it 
    // raises a dialog), WebKit never gets the mouse up event and never relinquishes 
    // mouse capture. This leads to WebKit handling mouse events, such as modifying
    // the selection, while other controls or top level windows have the focus.
    // I'm not sure if this is the right place to handle this, but I can't seem to
    // find a precedent on how to handle this in other ports.
    if (wxWindow::FindFocus() != this) {
        while (HasCapture())
            ReleaseMouse();

        frame->eventHandler()->setMousePressed(false);

        return;
    }
        
    int clickCount = event.ButtonDClick() ? 2 : 1;

    if (clickCount == 1 && m_impl->tripleClickTimer.IsRunning()) {
        wxPoint diff(event.GetPosition() - m_impl->tripleClickPos);
        if (abs(diff.x) <= wxSystemSettings::GetMetric(wxSYS_DCLICK_X) &&
            abs(diff.y) <= wxSystemSettings::GetMetric(wxSYS_DCLICK_Y)) {
            clickCount = 3;
        }
    } else if (clickCount == 2) {
        m_impl->tripleClickTimer.Start(getDoubleClickTime(), false);
        m_impl->tripleClickPos = event.GetPosition();
    }
    
    WebCore::PlatformMouseEvent wkEvent(event, globalPoint, clickCount);

    if (type == wxEVT_LEFT_DOWN || type == wxEVT_MIDDLE_DOWN || type == wxEVT_RIGHT_DOWN || 
                type == wxEVT_LEFT_DCLICK || type == wxEVT_MIDDLE_DCLICK || type == wxEVT_RIGHT_DCLICK) {
        frame->eventHandler()->handleMousePressEvent(wkEvent);
        if (!HasCapture())
            CaptureMouse();
    } else if (type == wxEVT_LEFT_UP || type == wxEVT_MIDDLE_UP || type == wxEVT_RIGHT_UP) {
        frame->eventHandler()->handleMouseReleaseEvent(wkEvent);
        while (HasCapture())
            ReleaseMouse();
    } else if (type == wxEVT_MOTION || type == wxEVT_ENTER_WINDOW || type == wxEVT_LEAVE_WINDOW)
        frame->eventHandler()->mouseMoved(wkEvent);
}

void WebView::OnContextMenuEvents(wxContextMenuEvent& event)
{
    Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WebView::OnMenuSelectEvents), 0, this);
    m_impl->page->contextMenuController()->clearContextMenu();
    wxPoint localEventPoint = ScreenToClient(event.GetPosition());

    if (!m_impl->page)
        return;
        
    WebCore::Frame* focusedFrame = m_impl->page->focusController()->focusedOrMainFrame();
    if (!focusedFrame->view())
        return;

    //Create WebCore mouse event from the wxContextMenuEvent
    wxMouseEvent mouseEvent(wxEVT_RIGHT_DOWN);
    mouseEvent.m_x = localEventPoint.x;
    mouseEvent.m_y = localEventPoint.y;
    WebCore::PlatformMouseEvent wkEvent(mouseEvent, event.GetPosition(), 1);

    bool handledEvent = focusedFrame->eventHandler()->sendContextMenuEvent(wkEvent);
    if (!handledEvent)
        return;

    WebCore::ContextMenu* coreMenu = m_impl->page->contextMenuController()->contextMenu();
    if (!coreMenu)
        return;

    WebCore::PlatformMenuDescription menuWx = coreMenu->platformDescription();
    if (!menuWx)
        return;

    PopupMenu(menuWx, localEventPoint);
    
    Disconnect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(WebView::OnMenuSelectEvents), 0, this);
}

void WebView::OnMenuSelectEvents(wxCommandEvent& event)
{
    // we shouldn't hit this unless there's a context menu showing
    WebCore::ContextMenu* coreMenu = m_impl->page->contextMenuController()->contextMenu();
    ASSERT(coreMenu);
    if (!coreMenu)
        return;

    WebCore::ContextMenuItem* item = WebCore::ContextMenu::itemWithId (event.GetId());
    if (!item)
        return;

    m_impl->page->contextMenuController()->contextMenuItemSelected(item);
    delete item;
}

void WebView::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
    // do nothing - unfortunately, we MUST handle this event due to wxWidgets rules,
    // otherwise we will assert, even though there is nothing for us to do here.
}

bool WebView::CanCopy()
{
    if (m_mainFrame)
        return m_mainFrame->CanCopy();

    return false;
}

void WebView::Copy()
{
    if (m_mainFrame)
        m_mainFrame->Copy();
}

bool WebView::CanCut()
{
    if (m_mainFrame)
        return m_mainFrame->CanCut();

    return false;
}

void WebView::Cut()
{
    if (m_mainFrame)
        m_mainFrame->Cut();
}

bool WebView::CanPaste()
{
    if (m_mainFrame)
        return m_mainFrame->CanPaste();

    return false;
}

void WebView::Paste()
{
    if (m_mainFrame)
        m_mainFrame->Paste();
}

void WebView::OnKeyEvents(wxKeyEvent& event)
{
    WebCore::Frame* frame = 0;
    if (m_impl->page)
        frame = m_impl->page->focusController()->focusedOrMainFrame();

    if (!(frame && frame->view()))
        return;

    WebCore::PlatformKeyboardEvent wkEvent(event);

    if (frame->eventHandler()->keyEvent(wkEvent))
        return;

    //Some things WebKit won't do for us... Copy/Cut/Paste and KB scrolling
    if (event.GetEventType() == wxEVT_KEY_DOWN) {
        switch (event.GetKeyCode()) {
        case 67: //"C"
            if (CanCopy() && event.GetModifiers() == wxMOD_CMD) {
                Copy();
                return;
            }
            break;
        case 86: //"V"
            if (CanPaste() && event.GetModifiers() == wxMOD_CMD) {
                Paste();
                return;
            }
            break;
        case 88: //"X"
            if (CanCut() && event.GetModifiers() == wxMOD_CMD) {
                Cut();
                return;
            }
            break;
        case WXK_INSERT:
            if (CanCopy() && event.GetModifiers() == wxMOD_CMD) {
                Copy();
                return;
            }
            if (CanPaste() && event.GetModifiers() == wxMOD_SHIFT) {
                Paste();
                return;
            }
            return; //Insert shall not become a char
        case WXK_DELETE:
            if (CanCut() && event.GetModifiers() == wxMOD_SHIFT) {
                Cut();
                return;
            }
            break;
        case WXK_LEFT:
        case WXK_NUMPAD_LEFT:
            frame->view()->scrollBy(WebCore::IntSize(-WebCore::Scrollbar::pixelsPerLineStep(), 0));
            return;
        case WXK_UP:
        case WXK_NUMPAD_UP:
            frame->view()->scrollBy(WebCore::IntSize(0, -WebCore::Scrollbar::pixelsPerLineStep()));
            return;
        case WXK_RIGHT:
        case WXK_NUMPAD_RIGHT:
            frame->view()->scrollBy(WebCore::IntSize(WebCore::Scrollbar::pixelsPerLineStep(), 0));
            return;
        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
            frame->view()->scrollBy(WebCore::IntSize(0, WebCore::Scrollbar::pixelsPerLineStep()));
            return;
        case WXK_END:
        case WXK_NUMPAD_END:
            frame->view()->setScrollPosition(WebCore::IntPoint(frame->view()->scrollX(), frame->view()->maximumScrollPosition().y()));
            return;
        case WXK_HOME:
        case WXK_NUMPAD_HOME:
            frame->view()->setScrollPosition(WebCore::IntPoint(frame->view()->scrollX(), 0));
            return;
        case WXK_PAGEUP:
        case WXK_NUMPAD_PAGEUP:
            frame->view()->scrollBy(WebCore::IntSize(0, -frame->view()->visibleHeight() * WebCore::Scrollbar::minFractionToStepWhenPaging()));
            return;
        case WXK_PAGEDOWN:
        case WXK_NUMPAD_PAGEDOWN:
            frame->view()->scrollBy(WebCore::IntSize(0, frame->view()->visibleHeight() * WebCore::Scrollbar::minFractionToStepWhenPaging()));
            return;
        //These we don't want turning into char events, stuff 'em
        case WXK_ESCAPE:
        case WXK_LBUTTON:
        case WXK_RBUTTON:
        case WXK_CANCEL:
        case WXK_MENU:
        case WXK_MBUTTON:
        case WXK_CLEAR:
        case WXK_PAUSE:
        case WXK_SELECT:
        case WXK_PRINT:
        case WXK_EXECUTE:
        case WXK_SNAPSHOT:
        case WXK_HELP:
        case WXK_F1:
        case WXK_F2:
        case WXK_F3:
        case WXK_F4:
        case WXK_F5:
        case WXK_F6:
        case WXK_F7:
        case WXK_F8:
        case WXK_F9:
        case WXK_F10:
        case WXK_F11:
        case WXK_F12:
        case WXK_F13:
        case WXK_F14:
        case WXK_F15:
        case WXK_F16:
        case WXK_F17:
        case WXK_F18:
        case WXK_F19:
        case WXK_F20:
        case WXK_F21:
        case WXK_F22:
        case WXK_F23:
        case WXK_F24:
        case WXK_NUMPAD_F1:
        case WXK_NUMPAD_F2:
        case WXK_NUMPAD_F3:
        case WXK_NUMPAD_F4:
        //When numlock is off Numpad 5 becomes BEGIN, or HOME on Char
        case WXK_NUMPAD_BEGIN:
        case WXK_NUMPAD_INSERT:
            return;
        }
    }

    event.Skip();
}

void WebView::OnSetFocus(wxFocusEvent& event)
{
    if (m_impl && m_impl->page && m_impl->page->focusController()) {
        m_impl->page->focusController()->setFocused(true);
        m_impl->page->focusController()->setActive(true);

        if (!m_impl->page->focusController()->focusedFrame() && m_mainFrame)
            m_impl->page->focusController()->setFocusedFrame(m_mainFrame->GetFrame());
    }
    
    event.Skip();
}

void WebView::OnKillFocus(wxFocusEvent& event)
{
    if (m_impl && m_impl->page && m_impl->page->focusController()) {
        m_impl->page->focusController()->setFocused(false);

        // We also handle active state in OnTLWActivated, but if a user does not
        // call event.Skip() in their own EVT_ACTIVATE handler, we won't get those
        // callbacks. So we handle active state here as well as a fallback.
        wxTopLevelWindow* tlw = dynamic_cast<wxTopLevelWindow*>(wxGetTopLevelParent(this));
        if (tlw && tlw->IsActive())
            m_impl->page->focusController()->setActive(true);
        else
            m_impl->page->focusController()->setActive(false);
    }
    
    while (HasCapture())
        ReleaseMouse();
    
    event.Skip();
}

WebViewDOMElementInfo WebView::HitTest(const wxPoint& pos) const
{
    if (m_mainFrame)
        return m_mainFrame->HitTest(pos);

    return WebViewDOMElementInfo();
}

bool WebView::ShouldClose() const
{
    if (m_mainFrame)
        return m_mainFrame->ShouldClose();

    return true;
}

/* static */
void WebView::SetDatabaseDirectory(const wxString& databaseDirectory)
{
#if ENABLE(SQL_DATABASE)
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(databaseDirectory);
#endif
}

/* static */
wxString WebView::GetDatabaseDirectory()
{
#if ENABLE(SQL_DATABASE)
    return WebCore::DatabaseTracker::tracker().databaseDirectoryPath();
#else
    return wxEmptyString;
#endif
}

/* static */
void WebView::SetDatabasesEnabled(bool enabled)
{
#if ENABLE(SQL_DATABASE)
    WebCore::AbstractDatabase::setIsAvailable(enabled);
#endif
}

/* static */
bool WebView::AreDatabasesEnabled()
{
#if ENABLE(SQL_DATABASE)
    return WebCore::AbstractDatabase::isAvailable();
#endif
    return false;
}

static WebCore::ResourceHandleManager::ProxyType curlProxyType(wxProxyType type)
{
    switch (type) {
        case HTTP: return WebCore::ResourceHandleManager::HTTP;
        case Socks4: return WebCore::ResourceHandleManager::Socks4;
        case Socks4A: return WebCore::ResourceHandleManager::Socks4A;
        case Socks5: return WebCore::ResourceHandleManager::Socks5;
        case Socks5Hostname: return WebCore::ResourceHandleManager::Socks5Hostname;
        default:
            ASSERT_NOT_REACHED();
            return WebCore::ResourceHandleManager::HTTP;
    }
}

/* static */
void WebView::SetProxyInfo(const wxString& host,
                             unsigned long port,
                             wxProxyType type,
                             const wxString& username,
                             const wxString& password)
{
    using WebCore::ResourceHandleManager;
    if (ResourceHandleManager* mgr = ResourceHandleManager::sharedInstance())
        mgr->setProxyInfo(host, port, curlProxyType(type), username, password);
}

WebSettings WebView::GetWebSettings()
{
    ASSERT(m_impl->page);
    if (m_impl->page)
        return WebSettings(m_impl->page->settings());
    
    return WebSettings();
}

WebKitCompatibilityMode WebView::GetCompatibilityMode() const
{
    if (m_mainFrame)
        return m_mainFrame->GetCompatibilityMode();

    return QuirksMode;
}

void WebView::GrantUniversalAccess()
{
    if (m_mainFrame)
        m_mainFrame->GrantUniversalAccess();
}

}
