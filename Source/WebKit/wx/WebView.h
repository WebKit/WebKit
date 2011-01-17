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

#include "WebKitDefines.h"
#include "WebDOMSelection.h"
#include "WebFrame.h"
#include "WebSettings.h"

class WebViewPrivate;
class WebViewFrameData;
class wxWebFrame;

typedef struct OpaqueJSContext* JSGlobalContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

namespace WebCore {
    class ChromeClientWx;
    class FrameLoaderClientWx;
}

#ifndef SWIG
extern WXDLLIMPEXP_WEBKIT const wxChar* wxWebViewNameStr;
#endif

static const int defaultCacheCapacity = 8192 * 1024; // mirrors MemoryCache.cpp

class WXDLLIMPEXP_WEBKIT wxWebViewCachePolicy
{
public:
    wxWebViewCachePolicy(unsigned minDead = 0, unsigned maxDead = defaultCacheCapacity, unsigned totalCapacity = defaultCacheCapacity)
        : m_minDeadCapacity(minDead)
        , m_maxDeadCapacity(maxDead)
        , m_capacity(totalCapacity)
    {}

    ~wxWebViewCachePolicy() {}

    unsigned GetCapacity() const { return m_capacity; }
    void SetCapacity(int capacity) { m_capacity = capacity; }

    unsigned GetMinDeadCapacity() const { return m_minDeadCapacity; }
    void SetMinDeadCapacity(unsigned minDeadCapacity) { m_minDeadCapacity = minDeadCapacity; }

    unsigned GetMaxDeadCapacity() const { return m_maxDeadCapacity; }
    void SetMaxDeadCapacity(unsigned maxDeadCapacity) { m_maxDeadCapacity = maxDeadCapacity; }

protected:
    unsigned m_capacity;
    unsigned m_minDeadCapacity;
    unsigned m_maxDeadCapacity;
};


// copied from WebKit/mac/Misc/WebKitErrors[Private].h
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort = 103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
};

enum wxProxyType {
    HTTP,
    Socks4,
    Socks4A,
    Socks5,
    Socks5Hostname
};

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
              const wxString& name = wxWebViewNameStr); // For wxWebView internal data passing
#if SWIG
    %rename(PreWebView) wxWebView();
#else
    wxWebView();
#endif
    
    bool Create(wxWindow* parent, int id = wxID_ANY,
                const wxPoint& point = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxWebViewNameStr); // For wxWebView internal data passing
    
#ifndef SWIG
    virtual ~wxWebView();
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
    void SetPageSource(const wxString& source, const wxString& baseUrl = wxEmptyString, const wxString& mimetype = wxT("text/html"));
    
    wxString GetInnerText();
    wxString GetAsMarkup();
    wxString GetExternalRepresentation();
    
    wxWebKitSelection GetSelection();
    wxString GetSelectionAsHTML();
    wxString GetSelectionAsText();
    
    void SetTransparent(bool transparent);
    bool IsTransparent() const;
    
    wxString RunScript(const wxString& javascript);
    bool ExecuteEditCommand(const wxString& command, const wxString& parameter = wxEmptyString);
    EditState GetEditCommandState(const wxString& command) const;
    wxString GetEditCommandValue(const wxString& command) const;

    bool FindString(const wxString& string, bool forward = true,
        bool caseSensitive = false, bool wrapSelection = true,
        bool startInSelection = true);
    
    bool CanIncreaseTextSize() const;
    void IncreaseTextSize();
    bool CanDecreaseTextSize() const;
    void DecreaseTextSize();
    void ResetTextSize();
    void MakeEditable(bool enable);
    bool IsEditable() const;

    wxString GetPageTitle() const { return m_title; }
    void SetPageTitle(const wxString& title) { m_title = title; }
    
    wxWebFrame* GetMainFrame() { return m_mainFrame; }

    wxWebViewDOMElementInfo HitTest(const wxPoint& pos) const;
    
    bool ShouldClose() const;
      
    static void SetCachePolicy(const wxWebViewCachePolicy& cachePolicy);
    static wxWebViewCachePolicy GetCachePolicy();

    void SetMouseWheelZooms(bool mouseWheelZooms) { m_mouseWheelZooms = mouseWheelZooms; }
    bool GetMouseWheelZooms() const { return m_mouseWheelZooms; }

    static void SetDatabaseDirectory(const wxString& databaseDirectory);
    static wxString GetDatabaseDirectory();
    
    /**
        Sets whether or not web pages can create databases.
    */
    static void SetDatabasesEnabled(bool enabled);
    
    /**
        Returns whether or not the WebView runs JavaScript code.
    */    
    static bool AreDatabasesEnabled();

    static void SetProxyInfo(const wxString& host = wxEmptyString,
                             unsigned long port = 0,
                             wxProxyType type = HTTP,
                             const wxString& username = wxEmptyString,
                             const wxString& password = wxEmptyString);

    wxWebSettings GetWebSettings();
    wxWebKitCompatibilityMode GetCompatibilityMode() const;
    
    /*
        This method allows cross site-scripting (XSS) in the WebView. 
        Use with caution!
    */
    void GrantUniversalAccess();

protected:

    // event handlers (these functions should _not_ be virtual)
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvents(wxMouseEvent& event);
    void OnContextMenuEvents(wxContextMenuEvent& event);
    void OnMenuSelectEvents(wxCommandEvent& event);
    void OnKeyEvents(wxKeyEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    void OnTLWActivated(wxActivateEvent& event);
    
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
    bool m_mouseWheelZooms;
    WebViewPrivate* m_impl;
    wxWebFrame* m_mainFrame;
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

class WXDLLIMPEXP_WEBKIT wxWebKitWindowFeatures
{
public:
    wxWebKitWindowFeatures()
        : menuBarVisible(true)
        , statusBarVisible(true)
        , toolBarVisible(true)
        , locationBarVisible(true)
        , scrollbarsVisible(true)
        , resizable(true)
        , fullscreen(false)
        , dialog(false)
    { }

    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarVisible;
    bool locationBarVisible;
    bool scrollbarsVisible;
    bool resizable;
    bool fullscreen;
    bool dialog;
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
    wxWebView* GetWebView() { return m_webView; }
    void SetWebView(wxWebView* webView) { m_webView = webView; }
    wxWebKitWindowFeatures GetWindowFeatures() { return m_features; }
    void SetWindowFeatures(wxWebKitWindowFeatures features) { m_features = features; }

    wxWebViewNewWindowEvent( wxWindow* win = static_cast<wxWindow*>(NULL));
    wxEvent *Clone(void) const { return new wxWebViewNewWindowEvent(*this); }

private:
    wxWebView* m_webView;
    wxWebKitWindowFeatures m_features;
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

// copied from page/Console.h
enum wxWebViewConsoleMessageLevel {
    TipMessageLevel,
    LogMessageLevel,
    WarningMessageLevel,
    ErrorMessageLevel
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

    wxWebViewConsoleMessageLevel GetLevel() const { return m_level; }
    void SetLevel(wxWebViewConsoleMessageLevel level) { m_level = level; }

private:
    unsigned int m_lineNumber;
    wxString m_message;
    wxString m_sourceID;
    wxWebViewConsoleMessageLevel m_level;
};

class WXDLLIMPEXP_WEBKIT wxWebViewAlertEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewAlertEvent )
#endif

public:
    wxString GetMessage() const { return m_message; }
    void SetMessage(const wxString& message) { m_message = message; }

    wxWebViewAlertEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewAlertEvent(*this); }

private:
    wxString m_message;
};

class WXDLLIMPEXP_WEBKIT wxWebViewConfirmEvent : public wxWebViewAlertEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewConfirmEvent )
#endif

public:   
    int GetReturnCode() const { return m_returnCode; }
    void SetReturnCode(int code) { m_returnCode = code; }

    wxWebViewConfirmEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewConfirmEvent(*this); }

private:
    int m_returnCode;
};

class WXDLLIMPEXP_WEBKIT wxWebViewPromptEvent : public wxWebViewConfirmEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewPromptEvent )
#endif

public:   
    wxString GetResponse() const { return m_response; }
    void SetResponse(const wxString& response) { m_response = response; }

    wxWebViewPromptEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewPromptEvent(*this); }

private:
    wxString m_response;
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

class WXDLLIMPEXP_WEBKIT wxWebViewWindowObjectClearedEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS( wxWebViewWindowObjectClearedEvent )
#endif

public:
    JSGlobalContextRef GetJSContext() const { return m_jsContext; }
    void SetJSContext(JSGlobalContextRef context) { m_jsContext = context; }
    
    JSObjectRef GetWindowObject() const { return m_windowObject; }
    void SetWindowObject(JSObjectRef object) { m_windowObject = object; }

    wxWebViewWindowObjectClearedEvent( wxWindow* win = static_cast<wxWindow*>(NULL));
    wxEvent *Clone(void) const { return new wxWebViewWindowObjectClearedEvent(*this); }

private:
    JSGlobalContextRef m_jsContext;
    JSObjectRef m_windowObject;
};

class WXDLLIMPEXP_WEBKIT wxWebViewContentsChangedEvent : public wxCommandEvent {
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(wxWebViewContentsChangedEvent)
#endif

public:
    wxWebViewContentsChangedEvent(wxWindow* win = static_cast<wxWindow*>(0));
    wxEvent *Clone(void) const { return new wxWebViewContentsChangedEvent(*this); }
};

class WXDLLIMPEXP_WEBKIT wxWebViewSelectionChangedEvent : public wxCommandEvent {
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(wxWebViewSelectionChangedEvent)
#endif

public:
    wxWebViewSelectionChangedEvent(wxWindow* win = static_cast<wxWindow*>(0));
    wxEvent *Clone(void) const { return new wxWebViewSelectionChangedEvent(*this); }
};

typedef void (wxEvtHandler::*wxWebViewLoadEventFunction)(wxWebViewLoadEvent&);
typedef void (wxEvtHandler::*wxWebViewBeforeLoadEventFunction)(wxWebViewBeforeLoadEvent&);
typedef void (wxEvtHandler::*wxWebViewNewWindowEventFunction)(wxWebViewNewWindowEvent&);
typedef void (wxEvtHandler::*wxWebViewRightClickEventFunction)(wxWebViewRightClickEvent&);
typedef void (wxEvtHandler::*wxWebViewConsoleMessageEventFunction)(wxWebViewConsoleMessageEvent&);
typedef void (wxEvtHandler::*wxWebViewAlertEventFunction)(wxWebViewAlertEvent&);
typedef void (wxEvtHandler::*wxWebViewConfirmEventFunction)(wxWebViewConfirmEvent&);
typedef void (wxEvtHandler::*wxWebViewPromptEventFunction)(wxWebViewPromptEvent&);
typedef void (wxEvtHandler::*wxWebViewReceivedTitleEventFunction)(wxWebViewReceivedTitleEvent&);
typedef void (wxEvtHandler::*wxWebViewWindowObjectClearedFunction)(wxWebViewWindowObjectClearedEvent&);
typedef void (wxEvtHandler::*wxWebViewContentsChangedFunction)(wxWebViewContentsChangedEvent&);
typedef void (wxEvtHandler::*wxWebViewSelectionChangedFunction)(wxWebViewSelectionChangedEvent&);

#define wxWebViewLoadEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewLoadEventFunction, &func)
#define wxWebViewBeforeLoadEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewBeforeLoadEventFunction, &func)
#define wxWebViewNewWindowEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewNewWindowEventFunction, &func)
#define wxWebViewRightClickEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewRightClickEventFunction, &func)
#define wxWebViewConsoleMessageEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewConsoleMessageEventFunction, &func)
#define wxWebViewAlertEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewAlertEventFunction, &func)
#define wxWebViewConfirmEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewConfirmEventFunction, &func)
#define wxWebViewPromptEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewPromptEventFunction, &func)
#define wxWebViewReceivedTitleEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewReceivedTitleEventFunction, &func)
#define wxWebViewWindowObjectClearedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewWindowObjectClearedFunction, &func)
#define wxWebViewContentsChangedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewContentsChangedEventFunction, &func)
#define wxWebViewSelectionChangedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(wxWebViewSelectionChangedEventFunction, &func)
    
#ifndef SWIG
BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_BEFORE_LOAD, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_LOAD, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_NEW_WINDOW, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_RIGHT_CLICK, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_CONSOLE_MESSAGE, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_JS_ALERT, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_JS_CONFIRM, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_JS_PROMPT, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_RECEIVED_TITLE, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_CONTENTS_CHANGED, wxID_ANY)
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_SELECTION_CHANGED, wxID_ANY)
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

#define EVT_WEBVIEW_JS_ALERT(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_ALERT, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewAlertEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_JS_CONFIRM(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_CONFIRM, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewConfirmEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_JS_PROMPT(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_PROMPT, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewPromptEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_RECEIVED_TITLE(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RECEIVED_TITLE, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewReceivedTitleEventFunction) & func, \
                            static_cast<wxObject*>(NULL)),
                            
#define EVT_WEBVIEW_WINDOW_OBJECT_CLEARED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewWindowObjectClearedFunction) & func, \
                            static_cast<wxObject*>(NULL)),

#define EVT_WEBVIEW_CONTENTS_CHANGED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY(wxEVT_WEBVIEW_CONTENTS_CHANGED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewContentsChangedEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_SELECTION_CHANGED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY(wxEVT_WEBVIEW_SELECTION_CHANGED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewSelectionChangedEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
                            
#endif // ifndef WXWEBVIEW_H
