/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
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
#include "ChromeClientHaiku.h"

#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "Icon.h"
#include "NavigationAction.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "SecurityOrigin.h"
#include "PopupMenuHaiku.h"
#include "SearchPopupMenuHaiku.h"

#include <Alert.h>
#include <String.h>


namespace WebCore {

ChromeClientHaiku::ChromeClientHaiku()
{
}

ChromeClientHaiku::~ChromeClientHaiku()
{
}

void ChromeClientHaiku::chromeDestroyed()
{
    notImplemented();
}

void ChromeClientHaiku::setWindowRect(const FloatRect&)
{
    notImplemented();
}

FloatRect ChromeClientHaiku::windowRect()
{
    notImplemented();
    return FloatRect(0, 0, 200, 200);
}

FloatRect ChromeClientHaiku::pageRect()
{
    notImplemented();
    return FloatRect(0, 0, 200, 200);
}

float ChromeClientHaiku::scaleFactor()
{
    notImplemented();
    return 1.0;
}

void ChromeClientHaiku::focus()
{
    notImplemented();
}

void ChromeClientHaiku::unfocus()
{
    notImplemented();
}

bool ChromeClientHaiku::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void ChromeClientHaiku::takeFocus(FocusDirection)
{
    notImplemented();
}

void ChromeClientHaiku::focusedNodeChanged(Node*)
{
}

void ChromeClientHaiku::focusedFrameChanged(Frame*)
{
}

Page* ChromeClientHaiku::createWindow(Frame*, const FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&)
{
    notImplemented();
    return 0;
}

Page* ChromeClientHaiku::createModalDialog(Frame*, const FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

void ChromeClientHaiku::show()
{
    notImplemented();
}

bool ChromeClientHaiku::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::runModal()
{
    notImplemented();
}

void ChromeClientHaiku::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientHaiku::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientHaiku::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientHaiku::scrollbarsVisible()
{
    notImplemented();
    return true;
}

void ChromeClientHaiku::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientHaiku::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::setResizable(bool)
{
    notImplemented();
}

void ChromeClientHaiku::addMessageToConsole(const String& message, unsigned int lineNumber,
                                            const String& sourceID)
{
    printf("MESSAGE %s:%i %s\n", BString(sourceID).String(), lineNumber, BString(message).String());
}

void ChromeClientHaiku::addMessageToConsole(MessageSource, MessageLevel, const String& message,
                                            unsigned int lineNumber, const String& sourceID)
{
    printf("MESSAGE %s:%i %s\n", BString(sourceID).String(), lineNumber, BString(message).String());
}

void ChromeClientHaiku::addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message,
                                            unsigned int lineNumber, const String& sourceID)
{
    printf("MESSAGE %s:%i %s\n", BString(sourceID).String(), lineNumber, BString(message).String());
}

bool ChromeClientHaiku::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClientHaiku::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::closeWindowSoon()
{
    notImplemented();
}

void ChromeClientHaiku::runJavaScriptAlert(Frame*, const String& msg)
{
    BAlert* alert = new BAlert("JavaScript", BString(msg).String(), "OK");
    alert->Go();
}

bool ChromeClientHaiku::runJavaScriptConfirm(Frame*, const String& msg)
{
    BAlert* alert = new BAlert("JavaScript", BString(msg).String(), "Yes", "No");
    return !alert->Go();
}

bool ChromeClientHaiku::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    notImplemented();
    return false;
}

void ChromeClientHaiku::setStatusbarText(const String&)
{
    notImplemented();
}

bool ChromeClientHaiku::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClientHaiku::tabsToLinks() const
{
    return false;
}

IntRect ChromeClientHaiku::windowResizerRect() const
{
    return IntRect();
}

void ChromeClientHaiku::invalidateWindow(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientHaiku::invalidateContentsAndWindow(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientHaiku::invalidateContentsForSlowScroll(const IntRect&, bool)
{
    notImplemented();
}

void ChromeClientHaiku::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    notImplemented();
}

IntPoint ChromeClientHaiku::screenToWindow(const IntPoint&) const
{
    notImplemented();
    return IntPoint();
}

IntRect ChromeClientHaiku::windowToScreen(const IntRect&) const
{
    notImplemented();
    return IntRect();
}

PlatformPageClient ChromeClientHaiku::platformPageClient() const
{
    notImplemented();
    return PlatformWidget();
}

void ChromeClientHaiku::contentsSizeChanged(Frame*, const IntSize&) const
{
    notImplemented();
}

void ChromeClientHaiku::scrollRectIntoView(const IntRect&, const ScrollView*) const
{
    notImplemented();
}

void ChromeClientHaiku::addToDirtyRegion(const IntRect&)
{
}

void ChromeClientHaiku::scrollBackingStore(int, int, const IntRect&, const IntRect&)
{
}

void ChromeClientHaiku::updateBackingStore()
{
}

void ChromeClientHaiku::mouseDidMoveOverElement(const HitTestResult& hit, unsigned /*modifierFlags*/)
{
    // Some extra info
    notImplemented();
}

void ChromeClientHaiku::setToolTip(const String& tip)
{
    notImplemented();
}

void ChromeClientHaiku::setToolTip(const String& tip, TextDirection)
{
    notImplemented();
}

void ChromeClientHaiku::print(Frame*)
{
    notImplemented();
}

void ChromeClientHaiku::exceededDatabaseQuota(Frame*, const String& databaseName)
{
    notImplemented();
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClientWx::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
    notImplemented();
}

void ChromeClientWx::reachedApplicationCacheOriginQuota(SecurityOrigin*)
{
    notImplemented();
}
#endif

void ChromeClientHaiku::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    notImplemented();
}

void ChromeClientHaiku::runOpenPanel(Frame*, PassRefPtr<FileChooser>)
{
    notImplemented();
}

void ChromeClientHaiku::chooseIconForFiles(const Vector<String>& filenames, FileChooser* chooser)
{
    chooser->iconLoaded(Icon::createIconForFiles(filenames));
}

void ChromeClientHaiku::setCursor(const Cursor&)
{
    notImplemented();
}

// Notification that the given form element has changed. This function
// will be called frequently, so handling should be very fast.
void ChromeClientHaiku::formStateDidChange(const Node*)
{
    notImplemented();
}

PassOwnPtr<HTMLParserQuirks> ChromeClientHaiku::createHTMLParserQuirks()
{
    notImplemented();
    return 0;
}

bool ChromeClientHaiku::selectItemWritingDirectionIsNatural()
{
    return false;
}

PassRefPtr<PopupMenu> ChromeClientHaiku::createPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuHaiku(client));
}

PassRefPtr<SearchPopupMenu> ChromeClientHaiku::createSearchPopupMenu(PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuHaiku(client));
}

} // namespace WebCore

