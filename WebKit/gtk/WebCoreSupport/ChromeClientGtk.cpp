/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
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
#include "ChromeClientGtk.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "CString.h"
#include "TextEncoding.h"
#include "webkitgtkpage.h"
#include "webkitgtkprivate.h"
#include "NotImplemented.h"

using namespace WebCore;

namespace WebKitGtk {
ChromeClientGtk::ChromeClientGtk(WebKitGtkPage* page)
    : m_webPage(page)
{
}

void ChromeClientGtk::chromeDestroyed()
{
    notImplemented();
}

FloatRect ChromeClientGtk::windowRect()
{
    notImplemented();
    return FloatRect();
}

void ChromeClientGtk::setWindowRect(const FloatRect& r)
{
    notImplemented();
}

FloatRect ChromeClientGtk::pageRect()
{
    notImplemented();
    return FloatRect();
}

float ChromeClientGtk::scaleFactor()
{
    notImplemented();
    return 1.0;
}

void ChromeClientGtk::focus()
{
    notImplemented();
}

void ChromeClientGtk::unfocus()
{
    notImplemented();
}
    
Page* ChromeClientGtk::createWindow(Frame*, const FrameLoadRequest&)
{
    /* TODO: FrameLoadRequest is not used */
    WebKitGtkPage* page = WEBKIT_GTK_PAGE_GET_CLASS(m_webPage)->create_page(m_webPage);
    if (!page)
        return 0;

    WebKitGtkPagePrivate *private_data = WEBKIT_GTK_PAGE_GET_PRIVATE(WEBKIT_GTK_PAGE(page));
    return private_data->page;
}

Page* ChromeClientGtk::createModalDialog(Frame*, const FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

void ChromeClientGtk::show()
{
    notImplemented();
}

bool ChromeClientGtk::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientGtk::runModal()
{
    notImplemented();
}

void ChromeClientGtk::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientGtk::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientGtk::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientGtk::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientGtk::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientGtk::scrollbarsVisible() {
    notImplemented();
    return false;
}

void ChromeClientGtk::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientGtk::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientGtk::setResizable(bool)
{
    notImplemented();
}

void ChromeClientGtk::closeWindowSoon()
{
    notImplemented();
}

bool ChromeClientGtk::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void ChromeClientGtk::takeFocus(FocusDirection)
{
    notImplemented();
}

bool ChromeClientGtk::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClientGtk::runBeforeUnloadConfirmPanel(const WebCore::String&, WebCore::Frame*)
{
    notImplemented();
    return false;
}

void ChromeClientGtk::addMessageToConsole(const WebCore::String& message, unsigned int lineNumber, const WebCore::String& sourceId)
{
    CString messageString = message.utf8();
    CString sourceIdString = sourceId.utf8();

    WEBKIT_GTK_PAGE_GET_CLASS(m_webPage)->java_script_console_message(m_webPage, messageString.data(), lineNumber, sourceIdString.data());
}

void ChromeClientGtk::runJavaScriptAlert(Frame* frame, const String& message)
{
    CString messageString = message.utf8();
    WEBKIT_GTK_PAGE_GET_CLASS(m_webPage)->java_script_alert(m_webPage, kit(frame), messageString.data());
}

bool ChromeClientGtk::runJavaScriptConfirm(Frame* frame, const String& message)
{
    CString messageString = message.utf8();
    return WEBKIT_GTK_PAGE_GET_CLASS(m_webPage)->java_script_confirm(m_webPage, kit(frame), messageString.data());
}

bool ChromeClientGtk::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    CString messageString = message.utf8();
    CString defaultValueString = defaultValue.utf8(); 

    gchar* cresult = WEBKIT_GTK_PAGE_GET_CLASS(m_webPage)->java_script_prompt(m_webPage,
                                                                              kit(frame),
                                                                              messageString.data(),
                                                                              defaultValueString.data());
    if (!cresult)
        return false;
    else {
        result = UTF8Encoding().decode(cresult, strlen(cresult));
        g_free(cresult);
        return true;
    }
}

void ChromeClientGtk::setStatusbarText(const String& string)
{
    CString stringMessage = string.utf8();
    g_signal_emit_by_name(m_webPage, "status_bar_text_changed", stringMessage.data());
}

bool ChromeClientGtk::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClientGtk::tabsToLinks() const
{
    notImplemented();
    return false;
}

IntRect ChromeClientGtk::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

void ChromeClientGtk::addToDirtyRegion(const IntRect&)
{
    notImplemented();
}

void ChromeClientGtk::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    notImplemented();
}

void ChromeClientGtk::updateBackingStore()
{
    notImplemented();
}

void ChromeClientGtk::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags)
{
    notImplemented();
}

void ChromeClientGtk::setToolTip(const String&)
{
    notImplemented();
}

void ChromeClientGtk::print(Frame*)
{
    notImplemented();
}
}
