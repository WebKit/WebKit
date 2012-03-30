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
 
#ifndef WebView_h
#define WebView_h

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

typedef struct OpaqueJSContext* JSGlobalContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

namespace WebCore {
    class ChromeClientWx;
    class FrameLoaderClientWx;
}

static const int defaultCacheCapacity = 8192 * 1024; // mirrors MemoryCache.cpp

namespace WebKit {

class WebFrame;

#ifndef SWIG
extern WXDLLIMPEXP_WEBKIT const wxChar* WebViewNameStr;
#endif

class WXDLLIMPEXP_WEBKIT WebViewCachePolicy
{
public:
    WebViewCachePolicy(unsigned minDead = 0, unsigned maxDead = defaultCacheCapacity, unsigned totalCapacity = defaultCacheCapacity)
        : m_minDeadCapacity(minDead)
        , m_maxDeadCapacity(maxDead)
        , m_capacity(totalCapacity)
    {}

    ~WebViewCachePolicy() {}

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

class WXDLLIMPEXP_WEBKIT WebView : public wxWindow
{
    // ChromeClientWx needs to get the Page* stored by the WebView
    // for the createWindow function. 
    friend class WebCore::ChromeClientWx;
    friend class WebCore::FrameLoaderClientWx;

public:
    // ctor(s)
#if SWIG
    %pythonAppend WebView    "self._setOORInfo(self)"
    %pythonAppend WebView()  ""
#endif

    WebView(wxWindow* parent,
              const wxString& url = "about:blank", 
              int id = wxID_ANY,
              const wxPoint& point = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = 0,
              const wxString& name = WebViewNameStr); // For WebView internal data passing
#if SWIG
    %rename(PreWebView) WebView();
#else
    WebView();
#endif
    
    bool Create(wxWindow* parent,
                const wxString& url = "about:blank", 
                int id = wxID_ANY,
                const wxPoint& point = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = WebViewNameStr); // For WebView internal data passing
    
#ifndef SWIG
    virtual ~WebView();
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
    
    WebKitSelection GetSelection();
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
    
    WebFrame* GetMainFrame() { return m_mainFrame; }
    
    wxString GetMainFrameURL() const;

    WebViewDOMElementInfo HitTest(const wxPoint& pos) const;
    
    bool ShouldClose() const;
      
    static void SetCachePolicy(const WebViewCachePolicy& cachePolicy);
    static WebViewCachePolicy GetCachePolicy();

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

    WebSettings GetWebSettings();
    WebKitCompatibilityMode GetCompatibilityMode() const;
    
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
    void OnMouseCaptureLost(wxMouseCaptureLostEvent&);
    
private:
    // any class wishing to process wxWindows events must use this macro
#ifndef SWIG
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(WebView)
#endif
    float m_textMagnifier;
    bool m_isInitialized;
    bool m_beingDestroyed;
    bool m_mouseWheelZooms;
    WebViewPrivate* m_impl;
    WebFrame* m_mainFrame;
    wxString m_title;
    
};

// ----------------------------------------------------------------------------
// Web Kit Events
// ----------------------------------------------------------------------------

enum {
    WEBVIEW_LOAD_STARTED = 1,
    WEBVIEW_LOAD_NEGOTIATING = 2,
    WEBVIEW_LOAD_REDIRECTING = 4,
    WEBVIEW_LOAD_TRANSFERRING = 8,
    WEBVIEW_LOAD_STOPPED = 16,
    WEBVIEW_LOAD_FAILED = 32,
    WEBVIEW_LOAD_DL_COMPLETED = 64,
    WEBVIEW_LOAD_DOC_COMPLETED = 128,
    WEBVIEW_LOAD_ONLOAD_HANDLED = 256,
    WEBVIEW_LOAD_WINDOW_OBJECT_CLEARED = 512
};

enum {
    WEBVIEW_NAV_LINK_CLICKED = 1,
    WEBVIEW_NAV_BACK_NEXT = 2,
    WEBVIEW_NAV_FORM_SUBMITTED = 4,
    WEBVIEW_NAV_RELOAD = 8,
    WEBVIEW_NAV_FORM_RESUBMITTED = 16,
    WEBVIEW_NAV_OTHER = 32
};

class WXDLLIMPEXP_WEBKIT WebViewBeforeLoadEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewBeforeLoadEvent)
#endif

public:
    bool IsCancelled() const { return m_cancelled; }
    void Cancel(bool cancel = true) { m_cancelled = cancel; }
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    void SetNavigationType(int navType) { m_navType = navType; }
    int GetNavigationType() const { return m_navType; }
    WebFrame* GetFrame() const { return m_frame; }
    void SetFrame(WebFrame* frame) { m_frame = frame; }

    WebViewBeforeLoadEvent(wxWindow* win = 0);
    wxEvent *Clone(void) const { return new WebViewBeforeLoadEvent(*this); }

private:
    bool m_cancelled;
    wxString m_url;
    int m_navType;
    WebFrame* m_frame;
};

class WXDLLIMPEXP_WEBKIT WebViewLoadEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewLoadEvent)
#endif

public:
    int GetState() const { return m_state; }
    void SetState(const int state) { m_state = state; }
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    WebFrame* GetFrame() const { return m_frame; }
    void SetFrame(WebFrame* frame) { m_frame = frame; }

    WebViewLoadEvent(wxWindow* win = 0);
    wxEvent *Clone(void) const { return new WebViewLoadEvent(*this); }

private:
    int m_state;
    wxString m_url;
    WebFrame* m_frame;
};

class WXDLLIMPEXP_WEBKIT WebKitWindowFeatures
{
public:
    WebKitWindowFeatures()
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

class WXDLLIMPEXP_WEBKIT WebViewNewWindowEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewNewWindowEvent)
#endif

public:
    wxString GetURL() const { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    wxString GetTargetName() const { return m_targetName; }
    void SetTargetName(const wxString& name) { m_targetName = name; }
    WebView* GetWebView() { return m_webView; }
    void SetWebView(WebView* webView) { m_webView = webView; }
    WebKitWindowFeatures GetWindowFeatures() { return m_features; }
    void SetWindowFeatures(WebKitWindowFeatures features) { m_features = features; }
    WebFrame* GetFrame() const { return m_frame; }
    void SetFrame(WebFrame* frame) { m_frame = frame; }

    WebViewNewWindowEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewNewWindowEvent(*this); }

private:
    WebView* m_webView;
    WebFrame* m_frame;
    WebKitWindowFeatures m_features;
    wxString m_url;
    wxString m_targetName;
};

class WXDLLIMPEXP_WEBKIT WebViewRightClickEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewRightClickEvent)
#endif

public:
    WebViewRightClickEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewRightClickEvent(*this); }
    
    WebViewDOMElementInfo GetInfo() const { return m_info; }
    void SetInfo(WebViewDOMElementInfo info) { m_info = info; }
    
    wxPoint GetPosition() const { return m_position; }
    void SetPosition(wxPoint pos) { m_position = pos; }

private:
    WebViewDOMElementInfo m_info;
    wxPoint m_position;
};

// copied from page/Console.h
enum WebViewConsoleMessageLevel {
    TipMessageLevel,
    LogMessageLevel,
    WarningMessageLevel,
    ErrorMessageLevel
};

class WXDLLIMPEXP_WEBKIT WebViewConsoleMessageEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewConsoleMessageEvent)
#endif

public:
    wxString GetMessage() const { return m_message; }
    void SetMessage(const wxString& message) { m_message = message; }
    
    unsigned int GetLineNumber() const { return m_lineNumber; }
    void SetLineNumber(unsigned int lineNumber) { m_lineNumber = lineNumber; }
    
    wxString GetSourceID() const { return m_sourceID; }
    void SetSourceID(const wxString& sourceID) { m_sourceID = sourceID; }

    WebViewConsoleMessageEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewConsoleMessageEvent(*this); }

    WebViewConsoleMessageLevel GetLevel() const { return m_level; }
    void SetLevel(WebViewConsoleMessageLevel level) { m_level = level; }

private:
    unsigned int m_lineNumber;
    wxString m_message;
    wxString m_sourceID;
    WebViewConsoleMessageLevel m_level;
};

class WXDLLIMPEXP_WEBKIT WebViewAlertEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewAlertEvent)
#endif

public:
    wxString GetMessage() const { return m_message; }
    void SetMessage(const wxString& message) { m_message = message; }

    WebViewAlertEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewAlertEvent(*this); }

private:
    wxString m_message;
};

class WXDLLIMPEXP_WEBKIT WebViewConfirmEvent : public WebViewAlertEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewConfirmEvent)
#endif

public:   
    int GetReturnCode() const { return m_returnCode; }
    void SetReturnCode(int code) { m_returnCode = code; }

    WebViewConfirmEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewConfirmEvent(*this); }

private:
    int m_returnCode;
};

class WXDLLIMPEXP_WEBKIT WebViewPromptEvent : public WebViewConfirmEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewPromptEvent)
#endif

public:   
    wxString GetResponse() const { return m_response; }
    void SetResponse(const wxString& response) { m_response = response; }

    WebViewPromptEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewPromptEvent(*this); }

private:
    wxString m_response;
};

class WXDLLIMPEXP_WEBKIT WebViewReceivedTitleEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewReceivedTitleEvent)
#endif

public:
    wxString GetTitle() const { return m_title; }
    void SetTitle(const wxString& title) { m_title = title; }

    WebFrame* GetFrame() const { return m_frame; }
    void SetFrame(WebFrame* frame) { m_frame = frame; }

    WebViewReceivedTitleEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewReceivedTitleEvent(*this); }

private:
    wxString m_title;
    WebFrame* m_frame;
};

class WXDLLIMPEXP_WEBKIT WebViewWindowObjectClearedEvent : public wxCommandEvent
{
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewWindowObjectClearedEvent)
#endif

public:
    JSGlobalContextRef GetJSContext() const { return m_jsContext; }
    void SetJSContext(JSGlobalContextRef context) { m_jsContext = context; }
    
    JSObjectRef GetWindowObject() const { return m_windowObject; }
    void SetWindowObject(JSObjectRef object) { m_windowObject = object; }

    WebViewWindowObjectClearedEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewWindowObjectClearedEvent(*this); }

private:
    JSGlobalContextRef m_jsContext;
    JSObjectRef m_windowObject;
};

class WXDLLIMPEXP_WEBKIT WebViewContentsChangedEvent : public wxCommandEvent {
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewContentsChangedEvent)
#endif

public:
    WebViewContentsChangedEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewContentsChangedEvent(*this); }
};

class WXDLLIMPEXP_WEBKIT WebViewSelectionChangedEvent : public wxCommandEvent {
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewSelectionChangedEvent)
#endif

public:
    WebViewSelectionChangedEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewSelectionChangedEvent(*this); }
};

class WXDLLIMPEXP_WEBKIT WebViewPrintFrameEvent : public wxCommandEvent {
#ifndef SWIG
    DECLARE_DYNAMIC_CLASS(WebViewPrintFrameEvent)
#endif
    
public:
    WebViewPrintFrameEvent(wxWindow* win = 0);
    wxEvent *Clone() const { return new WebViewPrintFrameEvent(*this); }
    
    WebFrame* GetWebFrame() { return m_webFrame; }
    void SetWebFrame(WebFrame* frame) { m_webFrame = frame; }
private:
    WebFrame* m_webFrame;
};

typedef void (wxEvtHandler::*WebViewLoadEventFunction)(WebViewLoadEvent&);
typedef void (wxEvtHandler::*WebViewBeforeLoadEventFunction)(WebViewBeforeLoadEvent&);
typedef void (wxEvtHandler::*WebViewNewWindowEventFunction)(WebViewNewWindowEvent&);
typedef void (wxEvtHandler::*WebViewRightClickEventFunction)(WebViewRightClickEvent&);
typedef void (wxEvtHandler::*WebViewConsoleMessageEventFunction)(WebViewConsoleMessageEvent&);
typedef void (wxEvtHandler::*WebViewAlertEventFunction)(WebViewAlertEvent&);
typedef void (wxEvtHandler::*WebViewConfirmEventFunction)(WebViewConfirmEvent&);
typedef void (wxEvtHandler::*WebViewPromptEventFunction)(WebViewPromptEvent&);
typedef void (wxEvtHandler::*WebViewReceivedTitleEventFunction)(WebViewReceivedTitleEvent&);
typedef void (wxEvtHandler::*WebViewWindowObjectClearedFunction)(WebViewWindowObjectClearedEvent&);
typedef void (wxEvtHandler::*WebViewContentsChangedFunction)(WebViewContentsChangedEvent&);
typedef void (wxEvtHandler::*WebViewSelectionChangedFunction)(WebViewSelectionChangedEvent&);
typedef void (wxEvtHandler::*WebViewPrintFrameFunction)(WebViewPrintFrameEvent&);

#define WebViewLoadEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewLoadEventFunction, &func)
#define WebViewBeforeLoadEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewBeforeLoadEventFunction, &func)
#define WebViewNewWindowEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewNewWindowEventFunction, &func)
#define WebViewRightClickEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewRightClickEventFunction, &func)
#define WebViewConsoleMessageEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewConsoleMessageEventFunction, &func)
#define WebViewAlertEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewAlertEventFunction, &func)
#define WebViewConfirmEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewConfirmEventFunction, &func)
#define WebViewPromptEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewPromptEventFunction, &func)
#define WebViewReceivedTitleEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewReceivedTitleEventFunction, &func)
#define WebViewWindowObjectClearedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewWindowObjectClearedFunction, &func)
#define WebViewContentsChangedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewContentsChangedEventFunction, &func)
#define WebViewSelectionChangedEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewSelectionChangedEventFunction, &func)
#define WebViewPrintFrameEventHandler(func) \
    (wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(WebViewPrintFrameEventFunction, &func)

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
    DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBKIT, wxEVT_WEBVIEW_PRINT_FRAME, wxID_ANY)
END_DECLARE_EVENT_TYPES()
#endif

#define EVT_WEBVIEW_LOAD(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_LOAD, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewLoadEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_BEFORE_LOAD(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_BEFORE_LOAD, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewBeforeLoadEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_NEW_WINDOW(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_NEW_WINDOW, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewNewWindowEventFunction) & func, \
                            static_cast<wxObject*>(0)),

#define EVT_WEBVIEW_RIGHT_CLICK(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RIGHT_CLICK, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewRightClickEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_CONSOLE_MESSAGE(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_CONSOLE_MESSAGE, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewConsoleMessageEventFunction) & func, \
                            static_cast<wxObject*>(0)),

#define EVT_WEBVIEW_JS_ALERT(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_ALERT, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewAlertEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_JS_CONFIRM(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_CONFIRM, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewConfirmEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_JS_PROMPT(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_JS_PROMPT, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewPromptEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_RECEIVED_TITLE(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RECEIVED_TITLE, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewReceivedTitleEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_WINDOW_OBJECT_CLEARED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewWindowObjectClearedFunction) & func, \
                            static_cast<wxObject*>(0)),

#define EVT_WEBVIEW_CONTENTS_CHANGED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY(wxEVT_WEBVIEW_CONTENTS_CHANGED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewContentsChangedEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
#define EVT_WEBVIEW_SELECTION_CHANGED(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY(wxEVT_WEBVIEW_SELECTION_CHANGED, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewSelectionChangedEventFunction) & func, \
                            static_cast<wxObject*>(0)),

#define EVT_WEBVIEW_PRINT_FRAME(winid, func)                       \
            DECLARE_EVENT_TABLE_ENTRY(wxEVT_WEBVIEW_PRINT_FRAME, \
                            winid, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (WebViewPrintFrameEventFunction) & func, \
                            static_cast<wxObject*>(0)),
                            
} // namespace WebKit

#endif // ifndef WebView_h
