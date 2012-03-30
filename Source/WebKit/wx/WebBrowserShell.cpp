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
 *
 * This class provides a default new window implementation for WebView clients
 * who don't want/need to roll their own browser frame UI.
 */
 
#include "config.h"

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/artprov.h"

#include "WebBrowserShell.h"
#include "WebFrame.h"
#include "WebView.h"
#include "WebViewPrivate.h"

namespace WebKit {

PageSourceViewFrame::PageSourceViewFrame(const wxString& source)
        : wxFrame(NULL, wxID_ANY, _("Page Source View"), wxDefaultPosition, wxSize(600, 500))
{
    wxTextCtrl* control = new wxTextCtrl(this, -1, source, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
}

enum {
    ID_LOADFILE = wxID_HIGHEST + 1,
    ID_TEXTCTRL = wxID_HIGHEST + 2,
    ID_BACK = wxID_HIGHEST + 3,
    ID_FORWARD = wxID_HIGHEST + 4,
    ID_TOGGLE_BEFORE_LOAD = wxID_HIGHEST + 5,
    ID_MAKE_TEXT_LARGER = wxID_HIGHEST + 6,
    ID_MAKE_TEXT_SMALLER = wxID_HIGHEST + 7,
    ID_STOP = wxID_HIGHEST + 8,
    ID_RELOAD = wxID_HIGHEST + 9,
    ID_GET_SOURCE = wxID_HIGHEST + 10,
    ID_SET_SOURCE = wxID_HIGHEST + 11,
    ID_SEARCHCTRL = wxID_HIGHEST + 12,
    ID_LOADURL = wxID_HIGHEST + 13,
    ID_NEW_WINDOW = wxID_HIGHEST + 14,
    ID_BROWSE = wxID_HIGHEST + 15,
    ID_EDIT = wxID_HIGHEST + 16,
    ID_RUN_SCRIPT = wxID_HIGHEST + 17,
    ID_WEBVIEW = wxID_HIGHEST + 18,
    ID_EDIT_COMMAND = wxID_HIGHEST + 19,
    ID_GET_EDIT_COMMAND_STATE = wxID_HIGHEST + 20
};

BEGIN_EVENT_TABLE(WebBrowserShell, wxFrame)
    EVT_MENU(wxID_CUT, WebBrowserShell::OnCut)
    EVT_MENU(wxID_COPY, WebBrowserShell::OnCopy)
    EVT_MENU(wxID_PASTE, WebBrowserShell::OnPaste)
    EVT_MENU(wxID_EXIT,  WebBrowserShell::OnQuit)
    EVT_MENU(wxID_ABOUT, WebBrowserShell::OnAbout)
    EVT_MENU(ID_LOADFILE, WebBrowserShell::OnLoadFile)
    EVT_TEXT_ENTER(ID_TEXTCTRL, WebBrowserShell::OnAddressBarEnter)
    EVT_TEXT_ENTER(ID_SEARCHCTRL, WebBrowserShell::OnSearchCtrlEnter)
    EVT_WEBVIEW_LOAD(ID_WEBVIEW, WebBrowserShell::OnLoadEvent)
    EVT_WEBVIEW_BEFORE_LOAD(ID_WEBVIEW, WebBrowserShell::OnBeforeLoad)
    EVT_MENU(ID_BACK, WebBrowserShell::OnBack)
    EVT_MENU(ID_FORWARD, WebBrowserShell::OnForward)
    EVT_MENU(ID_STOP, WebBrowserShell::OnStop)
    EVT_MENU(ID_RELOAD, WebBrowserShell::OnReload)
    EVT_MENU(ID_MAKE_TEXT_LARGER, WebBrowserShell::OnMakeTextLarger)
    EVT_MENU(ID_MAKE_TEXT_SMALLER, WebBrowserShell::OnMakeTextSmaller)
    EVT_MENU(ID_GET_SOURCE, WebBrowserShell::OnGetSource)
    EVT_MENU(ID_SET_SOURCE, WebBrowserShell::OnSetSource)
    EVT_MENU(ID_BROWSE, WebBrowserShell::OnBrowse)
    EVT_MENU(ID_EDIT, WebBrowserShell::OnEdit)
    EVT_MENU(ID_RUN_SCRIPT, WebBrowserShell::OnRunScript)
    EVT_MENU(ID_EDIT_COMMAND, WebBrowserShell::OnEditCommand)
    EVT_MENU(ID_GET_EDIT_COMMAND_STATE, WebBrowserShell::OnGetEditCommandState)
    EVT_MENU(wxID_PRINT, WebBrowserShell::OnPrint)
END_EVENT_TABLE()


WebBrowserShell::WebBrowserShell(const wxString& title, const wxString& url) : 
        wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(600, 500)),
        m_checkBeforeLoad(false)
{
    // create a menu bar
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(ID_NEW_WINDOW, _T("New Window\tCTRL+N"));
    fileMenu->Append(ID_LOADFILE, _T("Open File...\tCTRL+O"));
    fileMenu->Append(ID_LOADURL, _("Open Location...\tCTRL+L"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_PRINT, _("Print..."));
    fileMenu->Append(wxID_EXIT, _T("E&xit\tAlt-X"), _T("Quit this program"));
    
    wxMenu *editMenu = new wxMenu;
    editMenu->Append(wxID_CUT, _T("Cut\tCTRL+X"));
    editMenu->Append(wxID_COPY, _T("Copy\tCTRL+C"));
    editMenu->Append(wxID_PASTE, _T("Paste\tCTRL+V"));

    wxMenu* viewMenu = new wxMenu;
    viewMenu->AppendRadioItem(ID_BROWSE, _("Browse"));
    viewMenu->AppendRadioItem(ID_EDIT, _("Edit"));
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_STOP, _("Stop"));
    viewMenu->Append(ID_RELOAD, _("Reload Page"));
    viewMenu->Append(ID_MAKE_TEXT_SMALLER, _("Make Text Smaller\tCTRL+-"));
    viewMenu->Append(ID_MAKE_TEXT_LARGER, _("Make Text Bigger\tCTRL++"));
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_GET_SOURCE, _("View Page Source"));
    viewMenu->AppendSeparator();
    
    m_debugMenu = new wxMenu;
    m_debugMenu->SetTitle(_("&Debug"));
    m_debugMenu->Append(ID_SET_SOURCE, _("Test SetPageSource"));
    m_debugMenu->Append(ID_RUN_SCRIPT, _("Test RunScript"));
    m_debugMenu->Append(ID_EDIT_COMMAND, _("Test EditCommand::Execute"));
    m_debugMenu->Append(ID_GET_EDIT_COMMAND_STATE, _("Test EditCommand::GetState"));

    // the "About" item should be in the help menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, _T("&About...\tF1"), _T("Show about dialog"));

    // now append the freshly created menu to the menu bar...
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, _T("&File"));
    menuBar->Append(editMenu, _T("&Edit"));
    menuBar->Append(viewMenu, _T("&View"));
    menuBar->Append(helpMenu, _T("&Help"));

    // ... and attach this menu bar to the frame
    SetMenuBar(menuBar);
    
    wxToolBar* toolbar = CreateToolBar();
    toolbar->SetToolBitmapSize(wxSize(32, 32));
    
    wxBitmap back = wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR, wxSize(32,32));
    toolbar->AddTool(ID_BACK, back, wxT("Back"));
    
    wxBitmap forward = wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR, wxSize(32,32));
    toolbar->AddTool(ID_FORWARD, forward, wxT("Next"));

    addressBar = new wxTextCtrl(toolbar, ID_TEXTCTRL, _T(""), wxDefaultPosition, wxSize(400, -1), wxTE_PROCESS_ENTER);
    toolbar->AddControl(addressBar);
    
    searchCtrl = new wxSearchCtrl(toolbar, ID_SEARCHCTRL, _("Search"), wxDefaultPosition, wxSize(200, -1), wxTE_PROCESS_ENTER);
    toolbar->AddControl(searchCtrl);
    toolbar->Realize();
    
    SetToolBar(toolbar);

    // Create the WebView Window
    webview = new WebView((wxWindow*)this, url, ID_WEBVIEW, wxDefaultPosition, wxSize(200, 200));
    webview->SetBackgroundColour(*wxWHITE);

    // create a status bar just for fun (by default with 1 pane only)
    CreateStatusBar(2);
}

WebBrowserShell::~WebBrowserShell()
{
    if (m_debugMenu && GetMenuBar()->FindMenu(_("&Debug")) == wxNOT_FOUND)
        delete m_debugMenu;
}

void WebBrowserShell::ShowDebugMenu(bool show)
{
    int debugMenu = GetMenuBar()->FindMenu(_("&Debug"));
    if (show && debugMenu == wxNOT_FOUND) {
        int prevMenu = GetMenuBar()->FindMenu(_("&View"));
        if (prevMenu != wxNOT_FOUND)
            GetMenuBar()->Insert((size_t)prevMenu+1, m_debugMenu, _("&Debug"));
    }
    else if (!show && debugMenu != wxNOT_FOUND) {
        GetMenuBar()->Remove(debugMenu);
    }
}

// event handlers

void WebBrowserShell::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    // true is to force the frame to close
    Close(true);
}

void WebBrowserShell::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxString msg;
    msg.Printf(_T("This is the About dialog of the WebKit sample.\n")
               _T("Welcome to %s"), wxVERSION_STRING);

    wxMessageBox(msg, _T("About WebKit Sample"), wxOK | wxICON_INFORMATION, this);

}

void WebBrowserShell::OnLoadFile(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog* dialog = new wxFileDialog(this, wxT("Choose a file"));
    if (dialog->ShowModal() == wxID_OK) {  
        wxString path = dialog->GetPath().Prepend(wxT("file://"));
        
        if (webview)
            webview->LoadURL(path);
    }
}

void WebBrowserShell::OnLoadEvent(WebViewLoadEvent& event)
{
    if (GetStatusBar() != NULL){
        if (event.GetState() == WEBVIEW_LOAD_NEGOTIATING) {
            GetStatusBar()->SetStatusText(_("Contacting ") + event.GetURL());
        } else if (event.GetState() == WEBVIEW_LOAD_TRANSFERRING) {
            GetStatusBar()->SetStatusText(_("Loading ") + event.GetURL());
            if (event.GetFrame() == webview->GetMainFrame())
                addressBar->SetValue(event.GetURL());
        } else if (event.GetState() == WEBVIEW_LOAD_ONLOAD_HANDLED) {
            GetStatusBar()->SetStatusText(_("Load complete."));
            if (event.GetFrame() == webview->GetMainFrame())
                SetTitle(webview->GetPageTitle());
        } else if (event.GetState() == WEBVIEW_LOAD_FAILED) {
            GetStatusBar()->SetStatusText(_("Failed to load ") + event.GetURL());
        }
    }
}

void WebBrowserShell::OnBeforeLoad(WebViewBeforeLoadEvent& myEvent)
{
    if (m_checkBeforeLoad) {
        int reply = wxMessageBox(_("Would you like to continue loading ") + myEvent.GetURL() + wxT("?"), _("Continue Loading?"), wxYES_NO); 
        if (reply == wxNO) {
            myEvent.Cancel();
        }
    }
}

void WebBrowserShell::OnAddressBarEnter(wxCommandEvent& event)
{
    if (webview)
        webview->LoadURL(addressBar->GetValue());
}

void WebBrowserShell::OnSearchCtrlEnter(wxCommandEvent& event)
{
    if (webview) {
        webview->LoadURL(wxString::Format(wxT("http://www.google.com/search?rls=en&q=%s&ie=UTF-8&oe=UTF-8"), searchCtrl->GetValue().wc_str()));
    }
}

void WebBrowserShell::OnCut(wxCommandEvent& event)
{
    if (webview && webview->CanCut())
        webview->Cut();
}

void WebBrowserShell::OnCopy(wxCommandEvent& event)
{
    if (webview && webview->CanCopy())
        webview->Copy();
}

void WebBrowserShell::OnPaste(wxCommandEvent& event)
{
    if (webview && webview->CanPaste())
        webview->Paste();
}

void WebBrowserShell::OnBack(wxCommandEvent& event)
{
    if (webview)
        webview->GoBack();
}

void WebBrowserShell::OnForward(wxCommandEvent& event)
{
    if (webview)
        webview->GoForward();
}

void WebBrowserShell::OnStop(wxCommandEvent& myEvent)
{
    if (webview)
        webview->Stop();
}

void WebBrowserShell::OnReload(wxCommandEvent& myEvent)
{
    if (webview)
        webview->Reload();
}

void WebBrowserShell::OnMakeTextLarger(wxCommandEvent& myEvent)
{
    if (webview) {
        if (webview->CanIncreaseTextSize())
            webview->IncreaseTextSize();
    }
}

void WebBrowserShell::OnMakeTextSmaller(wxCommandEvent& myEvent)
{
    if (webview) {
        if (webview->CanDecreaseTextSize())
            webview->DecreaseTextSize();
    }
}

void WebBrowserShell::OnGetSource(wxCommandEvent& myEvent)
{
    if (webview) {
        PageSourceViewFrame* pageSourceFrame = new PageSourceViewFrame(webview->GetPageSource());
        pageSourceFrame->Show();
    }
}

void WebBrowserShell::OnSetSource(wxCommandEvent& event)
{
    if (webview)
        webview->SetPageSource(wxString(wxT("<p>Hello World!</p>")));
}

void WebBrowserShell::OnBrowse(wxCommandEvent& event)
{
    if (webview)
        webview->MakeEditable(!event.IsChecked());
}

void WebBrowserShell::OnEdit(wxCommandEvent& event)
{
    if (webview)
        webview->MakeEditable(event.IsChecked());
}

void WebBrowserShell::OnRunScript(wxCommandEvent& myEvent)
{
    if (webview) {
        wxTextEntryDialog* dialog = new wxTextEntryDialog(this, _("Type in a JavaScript to exectute."));
        if (dialog->ShowModal() == wxID_OK)
            wxMessageBox(wxT("Result is: ") + webview->RunScript(dialog->GetValue()));
    
        dialog->Destroy();
    }
}

void WebBrowserShell::OnEditCommand(wxCommandEvent& myEvent)
{
    if (webview) {
        if (!webview->IsEditable()) {
            wxMessageBox(wxT("Please enable editing before running editing commands."));
            return;
        }
        
        wxTextEntryDialog* dialog = new wxTextEntryDialog(this, _("Type in a editing command to exectute."));
        if (dialog->ShowModal() == wxID_OK) {
            bool result = webview->ExecuteEditCommand(dialog->GetValue());
            if (!result)
                wxMessageBox(wxT("Editing command failed."));
        }
        dialog->Destroy();
    }
}

void WebBrowserShell::OnGetEditCommandState(wxCommandEvent& myEvent)
{
    if (webview) {
        if (!webview->IsEditable()) {
            wxMessageBox(wxT("Please enable editing before running editing commands."));
            return;
        }
        
        wxTextEntryDialog* dialog = new wxTextEntryDialog(this, _("Type in a editing command whose state you want to get."));
        if (dialog->ShowModal() == wxID_OK) {
            EditState result = webview->GetEditCommandState(dialog->GetValue());
            if (result == EditStateTrue)
                wxMessageBox(wxT("State is true."));
            else if (result == EditStateFalse)
                wxMessageBox(wxT("State is false."));
            else if (result == EditStateMixed)
                wxMessageBox(wxT("State is mixed."));
        }
        dialog->Destroy();
    }
}

void WebBrowserShell::OnPrint(wxCommandEvent& myEvent)
{
    if (webview && webview->GetMainFrame())
        webview->GetMainFrame()->Print();
}
    
}
