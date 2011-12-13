/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com>
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
#include "DumpRenderTree.h"

#include "LayoutTestController.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"

#include <JavaScriptCore/JavaScript.h>

#include <wx/wx.h>
#include "WebView.h"
#include "WebFrame.h"
#include "WebBrowserShell.h"

#include <wtf/Assertions.h>

#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <time.h>

volatile bool done = true;
volatile bool notified = false;
static bool printSeparators = true;
static int dumpPixels;
static int dumpTree = 1;
time_t startTime; // to detect timeouts / failed tests

using namespace std;

FILE* logOutput;

RefPtr<LayoutTestController> gLayoutTestController;
static wxWebView* webView;
static wxTimer* idleTimer;

const unsigned timeOut = 10;
const unsigned maxViewHeight = 600;
const unsigned maxViewWidth = 800;

class LayoutWebViewEventHandler : public wxEvtHandler {

public:
    LayoutWebViewEventHandler(wxWebView* webView)
        : m_webView(webView)
    {
    }
    
    void bindEvents() 
    {
        m_webView->Connect(wxEVT_WEBVIEW_LOAD, wxWebViewLoadEventHandler(LayoutWebViewEventHandler::OnLoadEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_JS_ALERT, wxWebViewAlertEventHandler(LayoutWebViewEventHandler::OnAlertEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_JS_CONFIRM, wxWebViewConfirmEventHandler(LayoutWebViewEventHandler::OnConfirmEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_JS_PROMPT, wxWebViewPromptEventHandler(LayoutWebViewEventHandler::OnPromptEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_CONSOLE_MESSAGE, wxWebViewConsoleMessageEventHandler(LayoutWebViewEventHandler::OnConsoleMessageEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_RECEIVED_TITLE, wxWebViewReceivedTitleEventHandler(LayoutWebViewEventHandler::OnReceivedTitleEvent), NULL, this);
        m_webView->Connect(wxEVT_WEBVIEW_WINDOW_OBJECT_CLEARED, wxWebViewWindowObjectClearedEventHandler(LayoutWebViewEventHandler::OnWindowObjectClearedEvent), NULL, this);
    }
    
    void OnLoadEvent(wxWebViewLoadEvent& event) 
    {

        if (event.GetState() == wxWEBVIEW_LOAD_FAILED || event.GetState() == wxWEBVIEW_LOAD_STOPPED)
            done = true; 
        
        if (event.GetState() == wxWEBVIEW_LOAD_ONLOAD_HANDLED) {
            done = true;
            
            if (!gLayoutTestController->waitToDump() || notified) {
                dump();
            }
        }
    }
    
    void OnAlertEvent(wxWebViewAlertEvent& event)
    {
        fprintf(stdout, "ALERT: %S\n", event.GetMessage().c_str());
    }
    
    void OnConfirmEvent(wxWebViewConfirmEvent& event)
    {
        fprintf(stdout, "CONFIRM: %S\n", event.GetMessage().c_str());
        event.SetReturnCode(1);
    }
    
    void OnPromptEvent(wxWebViewPromptEvent& event)
    {
        fprintf(stdout, "PROMPT: %S, default text: %S\n", event.GetMessage().c_str(), event.GetResponse().c_str());
        event.SetReturnCode(1);
    }
    
    void OnConsoleMessageEvent(wxWebViewConsoleMessageEvent& event)
    {
        fprintf(stdout, "CONSOLE MESSAGE: line %d: %S\n", event.GetLineNumber(), event.GetMessage().c_str());
    }
    
    void OnReceivedTitleEvent(wxWebViewReceivedTitleEvent& event)
    {
        if (gLayoutTestController->dumpTitleChanges() && !done) {
            const char* title = event.GetTitle().mb_str(wxConvUTF8);
            printf("TITLE CHANGED: %S\n", title ? title : "");
        }
    }
    
    void OnWindowObjectClearedEvent(wxWebViewWindowObjectClearedEvent& event)
    {
        JSValueRef exception = 0;
        gLayoutTestController->makeWindowObject(event.GetJSContext(), event.GetWindowObject(), &exception);
    }
    
private:
    wxWebView* m_webView;

};

void notifyDoneFired() 
{
    notified = true;
    if (done)
        dump();
}

LayoutWebViewEventHandler* eventHandler = NULL;

static wxString dumpFramesAsText(wxWebFrame* frame)
{
    // TODO: implement this. leaving this here so we don't forget this case.
    if (gLayoutTestController->dumpChildFramesAsText()) {
    }
    
    return frame->GetInnerText();
}

void dump()
{    
    if (!done)
        return;
    
    if (gLayoutTestController->waitToDump() && !notified)
        return;
        
    if (dumpTree) {
        const char* result = 0;

        bool dumpAsText = gLayoutTestController->dumpAsText();
        wxString str;
        if (gLayoutTestController->dumpAsText())
            str = dumpFramesAsText(webView->GetMainFrame());
        else 
            str = webView->GetMainFrame()->GetExternalRepresentation();

        result = str.ToUTF8();
        if (!result) {
            const char* errorMessage;
            if (gLayoutTestController->dumpAsText())
                errorMessage = "WebFrame::GetInnerText";
            else
                errorMessage = "WebFrame::GetExternalRepresentation";
            printf("ERROR: NULL result from %s", errorMessage);
        } else {
            printf("%s\n", result);
        }

        if (gLayoutTestController->dumpBackForwardList()) {
            // FIXME: not implemented
        }

        if (printSeparators) {
            puts("#EOF");
            fputs("#EOF\n", stderr);
            fflush(stdout);
            fflush(stderr);
        }
    }

    if (dumpPixels
        && gLayoutTestController->generatePixelResults()
        && !gLayoutTestController->dumpDOMAsWebArchive()
        && !gLayoutTestController->dumpSourceAsWebArchive()) {
        // FIXME: Add support for dumping pixels
        fflush(stdout);
    }

    puts("#EOF");
    fflush(stdout);
    fflush(stderr);

    gLayoutTestController.clear();
}

static void runTest(const wxString testPathOrURL)
{
    done = false;
    time(&startTime);
    string pathOrURLString(testPathOrURL.char_str());
    string pathOrURL(pathOrURLString);
    string expectedPixelHash;

    size_t separatorPos = pathOrURL.find("'");
    if (separatorPos != string::npos) {
        pathOrURL = string(pathOrURLString, 0, separatorPos);
        expectedPixelHash = string(pathOrURLString, separatorPos + 1);
    }
    
    // CURL isn't happy if we don't have a protocol.
    size_t http = pathOrURL.find("http://");
    if (http == string::npos)
        pathOrURL.insert(0, "file://");
    
    gLayoutTestController = LayoutTestController::create(pathOrURL, expectedPixelHash);
    if (!gLayoutTestController) {
        wxTheApp->ExitMainLoop();
    }

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    webView->LoadURL(wxString(pathOrURL.c_str(), wxConvUTF8));
    
    // wait until load completes and the results are dumped
    while (!done)
        wxSafeYield();
}

class MyApp : public wxApp
{
public:

    virtual bool OnInit();
    
private:
    wxLog* logger;
};


IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
    logOutput = fopen("output.txt", "ab");
    if (logOutput) {
        logger = new wxLogStderr(logOutput);
        wxLog::SetActiveTarget(logger);
    }

    wxLogMessage(wxT("Starting DumpRenderTool, %d args.\n"), argc);

    for (int i = 1; i < argc; ++i) {
        wxString option = wxString(argv[i]);
        if (!option.CmpNoCase(_T("--notree"))) {
            dumpTree = false;
            continue;
        }
        
        if (!option.CmpNoCase(_T("--pixel-tests"))) {
            dumpPixels = true;
            continue;
        }
        
        if (!option.CmpNoCase(_T("--tree"))) {
            dumpTree = true;
            continue;
        }
    }
    wxInitAllImageHandlers();
        
    // create the main application window
    wxWebBrowserShell* webFrame = new wxWebBrowserShell(_T("wxWebKit DumpRenderTree App"), "about:blank");
    SetTopWindow(webFrame);
    webView = webFrame->webview;
    webView->SetSize(wxSize(maxViewWidth, maxViewHeight));
    
    if (!eventHandler) {
        eventHandler = new LayoutWebViewEventHandler(webView);
        eventHandler->bindEvents();
    }

    int optind = 1;
    time(&startTime);
    wxString option_str = wxString(argv[optind]);
    if (argc == optind+1 && option_str.Find(_T("-")) == 0) {
        char filenameBuffer[2048];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            wxString filename = wxString::FromUTF8(filenameBuffer);
            char* newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';

            if (strlen(filenameBuffer) == 0)
                return 0;
            wxLogMessage(wxT("Running test %S.\n"), filenameBuffer);
            runTest(filename);
        }
    
    } else {
        printSeparators = (optind < argc-1 || (dumpPixels && dumpTree));
        for (int i = optind; i != argc; ++i) {
            runTest(wxTheApp->argv[1]);
        }
    }
    
    webFrame->Close();
    delete eventHandler;

    wxLog::SetActiveTarget(NULL);
    delete logger;
    fclose(logOutput);
    
    // returning false shuts the app down
    return false;
}
