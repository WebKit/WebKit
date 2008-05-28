/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 *
 * All rights reserved.
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
 
#ifndef WXWEBVIEW_H
#define WXWEBVIEW_H

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

class WebViewPrivate;
class WebViewFrameData;

namespace WebCore {
    class ChromeClientWx;
    class FrameLoaderClientWx;
}

#ifndef SWIG

#if WXMAKINGDLL_WEBKIT
#define WXDLLIMPEXP_WEBKIT WXEXPORT
#elif defined(WXUSINGDLL_WEBKIT)
#define WXDLLIMPEXP_WEBKIT WXIMPORT
#else
#define WXDLLIMPEXP_WEBKIT
#endif

#else 
#define WXDLLIMPEXP_WEBKIT
#endif // SWIG

extern WXDLLIMPEXP_WEBKIT const wxChar* wxWebViewNameStr;

class WXDLLIMPEXP_WEBKIT wxWebView : public wxWindow
{
    // ChromeClientWx needs to get the Page* stored by the wxWebView
    // for the createWindow function. 
    friend class WebCore::ChromeClientWx;
    friend class WebCore::FrameLoaderClientWx;

public:
    // ctor(s)
#if SWIG
    %pythonAppend wxWebView    "self._setOORInfo(self)"
    %pythonAppend wxWebView()  ""
#endif

    wxWebView(wxWindow* parent, int id = wxID_ANY,
              const wxPoint& point = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = wxWebViewNameStr,
              WebViewFrameData* data = NULL); // For wxWebView internal data passing
#if SWIG
    %rename(PreWebView) wxWebView();
#else
    wxWebView();
#endif
    
    bool Create(wxWindow* parent, int id = wxID_ANY,
                const wxPoint& point = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxWebViewNameStr,
                WebViewFrameData* data = NULL); // For wxWebView internal data passing
    
#ifndef SWIG
    ~wxWebView();
#endif
    
    void LoadURL(const wxString& url);
    bool GoBack();
    bool GoForward();
    void Stop();
    void Reload();

    bool CanGoBack();
    bool CanGoForward();
    
    bool CanCut();
    bool CanCopy();
    bool CanPaste();
    
    void Cut();
    void Copy();
    void Paste();
    
    //bool CanGetPageSource();
    wxString GetPageSource();
    void SetPageSource(const wxString& source, const wxString& baseUrl = wxEmptyString);
    
    wxString GetInnerText();
    wxString GetAsMarkup();
    wxString GetExternalRepresentation();
    
    wxString RunScript(const wxString& javascript);
    
    bool CanIncreaseTextSize() const;
    void IncreaseTextSize();
    bool CanDecreaseTextSize() const;
    void DecreaseTextSize();
    void MakeEditable(bool enable);
    bool IsEditable() const { return m_isEditable; }

    wxString GetPageTitle() const { return m_title; }
    void SetPageTitle(const wxString& title) { m_title = title; }

protected:

    // event handlers (these functions should _not_ be virtual)
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvents(wxMouseEvent& event);
    void OnKeyEvents(wxKeyEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    void OnActivate(wxActivateEvent& event);
    
private:
    // any class wishing to process wxWindows events must use this macro
#ifndef SWIG
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxWebView)
#endif
    float m_textMagnifier;
    bool m_isEditable;
    bool m_isInitialized;
    bool m_beingDestroyed;
    WebViewPrivate* m_impl;
    wxString m_title;
    
};

// ----------------------------------------------------------------------------
// Web Kit Events
// ----------------------------------------------------------------------------

enum {
    wxWEBVIEW_LOAD_STARTED = 1,
    wxWEBVIEW_LOAD_NEGOTIATING = 2,
    wxWEBVIEW_LOAD_REDIRECTING = 4,
    wxWEBVIEW_LOAD_TRANSFERRING = 8,
    wxWEBVIEW_LOAD_STOPPED = 16,
    wxWEBVIEW_LOAD_FAILED = 32,
    wxWEBVIEW_LOAD_DL_COMPLETED = 64,
    wxWEBVIEW_LOAD_DOC_COMPLETED = 128,
    wxWEBVIEW_LOAD_ONLOAD_HANDLED = 256,
    wxWEBVIEW_LOAD_WINDOW_OBJECT_CLEARED = 512
};

enum {
    wxWEBVIEW_NAV_LINK_CLICKED = 1,
    wxWEBVIEW_NAV_BACK_NEXT = 2,
    wxWEBVIEW_NAV_FORM_SUBMITTED = 4,
    wxWEBVIEW_NAV_RELOAD = 8,
    wxWEBVIEW_NAV_FORM_RESUBMITTED = 16,
    wxWEBVIEW_NAV_OTHER = 32
};

class WXDLLIMPEXP_WEBKIT wxWebViewDOMElementInfo
{
public: 
    wxWebViewDOMElementInfo();

    ~wxWebViewDOMElementInfo() { }
    
    wxString GetTagName() const { return m_tagName; }
    void SetTagName(const wxString& name) { m_tagName = name; }

    bool IsSelected() const { return m_isSelected; }
    void SetSelected(bool sel) { m_isSelected = sel; }
 
    wxString GetText() const { return m_text; }
    void SetText(const wxString& text) { m_text = text; }
 
    wxString GetImageSrc() const { return m_imageSrc; }
    void SetImageSrc(const wxString& src) { m_imageSrc = src; }
 
    wxString GetLink() const { return m_link; }
    void SetLink(const wxString& link) { m_link = link; }

private:
    void* m_domElement;
    bool m_isSelected;
    wxString m_tagName;
    wxString m_text;
    wxString m_imageSrc;
    wxString m_link;
};

class WXDLLIMPEXP_WEBKIT wxWebViewBeforeLoadEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewBeforeLoadEvent )
#endif

public:
    bool IsCancelled() const { return m_cancelled; }
    void Cancel(bool cancel = true) { m_cancelled = cancel; }
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    void SetNavigationType(int navType) { m_navType = navType; }
    int GetNavigationType() const { return m_navType; }

    wxWebViewBeforeLoadEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewBeforeLoadEvent(*this); }

private:
    bool m_cancelled;
    wxString m_url;
    int m_navType;
};

class WXDLLIMPEXP_WEBKIT wxWebViewLoadEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewLoadEvent )
#endif

public:
    int GetState() const { return m_state; }
    void SetState(const int state) { m_state = state; }
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }

    wxWebViewLoadEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewLoadEvent(*this); }

private:
    int m_state;
    wxString m_url;
};

class WXDLLIMPEXP_WEBKIT wxWebViewNewWindowEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewNewWindowEvent )
#endif

public:
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    wxString GetTargetName() const { return m_targetName; }
    void SetTargetName(const wxString& name) { m_targetName = name; }

    wxWebViewNewWindowEvent( wxWindow* win = static_cast<wxWindow*>(NULL));
    wxEvent *Clone(void) const { return new wxWebViewNewWindowEvent(*this); }

private:
    wxString m_url;
    wxString m_targetName;
};

class WXDLLIMPEXP_WEBKIT wxWebViewRightClickEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewRightClickEvent )
#endif

public:
    wxWebViewRightClickEvent( wxWindow* win = static_cast<wxWindow*>(NULL));
    wxEvent *Clone(void) const { return new wxWebViewRightClickEvent(*this); }
    
    wxWebViewDOMElementInfo GetInfo() const { return m_info; }
    void SetInfo(wxWebViewDOMElementInfo info) { m_info = info; }
    
    wxPoint GetPosition() const { return m_position; }
    void SetPosition(wxPoint pos) { m_position = pos; }

private:
    wxWebViewDOMElementInfo m_info;
    wxPoint m_position;
};

class WXDLLIMPEXP_WEBKIT wxWebViewConsoleMessageEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewConsoleMessageEvent )
#endif

public:
    wxString GetMessage() const { return m_message; }
    void SetMessage(const wxString& message) { m_message = message; }
    
    unsigned int GetLineNumber() const { return m_lineNumber; }
    void SetLineNumber(unsigned int lineNumber) { m_lineNumber = lineNumber; }
    
    wxString GetSourceID() const { return m_sourceID; }
    void SetSourceID(const wxString& sourceID) { m_sourceID = sourceID; }

    wxWebViewConsoleMessageEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewConsoleMessageEvent(*this); }

private:
    unsigned int m_lineNumber;
    wxString m_message;
    wxString m_sourceID;
};

class WXDLLIMPEXP_WEBKIT wxWebViewReceivedTitleEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewReceivedTitleEvent )
#endif

public:
    wxString GetTitle() const { return m_title; }
    void SetTitle(const wxString& title) { m_title = title; }

    wxWebViewReceivedTitleEvent( wxWindow* win = static_cast<wxWindow*>(NULL));
    wxEvent *Clone(void) const { return new wxWebViewReceivedTitleEvent(*this); }

private:
    wxString m_title;
};


typedef void (wxEvtHandler::*wxWebViewLoadEventFunction)(wxWebViewLoadEvent&);
typedef void (wxEvtHandler::*wxWebViewBeforeLoadEventFunction)(wxWebViewBeforeLoadEvent&);
typedef void (wxEvtHandler::*wxWebViewNewWindowEventFunction)(wxWebViewNewWindowEvent&);
typedef void (wxEvtHandler::*wxWebViewRightClickEventFunction)(wxWebViewRightClickEvent&);
typedef void (wxEvtHandler::*wxWebViewConsoleMessageEventFunction)(wxWebViewConsoleMessageEvent&);
typedef void (wxEvtHandler::*wxWebViewReceivedTitleEventFunction)(wxWebViewReceivedTitleEvent&);

#ifndef SWIG
BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_BEFORE_LOAD, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_LOAD, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_NEW_WINDOW, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_RIGHT_CLICK, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_CONSOLE_MESSAGE, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_RECEIVED_TITLE, wxID_ANY)
END_DECLARE_EVENT_TYPES()
#endif

#define EVT_WEBVIEW_LOAD(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_LOAD, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewLoadEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_BEFORE_LOAD(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_BEFORE_LOAD, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewBeforeLoadEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_NEW_WINDOW(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_NEW_WINDOW, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewNewWindowEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),

#define EVT_WEBVIEW_RIGHT_CLICK(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RIGHT_CLICK, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewRightClickEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_CONSOLE_MESSAGE(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_CONSOLE_MESSAGE, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewConsoleMessageEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),

#define EVT_WEBVIEW_RECEIVED_TITLE(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RECEIVED_TITLE, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewReceivedTitleEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),

#endif // ifndef WXWEBVIEW_H
