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
 
#ifndef WebBrowserShell_h
#define WebBrowserShell_h

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebKitDefines.h"
#include "WebView.h"
#include <wx/srchctrl.h>

namespace WebKit {

class WXDLLIMPEXP_WEBKIT WebBrowserShell : public wxFrame
{
public:
    // ctor(s)
#if SWIG
    %pythonAppend WebBrowserShell "self._setOORInfo(self)"
#endif
    WebBrowserShell(const wxString& title, const wxString& url = "about:blank");

#ifndef SWIG
    ~WebBrowserShell();
#endif

    void ShowDebugMenu(bool show = true);
    WebView* webview;

protected:

    // event handlers (these functions should _not_ be virtual)
    void OnCut(wxCommandEvent&);
    void OnCopy(wxCommandEvent&);
    void OnPaste(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    void OnAbout(wxCommandEvent&);
    void OnLoadFile(wxCommandEvent&);
    void OnAddressBarEnter(wxCommandEvent&);
    void OnSearchCtrlEnter(wxCommandEvent&);
    void OnLoadEvent(WebViewLoadEvent& event);
    void OnBeforeLoad(WebViewBeforeLoadEvent& event);
    void OnBack(wxCommandEvent&);
    void OnForward(wxCommandEvent&);
    void OnStop(wxCommandEvent&);
    void OnReload(wxCommandEvent&);
    void OnBrowse(wxCommandEvent&);
    void OnEdit(wxCommandEvent&);
    void OnPrint(wxCommandEvent& myEvent);
    
    void OnMakeTextLarger(wxCommandEvent&);
    void OnMakeTextSmaller(wxCommandEvent&);
    void OnGetSource(wxCommandEvent&);
    
    // debug menu items
    void OnSetSource(wxCommandEvent&);
    void OnRunScript(wxCommandEvent&);
    void OnEditCommand(wxCommandEvent&);
    void OnGetEditCommandState(wxCommandEvent&);

private:
    wxTextCtrl* addressBar;
    wxSearchCtrl* searchCtrl;

    bool m_checkBeforeLoad;
    wxMenu* m_debugMenu;
    // any class wishing to process wxWindows events must use this macro
#ifndef SWIG
    DECLARE_EVENT_TABLE()
#endif
};

class WXDLLIMPEXP_WEBKIT PageSourceViewFrame : public wxFrame
{
public:
    PageSourceViewFrame(const wxString& source);
};

}

#endif // ifndef WebBrowserShell_h
