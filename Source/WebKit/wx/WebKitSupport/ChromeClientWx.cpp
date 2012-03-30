/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#include "ChromeClientWx.h"
#include "Console.h"
#if ENABLE(SQL_DATABASE)
#include "DatabaseTracker.h"
#endif
#include "FileChooser.h"
#include "FileIconLoader.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "Icon.h"
#include "NavigationAction.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "PopupMenuWx.h"
#include "SearchPopupMenuWx.h"
#include "WindowFeatures.h"

#include <stdio.h>

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/textdlg.h>
#include <wx/tooltip.h>

#include "WebBrowserShell.h"
#include "WebView.h"
#include "WebViewPrivate.h"

namespace WebCore {

WebKitWindowFeatures wkFeaturesforWindowFeatures(const WindowFeatures& features)
{
    WebKitWindowFeatures wkFeatures;
    wkFeatures.menuBarVisible = features.menuBarVisible;
    wkFeatures.statusBarVisible = features.statusBarVisible;
    wkFeatures.toolBarVisible = features.toolBarVisible;
    wkFeatures.locationBarVisible = features.locationBarVisible;
    wkFeatures.scrollbarsVisible = features.scrollbarsVisible;
    wkFeatures.resizable = features.resizable;
    wkFeatures.fullscreen = features.fullscreen;
    wkFeatures.dialog = features.dialog;
    
    return wkFeatures;
}

ChromeClientWx::ChromeClientWx(WebView* webView)
{
    m_webView = webView;
}

ChromeClientWx::~ChromeClientWx()
{
}

void ChromeClientWx::chromeDestroyed()
{
    notImplemented();
}

void ChromeClientWx::setWindowRect(const FloatRect&)
{
    notImplemented();
}

FloatRect ChromeClientWx::windowRect()
{
    notImplemented();
    return FloatRect();
}

FloatRect ChromeClientWx::pageRect()
{
    notImplemented();
    return FloatRect();
}

void ChromeClientWx::focus()
{
    notImplemented();
}

void ChromeClientWx::unfocus()
{
    notImplemented();
}

bool ChromeClientWx::canTakeFocus(FocusDirection)
{
    notImplemented();
    return false;
}

void ChromeClientWx::takeFocus(FocusDirection)
{
    notImplemented();
}

void ChromeClientWx::focusedNodeChanged(Node*)
{
}

void ChromeClientWx::focusedFrameChanged(Frame*)
{
}

Page* ChromeClientWx::createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures& features, const NavigationAction&)
{
    Page* myPage = 0;
    WebViewNewWindowEvent wkEvent(m_webView);
    
    WebKitWindowFeatures wkFeatures = wkFeaturesforWindowFeatures(features);
    wkEvent.SetWindowFeatures(wkFeatures);
    
    if (m_webView->GetEventHandler()->ProcessEvent(wkEvent)) {
        if (WebView* webView = wkEvent.GetWebView()) {
            WebViewPrivate* impl = webView->m_impl;
            if (impl)
                myPage = impl->page;
        }
    }
    
    return myPage;
}

Page* ChromeClientWx::createModalDialog(Frame*, const FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

void ChromeClientWx::show()
{
    notImplemented();
}

bool ChromeClientWx::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientWx::runModal()
{
    notImplemented();
}

void ChromeClientWx::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setResizable(bool)
{
    notImplemented();
}

void ChromeClientWx::addMessageToConsole(MessageSource source,
                                          MessageType type,
                                          MessageLevel level,
                                          const String& message,
                                          unsigned int lineNumber,
                                          const String& sourceID)
{
    if (m_webView) {
        WebViewConsoleMessageEvent wkEvent(m_webView);
        wkEvent.SetMessage(message);
        wkEvent.SetLineNumber(lineNumber);
        wkEvent.SetSourceID(sourceID);
        wkEvent.SetLevel(static_cast<WebViewConsoleMessageLevel>(level));
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

bool ChromeClientWx::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return true;
}

bool ChromeClientWx::runBeforeUnloadConfirmPanel(const String& string,
                                                  Frame* frame)
{
    wxMessageDialog dialog(NULL, string, wxT("Confirm Action?"), wxYES_NO);
    return dialog.ShowModal() == wxYES;
}

void ChromeClientWx::closeWindowSoon()
{
    notImplemented();
}

/*
    Sites for testing prompts: 
    Alert - just type in a bad web address or http://www.htmlite.com/JS002.php
    Prompt - http://www.htmlite.com/JS007.php
    Confirm - http://www.htmlite.com/JS006.php
*/

void ChromeClientWx::runJavaScriptAlert(Frame* frame, const String& string)
{
    if (m_webView) {
        WebViewAlertEvent wkEvent(m_webView);
        wkEvent.SetMessage(string);
        if (!m_webView->GetEventHandler()->ProcessEvent(wkEvent))
            wxMessageBox(string, wxT("JavaScript Alert"), wxOK);
    }
}

bool ChromeClientWx::runJavaScriptConfirm(Frame* frame, const String& string)
{
    bool result = false;
    if (m_webView) {
        WebViewConfirmEvent wkEvent(m_webView);
        wkEvent.SetMessage(string);
        if (m_webView->GetEventHandler()->ProcessEvent(wkEvent))
            result = wkEvent.GetReturnCode() == wxID_YES;
        else {
            wxMessageDialog dialog(NULL, string, wxT("JavaScript Confirm"), wxYES_NO);
            dialog.Centre();
            result = (dialog.ShowModal() == wxID_YES);
        }
    }
    return result;
}

bool ChromeClientWx::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    if (m_webView) {
        WebViewPromptEvent wkEvent(m_webView);
        wkEvent.SetMessage(message);
        wkEvent.SetResponse(defaultValue);
        if (m_webView->GetEventHandler()->ProcessEvent(wkEvent)) {
            result = wkEvent.GetResponse();
            return true;
        }
        else {
            wxTextEntryDialog dialog(NULL, message, wxT("JavaScript Prompt"), wxEmptyString, wxOK | wxCANCEL);
            dialog.Centre();
            if (dialog.ShowModal() == wxID_OK) {
                result = dialog.GetValue();
                return true;
            }
        }
    }
    return false;
}

void ChromeClientWx::setStatusbarText(const String&)
{
    notImplemented();
}

bool ChromeClientWx::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

KeyboardUIMode ChromeClientWx::keyboardUIMode()
{
    notImplemented();
    return KeyboardAccessDefault;
}

IntRect ChromeClientWx::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

void ChromeClientWx::invalidateRootView(const IntRect& rect, bool immediate)
{
    if (immediate)
        m_webView->Update();
}

void ChromeClientWx::invalidateContentsForSlowScroll(const IntRect& rect, bool immediate)
{
    invalidateContentsAndRootView(rect, immediate);
}

void ChromeClientWx::invalidateContentsAndRootView(const IntRect& rect, bool immediate)
{
    if (!m_webView)
        return;

    m_webView->RefreshRect(rect);

    if (immediate) {
        m_webView->Update();
    }
}

IntRect ChromeClientWx::rootViewToScreen(const IntRect& rect) const
{
    notImplemented();
    return rect;
}

IntPoint ChromeClientWx::screenToRootView(const IntPoint& point) const
{
    notImplemented();
    return point;
}

PlatformPageClient ChromeClientWx::platformPageClient() const
{
    return m_webView;
}

void ChromeClientWx::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

void ChromeClientWx::scrollBackingStore(int dx, int dy, 
                    const IntRect& scrollViewRect, 
                    const IntRect& clipRect)
{
    notImplemented();
}

void ChromeClientWx::updateBackingStore()
{
    notImplemented();
}

void ChromeClientWx::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags)
{
    notImplemented();
}

void ChromeClientWx::setToolTip(const String& tip, TextDirection)
{
    wxToolTip* tooltip = m_webView->GetToolTip();
    if (!tooltip || tooltip->GetTip() != wxString(tip))
        m_webView->SetToolTip(tip);
}

void ChromeClientWx::print(Frame* frame)
{
    WebFrame* webFrame = kit(frame);
    if (webFrame) {
        WebViewPrintFrameEvent event(m_webView);
        event.SetWebFrame(webFrame);
        if (!m_webView->GetEventHandler()->ProcessEvent(event))
            webFrame->Print(true);
    }
}

#if ENABLE(SQL_DATABASE)
void ChromeClientWx::exceededDatabaseQuota(Frame*, const String&)
{
    unsigned long long quota = 5 * 1024 * 1024;

    if (WebFrame* webFrame = m_webView->GetMainFrame())
        if (Frame* frame = webFrame->GetFrame())
            if (Document* document = frame->document())
                if (!DatabaseTracker::tracker().hasEntryForOrigin(document->securityOrigin()))
                    DatabaseTracker::tracker().setQuota(document->securityOrigin(), quota);
}
#endif

void ChromeClientWx::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    notImplemented();
}

void ChromeClientWx::reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t)
{
    notImplemented();
}

void ChromeClientWx::scroll(const IntSize&, const IntRect&, const IntRect&)
{
    m_webView->Refresh();
    notImplemented();
}

void ChromeClientWx::runOpenPanel(Frame*, PassRefPtr<FileChooser>)
{
    notImplemented();
}

void ChromeClientWx::loadIconForFiles(const Vector<String>& filenames, FileIconLoader* loader)
{
    loader->notifyFinished(Icon::createIconForFiles(filenames));
}

void ChromeClientWx::setCursor(const Cursor& cursor)
{
    if (m_webView && cursor.impl())
        m_webView->SetCursor(*cursor.impl());
}

bool ChromeClientWx::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool ChromeClientWx::selectItemAlignmentFollowsMenuWritingDirection()
{
    return false;
}

PassRefPtr<PopupMenu> ChromeClientWx::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuWx(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientWx::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuWx(client));
}
    
bool ChromeClientWx::hasOpenedPopup() const
{
    notImplemented();
    return false;
}

}
