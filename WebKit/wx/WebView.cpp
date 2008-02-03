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
#include "CString.h"
#include "DeprecatedString.h"
#include "Document.h"
#include "Element.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "Logging.h"
#include "markup.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "PlatformWheelEvent.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SubstituteData.h"

#include "ChromeClientWx.h"
#include "ContextMenuClientWx.h"
#include "DragClientWx.h"
#include "EditorClientWx.h"
#include "FrameLoaderClientWx.h"
#include "InspectorClientWx.h"

#include "kjs_proxy.h"
#include "kjs_binding.h"
#include <kjs/value.h>
#include <kjs/ustring.h>

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebView.h"
#include "WebViewPrivate.h"

#include <wx/defs.h>
#include <wx/dcbuffer.h>

// Match Safari's min/max zoom sizes by default
#define MinimumTextSizeMultiplier       0.5f
#define MaximumTextSizeMultiplier       3.0f
#define TextSizeMultiplierRatio         1.2f


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
    SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewBeforeLoadEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_BEFORE_LOAD)

wxWebViewBeforeLoadEvent::wxWebViewBeforeLoadEvent(wxWindow* win)
{
    m_cancelled = false;
    SetEventType(wxEVT_WEBVIEW_BEFORE_LOAD);
    SetEventObject(win);
    SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewNewWindowEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_NEW_WINDOW)

wxWebViewNewWindowEvent::wxWebViewNewWindowEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_NEW_WINDOW);
    SetEventObject(win);
    SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewRightClickEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_RIGHT_CLICK)

wxWebViewRightClickEvent::wxWebViewRightClickEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_RIGHT_CLICK);
    SetEventObject(win);
    SetId(win->GetId());
}

IMPLEMENT_DYNAMIC_CLASS(wxWebViewConsoleMessageEvent, wxCommandEvent)

DEFINE_EVENT_TYPE(wxEVT_WEBVIEW_CONSOLE_MESSAGE)

wxWebViewConsoleMessageEvent::wxWebViewConsoleMessageEvent(wxWindow* win)
{
    SetEventType(wxEVT_WEBVIEW_CONSOLE_MESSAGE);
    SetEventObject(win);
    SetId(win->GetId());
}

//---------------------------------------------------------
// DOM Element info data type
//---------------------------------------------------------

wxWebViewDOMElementInfo::wxWebViewDOMElementInfo() :
    m_domElement(NULL),
    m_isSelected(false),
    m_text(wxEmptyString),
    m_imageSrc(wxEmptyString),
    m_link(wxEmptyString)
{
}

BEGIN_EVENT_TABLE(wxWebView, wxWindow)
    EVT_PAINT(wxWebView::OnPaint)
    EVT_SIZE(wxWebView::OnSize)
    EVT_MOUSE_EVENTS(wxWebView::OnMouseEvents)
    EVT_KEY_DOWN(wxWebView::OnKeyEvents)
    EVT_KEY_UP(wxWebView::OnKeyEvents)
    EVT_CHAR(wxWebView::OnKeyEvents)
    EVT_SET_FOCUS(wxWebView::OnSetFocus)
    EVT_KILL_FOCUS(wxWebView::OnKillFocus)
    EVT_ACTIVATE(wxWebView::OnActivate)
END_EVENT_TABLE()

wxWebView::wxWebView(wxWindow* parent, int id, const wxPoint& position, 
                    const wxSize& size, WebViewFrameData* data) :
    m_textMagnifier(1.0),
    m_isEditable(false),
    m_isInitialized(false),
    m_beingDestroyed(false),
    m_title(wxEmptyString)
{
    if (!wxWindow::Create(parent, id, position, size, wxBORDER_NONE | wxHSCROLL | wxVSCROLL))
        return;

// This is necessary because we are using SharedTimerWin.cpp on Windows,
// due to a problem with exceptions getting eaten when using the callback
// approach to timers (which wx itself uses).
#if __WXMSW__
    WebCore::Page::setInstanceHandle(wxGetInstance());
#endif

    // this helps reduce flicker on platforms like MSW
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    m_impl = new WebViewPrivate();

    WebCore::InitializeLoggingChannelsIfNecessary();    
    WebCore::HTMLFrameOwnerElement* parentFrame = 0;

    // FIXME: This cast is obviously not as safe as a dynamic
    // cast, but this allows us to get around requiring RTTI
    // support for the moment. This is only used for subframes
    // in any case, which aren't currently supported.
    wxWebView* parentWebView = static_cast<wxWebView*>(parent);
    
    if (data) {
        parentFrame = data->ownerElement;
        m_impl->page = parentWebView->m_impl->frame->page();
    }
    else {
        WebCore::EditorClientWx* editorClient = new WebCore::EditorClientWx();
        m_impl->page = new WebCore::Page(new WebCore::ChromeClientWx(this), new WebCore::ContextMenuClientWx(), editorClient, new WebCore::DragClientWx(), new WebCore::InspectorClientWx());
        editorClient->setPage(m_impl->page);
    }
    
    WebCore::FrameLoaderClientWx* loaderClient = new WebCore::FrameLoaderClientWx();
    
    m_impl->frame = new WebCore::Frame(m_impl->page, parentFrame, loaderClient);
    m_impl->frame->deref();
    m_impl->frameView = new WebCore::FrameView(m_impl->frame.get());
    m_impl->frameView->deref();
    
    m_impl->frame->setView(m_impl->frameView.get());
    m_impl->frame->init();
    
    m_impl->frameView->setNativeWindow(this);
    loaderClient->setFrame(m_impl->frame.get());
        
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

    m_isInitialized = true;
}

wxWebView::~wxWebView()
{
    m_beingDestroyed = true;
    
    m_impl->frame->loader()->detachFromParent();
    
    delete m_impl->page;
    m_impl->page = 0;
    // Since frameView has the last reference to Frame, once it is
    // destroyed the destructor for Frame will happen as well.
    m_impl->frameView = 0;    
}

void wxWebView::Stop()
{
    if (m_impl->frame && m_impl->frame->loader())
        m_impl->frame->loader()->stop();
}

void wxWebView::Reload()
{
    if (m_impl->frame && m_impl->frame->loader())
        m_impl->frame->loader()->reload();
}

wxString wxWebView::GetPageSource()
{
    if (m_impl->frame) {
        if (m_impl->frameView && m_impl->frameView->layoutPending())
            m_impl->frameView->layout();
    
        WebCore::Document* doc = m_impl->frame->document();
        
        if (doc) {
            wxString source = doc->toString();
            return source;
        }
    }
    return wxEmptyString;
}

void wxWebView::SetPageSource(const wxString& source, const wxString& baseUrl)
{
    if (m_impl->frame && m_impl->frame->loader()) {
        WebCore::FrameLoader* loader = m_impl->frame->loader();
        loader->begin(WebCore::KURL(static_cast<const char*>(baseUrl.mb_str(wxConvUTF8))));
        loader->write(source);
        loader->end();
    }
}

wxString wxWebView::GetInnerText()
{
    if (m_impl->frameView && m_impl->frameView->layoutPending())
        m_impl->frameView->layout();
        
    WebCore::Element *documentElement = m_impl->frame->document()->documentElement();
    return documentElement->innerText();
}

wxString wxWebView::GetAsMarkup()
{
    if (!m_impl->frame || !m_impl->frame->document())
        return wxEmptyString;

    return createMarkup(m_impl->frame->document());
}

wxString wxWebView::GetExternalRepresentation()
{
    if (m_impl->frameView && m_impl->frameView->layoutPending())
        m_impl->frameView->layout();

    return externalRepresentation(m_impl->frame->renderer());
}

wxString wxWebView::RunScript(const wxString& javascript)
{
    wxString returnValue = wxEmptyString;
    if (m_impl->frame) {
        KJS::JSValue* result = m_impl->frame->loader()->executeScript(javascript, true);
        if (result)
            returnValue = wxString(result->toString(m_impl->frame->scriptProxy()->globalObject()->globalExec()).UTF8String().c_str(), wxConvUTF8);        
    }
    return returnValue;
}

void wxWebView::LoadURL(wxString url)
{
    if (m_impl->frame && m_impl->frame->loader()) {
        WebCore::KURL kurl = WebCore::KURL(static_cast<const char*>(url.mb_str(wxConvUTF8)));
        // NB: This is an ugly fix, but CURL won't load sub-resources if the
        // protocol is omitted; sadly, it will not emit an error, either, so
        // there's no way for us to catch this problem the correct way yet.
        if (kurl.protocol().isEmpty()) {
            // is it a file on disk?
            if (wxFileExists(url)) {
                kurl.setProtocol("file");
                kurl.setPath("//" + kurl.path());
            }
            else {
                kurl.setProtocol("http");
                kurl.setPath("//" + kurl.path());
            }
        }
        m_impl->frame->loader()->load(kurl);
    }
}

bool wxWebView::GoBack()
{
    if (m_impl->frame && m_impl->frame->page()) {
        return m_impl->frame->page()->goBack();
    }
}

bool wxWebView::GoForward()
{
    if (m_impl->frame && m_impl->frame->page())
        return m_impl->frame->page()->goForward();
}

bool wxWebView::CanIncreaseTextSize() const
{
    if (m_impl->frame) {
        if (m_textMagnifier*TextSizeMultiplierRatio <= MaximumTextSizeMultiplier)
            return true;
    }
    return false;
}

void wxWebView::IncreaseTextSize()
{
    if (CanIncreaseTextSize()) {
        m_textMagnifier = m_textMagnifier*TextSizeMultiplierRatio;
        m_impl->frame->setZoomFactor((int)rint(m_textMagnifier*100));
    }
}

bool wxWebView::CanDecreaseTextSize() const
{
    if (m_impl->frame) {
        if (m_textMagnifier/TextSizeMultiplierRatio >= MinimumTextSizeMultiplier)
            return true;
    }
    return false;
}

void wxWebView::DecreaseTextSize()
{        
    if (CanDecreaseTextSize()) {
        m_textMagnifier = m_textMagnifier/TextSizeMultiplierRatio;
        m_impl->frame->setZoomFactor( (int)rint(m_textMagnifier*100));
    }
}

void wxWebView::MakeEditable(bool enable)
{
    m_isEditable = enable;
}


/* 
 * Event forwarding functions to send events down to WebCore.
 */

void wxWebView::OnPaint(wxPaintEvent& event)
{
    if (m_beingDestroyed || !m_impl->frameView || !m_impl->frame)
        return;
    
    wxAutoBufferedPaintDC dc(this);

    if (IsShown() && m_impl->frame && m_impl->frame->document()) {
#if USE(WXGC)
        wxGCDC gcdc(dc);
#endif

        if (dc.IsOk()) {
            wxRect paintRect = GetUpdateRegion().GetBox();

            WebCore::IntSize offset = m_impl->frameView->scrollOffset();
            dc.SetDeviceOrigin(-offset.width(), -offset.height());
            paintRect.Offset(offset.width(), offset.height());

#if USE(WXGC)
            WebCore::GraphicsContext* gc = new WebCore::GraphicsContext(&gcdc);
#else
            WebCore::GraphicsContext* gc = new WebCore::GraphicsContext((wxWindowDC*)&dc);
#endif
            if (gc && m_impl->frame->renderer()) {
                if (m_impl->frameView->needsLayout())
                    m_impl->frameView->layout();

                m_impl->frame->paint(gc, paintRect);
            }
        }
    }
}

void wxWebView::OnSize(wxSizeEvent& event)
{ 
    if (m_isInitialized && m_impl->frame && m_impl->frameView) {
        m_impl->frame->sendResizeEvent();
        m_impl->frameView->layout();
    }
    
    event.Skip();

}

void wxWebView::OnMouseEvents(wxMouseEvent& event)
{
    event.Skip();
    
    if (!m_impl->frame  && m_impl->frameView)
        return; 
        
    wxPoint globalPoint = ClientToScreen(event.GetPosition());

    wxEventType type = event.GetEventType();
    
    if (type == wxEVT_MOUSEWHEEL) {
        WebCore::PlatformWheelEvent wkEvent(event, globalPoint);
        m_impl->frame->eventHandler()->handleWheelEvent(wkEvent);
        return;
    }
    
    WebCore::PlatformMouseEvent wkEvent(event, globalPoint);

    if (type == wxEVT_LEFT_DOWN || type == wxEVT_MIDDLE_DOWN || type == wxEVT_RIGHT_DOWN)
        m_impl->frame->eventHandler()->handleMousePressEvent(wkEvent);
    
    else if (type == wxEVT_LEFT_UP || type == wxEVT_MIDDLE_UP || type == wxEVT_RIGHT_UP || 
                type == wxEVT_LEFT_DCLICK || type == wxEVT_MIDDLE_DCLICK || type == wxEVT_RIGHT_DCLICK)
        m_impl->frame->eventHandler()->handleMouseReleaseEvent(wkEvent);

    else if (type == wxEVT_MOTION)
        m_impl->frame->eventHandler()->handleMouseMoveEvent(wkEvent);
}

bool wxWebView::CanCopy()
{
    if (m_impl->frame && m_impl->frameView)
        return (m_impl->frame->editor()->canCopy() || m_impl->frame->editor()->canDHTMLCopy());

    return false;
}

void wxWebView::Copy()
{
    if (CanCopy())
        m_impl->frame->editor()->copy();
}

bool wxWebView::CanCut()
{
    if (m_impl->frame && m_impl->frameView)
        return (m_impl->frame->editor()->canCut() || m_impl->frame->editor()->canDHTMLCut());

    return false;
}

void wxWebView::Cut()
{
    if (CanCut())
        m_impl->frame->editor()->cut();
}

bool wxWebView::CanPaste()
{
    if (m_impl->frame && m_impl->frameView)
        return (m_impl->frame->editor()->canPaste() || m_impl->frame->editor()->canDHTMLPaste());

    return false;
}

void wxWebView::Paste()
{
    if (CanPaste())
        m_impl->frame->editor()->paste();

}

void wxWebView::OnKeyEvents(wxKeyEvent& event)
{
    if (m_impl->frame && m_impl->frameView) {
        // WebCore doesn't handle these events itself, so we need to do
        // it and not send the event down or else CTRL+C will erase the text
        // and replace it with c.
        if (event.CmdDown() && event.GetKeyCode() == static_cast<int>('C'))
            Copy();
        else if (event.CmdDown() && event.GetKeyCode() == static_cast<int>('X'))
            Cut();
        else if (event.CmdDown() && event.GetKeyCode() == static_cast<int>('V'))
            Paste();
        else {    
            WebCore::PlatformKeyboardEvent wkEvent(event);
            if (wkEvent.type() == WebCore::PlatformKeyboardEvent::Char && wkEvent.altKey())
                m_impl->frame->eventHandler()->handleAccessKey(wkEvent);
            else
                m_impl->frame->eventHandler()->keyEvent(wkEvent);
        }
    }
    
    // make sure we get the character event.
    if (event.GetEventType() != wxEVT_CHAR)
        event.Skip();
}

void wxWebView::OnSetFocus(wxFocusEvent& event)
{
    if (m_impl->frame)
        m_impl->frame->selectionController()->setFocused(true);

    event.Skip();
}

void wxWebView::OnKillFocus(wxFocusEvent& event)
{
    if (m_impl->frame)
        m_impl->frame->selectionController()->setFocused(false);

    event.Skip();
}

void wxWebView::OnActivate(wxActivateEvent& event)
{
    if (m_impl->page)
        m_impl->page->focusController()->setActive(event.GetActive());

    event.Skip();
}
