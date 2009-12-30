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

%module webview

%{
#include "wx/wxPython/wxPython.h"
#include "wx/wxPython/pyclasses.h"

#include "WebBrowserShell.h"
#include "WebFrame.h"
#include "WebKitDefines.h"
#include "WebSettings.h"
#include "WebView.h"
%}
//---------------------------------------------------------------------------

%import core.i
%import windows.i

MAKE_CONST_WXSTRING(WebViewNameStr);

MustHaveApp(wxWebBrowserShell);
MustHaveApp(wxWebFrame);
MustHaveApp(wxWebView);

%include WebKitDefines.h

%include WebBrowserShell.h
%include WebFrame.h
%include WebSettings.h
%include WebView.h

%constant wxEventType wxEVT_WEBVIEW_BEFORE_LOAD;
%constant wxEventType wxEVT_WEBVIEW_LOAD;
%constant wxEventType wxEVT_WEBVIEW_NEW_WINDOW;
%constant wxEventType wxEVT_WEBVIEW_RIGHT_CLICK;
%constant wxEventType wxEVT_WEBVIEW_CONSOLE_MESSAGE;
%constant wxEventType wxEVT_WEBVIEW_RECEIVED_TITLE;

%pythoncode {
EVT_WEBVIEW_BEFORE_LOAD = wx.PyEventBinder( wxEVT_WEBVIEW_BEFORE_LOAD, 1 )
EVT_WEBVIEW_LOAD = wx.PyEventBinder( wxEVT_WEBVIEW_LOAD, 1 )
EVT_WEBVIEW_NEW_WINDOW = wx.PyEventBinder( wxEVT_WEBVIEW_NEW_WINDOW, 1 )
EVT_WEBVIEW_RIGHT_CLICK = wx.PyEventBinder( wxEVT_WEBVIEW_RIGHT_CLICK, 1 )
EVT_WEBVIEW_CONSOLE_MESSAGE = wx.PyEventBinder( wxEVT_WEBVIEW_CONSOLE_MESSAGE, 1 )
EVT_WEBVIEW_RECEIVED_TITLE = wx.PyEventBinder( wxEVT_WEBVIEW_RECEIVED_TITLE, 1 )    
}
