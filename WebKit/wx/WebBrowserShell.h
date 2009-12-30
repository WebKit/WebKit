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
 
#ifndef WXWEBBROWSERSHELL_H
#define WXWEBBROWSERSHELL_H

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "WebKitDefines.h"
#include "WebView.h"
#include <wx/srchctrl.h>

class WXDLLIMPEXP_WEBKIT wxWebBrowserShell : public wxFrame
{
public:
    // ctor(s)
#if SWIG
    %pythonAppend wxWebBrowserShell "self._setOORInfo(self)"
#endif
    wxWebBrowserShell(const wxString& title);

#ifndef SWIG
    ~wxWebBrowserShell();
#endif

    void ShowDebugMenu(bool show = true);
    wxWebView* webview;

protected:

    // event handlers (these functions should _not_ be virtual)
    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnLoadFile(wxCommandEvent& event);
    void OnAddressBarEnter(wxCommandEvent& event);
    void OnSearchCtrlEnter(wxCommandEvent& event);
    void OnLoadEvent(wxWebViewLoadEvent& event);
    void OnBeforeLoad(wxWebViewBeforeLoadEvent& event);
    void OnBack(wxCommandEvent& event);
    void OnForward(wxCommandEvent& event);
    void OnStop(wxCommandEvent& event);
    void OnReload(wxCommandEvent& event);
    void OnBrowse(wxCommandEvent& event);
    void OnEdit(wxCommandEvent& event);
    
    void OnMakeTextLarger(wxCommandEvent& event);
    void OnMakeTextSmaller(wxCommandEvent& event);
    void OnGetSource(wxCommandEvent& event);
    
    // debug menu items
    void OnSetSource(wxCommandEvent& event);
    void OnRunScript(wxCommandEvent& myEvent);

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

class WXDLLIMPEXP_WEBKIT wxPageSourceViewFrame : public wxFrame
{
public:
    wxPageSourceViewFrame(const wxString& source);
};

#endif // ifndef WXWEBBROWSERSHELL_H
