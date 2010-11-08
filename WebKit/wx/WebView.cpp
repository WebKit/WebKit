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
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "PlatformWheelEvent.h"
#include "PluginHalterClient.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ResourceHandleManager.h"
#include "Scrollbar.h"
#include "SelectionController.h"
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
#include <runtime/JSValue.h>
#include <runtime/UString.h>
#include <wtf/text/CString.h>

#if ENABLE(DATABASE)
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

// ----------------------------------------------------------------------------
// wxWebView Events
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxWebViewLoadEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_LOAD)

wxWebViewLoadEvent::wxWebViewLoadEvent(wxWindow* win)
{
    SetEventType( wxEVT_WEBVIEW_LOAD);
    SetEventObject( win );
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewBeforeLoadEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_BEFORE_LOAD)

wxWebViewBeforeLoadEvent::wxWebViewBeforeLoadEvent(wxWindow* win)
{
    m_cancelled = false;
    SetEventType(wxEVT_WEBVIEW_BEFORE_LOAD);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewNewWindowEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_NEW_WINDOW)

wxWebViewNewWindowEvent::wxWebViewNewWindowEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_NEW_WINDOW);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewRightClickEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_RIGHT_CLICK)

wxWebViewRightClickEvent::wxWebViewRightClickEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_RIGHT_CLICK);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewConsoleMessageEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_CONSOLE_MESSAGE)

wxWebViewConsoleMessageEvent::wxWebViewConsoleMessageEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_CONSOLE_MESSAGE);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewAlertEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_ALERT)

wxWebViewAlertEvent::wxWebViewAlertEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_ALERT);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewConfirmEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_CONFIRM)

wxWebViewConfirmEvent::wxWebViewConfirmEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_CONFIRM);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewPromptEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_JS_PROMPT)

wxWebViewPromptEvent::wxWebViewPromptEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_JS_PROMPT);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewReceivedTitleEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_RECEIVED_TITLE)

wxWebViewReceivedTitleEvent::wxWebViewReceivedTitleEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_RECEIVED_TITLE);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewWindowObjectClearedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED)

wxWebViewWindowObjectClearedEvent::wxWebViewWindowObjectClearedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewContentsChangedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_CONTENTS_CHANGED)

wxWebViewContentsChangedEvent::wxWebViewContentsChangedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_CONTENTS_CHANGED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewSelectionChangedEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_SELECTION_CHANGED)

wxWebViewSelectionChangedEvent::wxWebViewSelectionChangedEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_SELECTION_CHANGED);
    SetEventObject(win);
    if (win)
        SetId(win->GetId());
}

//---------------------------------------------------------
// DOM Element info data type
//---------------------------------------------------------

wxWebViewDOMElementInfo::wxWebViewDOMElementInfo() :
    m_isSelected(false),
    m_text(wxEmptyString),
    m_imageSrc(wxEmptyString),
    m_link(wxEmptyString),
    m_urlElement(NULL),
    m_innerNode(NULL)
{
}

static wxWebViewCachePolicy gs_cachePolicy;

/* static */
void wxWebView::SetCachePolicy(const wxWebViewCachePolicy& cachePolicy)
{
    WebCore::MemoryCache* globalCache = WebCore::cache();
    globalCache->setCapacities(cachePolicy.GetMinDeadCapacity(),
                               cachePolicy.GetMaxDeadCapacity(),
                               cachePolicy.GetCapacity());

    // store a copy since there is no getter for MemoryCache values
    gs_cachePolicy = cachePolicy;
}

/* static */
wxWebViewCachePolicy wxWebView::GetCachePolicy()
{
    return gs_cachePolicy;
}

wxWebViewDOMElementInfo::wxWebViewDOMElementInfo(const wxWebViewDOMElementInfo& other)
{
    m_isSelected = other.m_isSelected;
    m_text = other.m_text;
    m_imageSrc = other.m_imageSrc;
    m_link = other.m_link;
    m_innerNode = other.m_innerNode;
    m_urlElement = other.m_urlElement;
}

wxWebViewDOMElementInfo::~wxWebViewDOMElementInfo() 
{
    if (m_innerNode)
        delete m_innerNode;
        
    if (m_urlElement)
        delete m_urlElement;
}

#if OS(DARWIN)
// prototype - function is in WebSystemInterface.mm
void InitWebCoreSystemInterface(void);
#endif

BEGIN_EVENT_TABLE(wxWebView, wxWindow)
    EVT_PAINT(wxWebView::OnPaint)
    EVT_SIZE(wxWebView::OnSize)
    EVT_MOUSE_EVENTS(wxWebView::OnMouseEvents)
    EVT_CONTEXT_MENU(wxWebView::OnContextMenuEvents)
    EVT_KEY_DOWN(wxWebView::OnKeyEvents)
    EVT_KEY_UP(wxWebView::OnKeyEvents)
    EVT_CHAR(wxWebView::OnKeyEvents)
    EVT_SET_FOCUS(wxWebView::OnSetFocus)
    EVT_KILL_FOCUS(wxWebView::OnKillFocus)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(wxWebView, wxWindow)

const wxChar* wxWebViewNameStr = wxT("webView");

wxWebView::wxWebView() :
    m_textMagnifier(1.0),
    m_isEditable(false),
    m_isInitialized(false),
    m_beingDestroyed(false),
    m_mouseWheelZooms(false),
    m_title(wxEmptyString)
{
}

wxWebView::wxWebView(wxWindow* parent, int id, const wxPoint& position, 
                     const wxSize& size, long style, const wxString& name) :
    m_textMagnifier(1.0),
    m_isEditable(false),
    m_isInitialized(false),
    m_beingDestroyed(false),
    m_mouseWheelZooms(false),
    m_title(wxEmptyString)
{
    Create(parent, id, position, size, style, name);
}

bool wxWebView::Create(wxWindow* parent, int id, const wxPoint& position, 
                       const wxSize& size, long style, const wxString& name)
{
#if OS(DARWIN)
    InitWebCoreSystemInterface();
#endif

    if ( (style & wxBORDER_MASK) == 0)
        style |= wxBORDER_NONE;
    
    if (!wxWindow::Create(parent, id, position, size, style, name))
        return false;

    WTF::initializeThreading();
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

    WebCore::InitializeLoggingChannelsIfNecessary();    
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
    
    m_mainFrame = new wxWebFrame(this);

    // Default settings - we should have wxWebViewSettings class for this
    // eventually
    WebCore::Settings* settings = m_impl->page->settings();
    settings->setLoadsImagesAutomatically(true);
    settings->setDefaultFixedFontSize(13);
    settings->setDefaultFontSize(16);
    settings->setSerifFontFamily("Times New Roman");
    settings->setFixedFontFamily("Courier New");
    settings->setSansSerifFontFamily("Arial");
    settings->setStandardFontFamily("Times New Roman");
    settings->setJavaScriptEnabled(true);

#if ENABLE(DATABASE)
    SetDatabasesEnabled(true);
#endif

    // we need to do this so that objects like the focusController are properly
    // initialized so that the activate handler is run properly.
    LoadURL(wxT("about:blank"));
    
    m_isInitialized = true;

    return true;
}

wxWebView::~wxWebView()
{
    m_beingDestroyed = true;
    
    while (HasCapture())
        ReleaseMouse();
    
    if (m_mainFrame && m_mainFrame->GetFrame())
        m_mainFrame->GetFrame()->loader()->detachFromParent();
    
    delete m_impl->page;
    m_impl->page = 0;   
}

// NOTE: binding to this event in the wxWebView constructor is too early in 
// some cases, but leave the event handler here so that users can bind to it
// at a later time if they have activation state problems.
void wxWebView::OnTLWActivated(wxActivateEvent& event)
{        
    if (m_impl && m_impl->page && m_impl->page->focusController())
        m_impl->page->focusController()->setActive(event.GetActive());
    
    event.Skip();
    
}

void wxWebView::Stop()
{
    if (m_mainFrame)
        m_mainFrame->Stop();
}

void wxWebView::Reload()
{
    if (m_mainFrame)
        m_mainFrame->Reload();
}

wxString wxWebView::GetPageSource()
{
    if (m_mainFrame)
        return m_mainFrame->GetPageSource();

    return wxEmptyString;
}

void wxWebView::SetPageSource(const wxString& source, const wxString& baseUrl, const wxString& mimetype)
{
    if (m_mainFrame)
        m_mainFrame->SetPageSource(source, baseUrl, mimetype);
}

wxString wxWebView::GetInnerText()
{
    if (m_mainFrame)
        return m_mainFrame->GetInnerText();
        
    return wxEmptyString;
}

wxString wxWebView::GetAsMarkup()
{
    if (m_mainFrame)
        return m_mainFrame->GetAsMarkup();
        
    return wxEmptyString;
}

wxString wxWebView::GetExternalRepresentation()
{
    if (m_mainFrame)
        return m_mainFrame->GetExternalRepresentation();
        
    return wxEmptyString;
}

wxWebKitSelection wxWebView::GetSelection()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelection();
        
    return 0;
}

wxString wxWebView::GetSelectionAsHTML()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelectionAsHTML();
        
    return wxEmptyString;
}

wxString wxWebView::GetSelectionAsText()
{
    if (m_mainFrame)
        return m_mainFrame->GetSelectionAsText();
        
    return wxEmptyString;
}

void wxWebView::SetTransparent(bool transparent)
{
    WebCore::Frame* frame = 0;
    if (m_mainFrame)
        frame = m_mainFrame->GetFrame();
    
    if (!frame || !frame->view())
        return;

    frame->view()->setTransparent(transparent);
}

bool wxWebView::IsTransparent() const
{
    WebCore::Frame* frame = 0;
    if (m_mainFrame)
        frame = m_mainFrame->GetFrame();

   if (!frame || !frame->view())
        return false;

    return frame->view()->isTransparent();
}

wxString wxWebView::RunScript(const wxString& javascript)
{
    if (m_mainFrame)
        return m_mainFrame->RunScript(javascript);
    
    return wxEmptyString;
}

bool wxWebView::ExecuteEditCommand(const wxString& command, const wxString& parameter)
{
    if (m_mainFrame)
        return m_mainFrame->ExecuteEditCommand(command, parameter);
}

EditState wxWebView::GetEditCommandState(const wxString& command) const
{
    if (m_mainFrame)
        return m_mainFrame->GetEditCommandState(command);
}

wxString wxWebView::GetEditCommandValue(const wxString& command) const
{
    if (m_mainFrame)
        return m_mainFrame->GetEditCommandValue(command);
 
    return wxEmptyString;
}

void wxWebView::LoadURL(const wxString& url)
{
    if (m_mainFrame)
        m_mainFrame->LoadURL(url);
}

bool wxWebView::GoBack()
{
    if (m_mainFrame)
        return m_mainFrame->GoBack();

    return false;
}

bool wxWebView::GoForward()
{
    if (m_mainFrame)
        return m_mainFrame->GoForward();

    return false;
}

bool wxWebView::CanGoBack()
{
    if (m_mainFrame)
        return m_mainFrame->CanGoBack();

    return false;
}

bool wxWebView::CanGoForward()
{
    if (m_mainFrame)
        return m_mainFrame->CanGoForward();

    return false;
}

bool wxWebView::CanIncreaseTextSize() const
{
    if (m_mainFrame)
        return m_mainFrame->CanIncreaseTextSize();

    return false;
}

void wxWebView::IncreaseTextSize()
{
    if (m_mainFrame)
        m_mainFrame->IncreaseTextSize();
}

bool wxWebView::CanDecreaseTextSize() const
{
    if (m_mainFrame)
        m_mainFrame->CanDecreaseTextSize();

    return false;
}

void wxWebView::DecreaseTextSize()
{        
    if (m_mainFrame)
        m_mainFrame->DecreaseTextSize();
}

void wxWebView::ResetTextSize()
{
    if (m_mainFrame)
        m_mainFrame->ResetTextSize();    
}

void wxWebView::MakeEditable(bool enable)
{
    if (m_mainFrame)
        m_mainFrame->MakeEditable(enable);
}

bool wxWebView::IsEditable() const
{
    if (m_mainFrame)
        return m_mainFrame->IsEditable();
    
    return false;
}



/* 
 * Event forwarding functions to send events down to WebCore.
 */

void wxWebView::OnPaint(wxPaintEvent& event)
{
    if (m_beingDestroyed || !m_mainFrame)
        return;

    WebCore::Frame* frame = m_mainFrame->GetFrame();
    if (!frame || !frame->view())
        return;
    
    wxAutoBufferedPaintDC dc(this);

    if (IsShown() && frame->document()) {
#if USE(WXGC)
        wxGCDC gcdc(dc);
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
            }
        }
    }
}

bool wxWebView::FindString(const wxString& string, bool forward, bool caseSensitive, bool wrapSelection, bool startInSelection)
{
    if (m_mainFrame)
        return m_mainFrame->FindString(string, forward, caseSensitive, wrapSelection, startInSelection);

    return false;
}

void wxWebView::OnSize(wxSizeEvent& event)
{ 
    if (m_isInitialized && m_mainFrame) {
        WebCore::Frame* frame = m_mainFrame->GetFrame();
        frame->view()->setFrameRect(wxRect(wxPoint(0,0), event.GetSize()));
        frame->view()->forceLayout();
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

void wxWebView::OnMouseEvents(wxMouseEvent& event)
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

void wxWebView::OnContextMenuEvents(wxContextMenuEvent& event)
{
    Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxWebView::OnMenuSelectEvents), NULL, this);
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
    
    Disconnect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxWebView::OnMenuSelectEvents), NULL, this);
}

void wxWebView::OnMenuSelectEvents(wxCommandEvent& event)
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

bool wxWebView::CanCopy()
{
    if (m_mainFrame)
        return m_mainFrame->CanCopy();

    return false;
}

void wxWebView::Copy()
{
    if (m_mainFrame)
        m_mainFrame->Copy();
}

bool wxWebView::CanCut()
{
    if (m_mainFrame)
        return m_mainFrame->CanCut();

    return false;
}

void wxWebView::Cut()
{
    if (m_mainFrame)
        m_mainFrame->Cut();
}

bool wxWebView::CanPaste()
{
    if (m_mainFrame)
        return m_mainFrame->CanPaste();

    return false;
}

void wxWebView::Paste()
{
    if (m_mainFrame)
        m_mainFrame->Paste();
}

void wxWebView::OnKeyEvents(wxKeyEvent& event)
{
    WebCore::Frame* frame = 0;
    if (m_impl->page)
        frame = m_impl->page->focusController()->focusedOrMainFrame();

    if (!(frame && frame->view()))
        return;

    if (event.GetKeyCode() == WXK_CAPITAL)
        frame->eventHandler()->capsLockStateMayHaveChanged();

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

void wxWebView::OnSetFocus(wxFocusEvent& event)
{
    if (m_impl && m_impl->page && m_impl->page->focusController()) {
        m_impl->page->focusController()->setFocused(true);
        m_impl->page->focusController()->setActive(true);

        if (!m_impl->page->focusController()->focusedFrame() && m_mainFrame)
            m_impl->page->focusController()->setFocusedFrame(m_mainFrame->GetFrame());
    }
    
    event.Skip();
}

void wxWebView::OnKillFocus(wxFocusEvent& event)
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

wxWebViewDOMElementInfo wxWebView::HitTest(const wxPoint& pos) const
{
    if (m_mainFrame)
        return m_mainFrame->HitTest(pos);

    return wxWebViewDOMElementInfo();
}

bool wxWebView::ShouldClose() const
{
    if (m_mainFrame)
        return m_mainFrame->ShouldClose();

    return true;
}

/* static */
void wxWebView::SetDatabaseDirectory(const wxString& databaseDirectory)
{
#if ENABLE(DATABASE)
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(databaseDirectory);
#endif
}

/* static */
wxString wxWebView::GetDatabaseDirectory()
{
#if ENABLE(DATABASE)
    return WebCore::DatabaseTracker::tracker().databaseDirectoryPath();
#else
    return wxEmptyString;
#endif
}

/* static */
void wxWebView::SetDatabasesEnabled(bool enabled)
{
#if ENABLE(DATABASE)
    WebCore::AbstractDatabase::setIsAvailable(enabled);
#endif
}

/* static */
bool wxWebView::AreDatabasesEnabled()
{
#if ENABLE(DATABASE)
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
void wxWebView::SetProxyInfo(const wxString& host,
                             unsigned long port,
                             wxProxyType type,
                             const wxString& username,
                             const wxString& password)
{
    using WebCore::ResourceHandleManager;
    if (ResourceHandleManager* mgr = ResourceHandleManager::sharedInstance())
        mgr->setProxyInfo(host, port, curlProxyType(type), username, password);
}

wxWebSettings wxWebView::GetWebSettings()
{
    ASSERT(m_impl->page);
    if (m_impl->page)
        return wxWebSettings(m_impl->page->settings());
    
    return wxWebSettings();
}

wxWebKitCompatibilityMode wxWebView::GetCompatibilityMode() const
{
    if (m_mainFrame)
        return m_mainFrame->GetCompatibilityMode();

    return QuirksMode;
}

void wxWebView::GrantUniversalAccess()
{
    if (m_mainFrame)
        m_mainFrame->GrantUniversalAccess();
}
