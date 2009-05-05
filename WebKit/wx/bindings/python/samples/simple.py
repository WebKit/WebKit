#!/usr/bin/python

# Copyright (C) 2007 Kevin Ollivier  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 

import wx
import wx.webview
        
class TestPanel(wx.Panel):
    def __init__(self, parent, log, frame=None):
        wx.Panel.__init__(
            self, parent, -1,
            style=wx.TAB_TRAVERSAL|wx.CLIP_CHILDREN|wx.NO_FULL_REPAINT_ON_RESIZE
            )
            
        self.log = log
        self.current = "http://wxPython.org/"
        self.frame = frame

        if frame:
            self.titleBase = frame.GetTitle()

        sizer = wx.BoxSizer(wx.VERTICAL)
        btnSizer = wx.BoxSizer(wx.HORIZONTAL)

        self.webview = wx.webview.WebView(self, -1)


        btn = wx.Button(self, -1, "Open", style=wx.BU_EXACTFIT)
        self.Bind(wx.EVT_BUTTON, self.OnOpenButton, btn)
        btnSizer.Add(btn, 0, wx.EXPAND|wx.ALL, 2)

        btn = wx.Button(self, -1, "<--", style=wx.BU_EXACTFIT)
        self.Bind(wx.EVT_BUTTON, self.OnPrevPageButton, btn)
        btnSizer.Add(btn, 0, wx.EXPAND|wx.ALL, 2)

        btn = wx.Button(self, -1, "-->", style=wx.BU_EXACTFIT)
        self.Bind(wx.EVT_BUTTON, self.OnNextPageButton, btn)
        btnSizer.Add(btn, 0, wx.EXPAND|wx.ALL, 2)

        btn = wx.Button(self, -1, "Stop", style=wx.BU_EXACTFIT)
        self.Bind(wx.EVT_BUTTON, self.OnStopButton, btn)
        btnSizer.Add(btn, 0, wx.EXPAND|wx.ALL, 2)

        btn = wx.Button(self, -1, "Refresh", style=wx.BU_EXACTFIT)
        self.Bind(wx.EVT_BUTTON, self.OnRefreshPageButton, btn)
        btnSizer.Add(btn, 0, wx.EXPAND|wx.ALL, 2)

        txt = wx.StaticText(self, -1, "Location:")
        btnSizer.Add(txt, 0, wx.CENTER|wx.ALL, 2)

        self.location = wx.ComboBox(
                            self, -1, "", style=wx.CB_DROPDOWN|wx.PROCESS_ENTER
                            )
        
        self.Bind(wx.EVT_COMBOBOX, self.OnLocationSelect, self.location)
        self.location.Bind(wx.EVT_KEY_UP, self.OnLocationKey)
        self.location.Bind(wx.EVT_CHAR, self.IgnoreReturn)
        btnSizer.Add(self.location, 1, wx.EXPAND|wx.ALL, 2)

        sizer.Add(btnSizer, 0, wx.EXPAND)
        sizer.Add(self.webview, 1, wx.EXPAND)

        self.webview.LoadURL(self.current)
        self.location.Append(self.current)
        
        self.webview.Bind(wx.webview.EVT_WEBVIEW_LOAD, self.OnStateChanged)

        self.SetSizer(sizer)

    def OnStateChanged(self, event):
        statusbar = self.GetParent().GetStatusBar() 
        if statusbar:
            if event.GetState() == wx.webview.WEBVIEW_STATE_NEGOTIATING:
                statusbar.SetStatusText("Contacting " + event.GetURL())
            elif event.GetState() == wx.webview.WEBVIEW_STATE_TRANSFERRING:
                statusbar.SetStatusText("Loading " + event.GetURL())
            elif event.GetState() == wx.webview.WEBVIEW_STATE_STOP:
                statusbar.SetStatusText("")
                self.location.SetValue(event.GetURL())
                self.GetParent().SetTitle("wxWebView - " + self.webview.GetPageTitle())

    def OnLocationKey(self, evt):
        if evt.GetKeyCode() == wx.WXK_RETURN:
            URL = self.location.GetValue()
            self.location.Append(URL)
            self.webview.LoadURL(URL)
        else:
            evt.Skip()
            
    def IgnoreReturn(self, evt):
        if evt.GetKeyCode() != wx.WXK_RETURN:
            evt.Skip()
            
    def OnLocationSelect(self, evt):
        url = self.location.GetStringSelection()
        self.webview.LoadURL(url)

    def OnOpenButton(self, event):
        dlg = wx.TextEntryDialog(self, "Open Location",
                                "Enter a full URL or local path",
                                self.current, wx.OK|wx.CANCEL)
        dlg.CentreOnParent()

        if dlg.ShowModal() == wx.ID_OK:
            self.current = dlg.GetValue()
            self.webview.LoadURL(self.current)

        dlg.Destroy()

    def OnPrevPageButton(self, event):
        self.webview.GoBack()

    def OnNextPageButton(self, event):
        self.webview.GoForward()

    def OnStopButton(self, evt):
        self.webview.Stop()

    def OnRefreshPageButton(self, evt):
        self.webview.Reload()
        

class wkFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self, None, -1, "WebKit in wxPython!")
        
        self.panel = TestPanel(self, -1)
        self.panel.webview.LoadURL("http://www.wxwidgets.org/")
        self.CreateStatusBar()

class wkApp(wx.App):
    def OnInit(self):
        self.webFrame = wkFrame()
        self.SetTopWindow(self.webFrame)
        self.webFrame.Show()
    
        return True
        
app = wkApp(redirect=False)
app.MainLoop()
