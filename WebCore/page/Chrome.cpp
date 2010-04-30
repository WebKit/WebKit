/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Chrome.h"

#include "ChromeClient.h"
#include "DNS.h"
#include "Document.h"
#include "FileList.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameTree.h"
#include "Geolocation.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InspectorController.h"
#include "Page.h"
#include "PageGroupLoadDeferrer.h"
#include "RenderObject.h"
#include "ResourceHandle.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if ENABLE(DOM_STORAGE)
#include "StorageNamespace.h"
#endif

namespace WebCore {

using namespace HTMLNames;
using namespace std;

Chrome::Chrome(Page* page, ChromeClient* client)
    : m_page(page)
    , m_client(client)
{
    ASSERT(m_client);
}

Chrome::~Chrome()
{
    m_client->chromeDestroyed();
}

void Chrome::invalidateWindow(const IntRect& updateRect, bool immediate)
{
    m_client->invalidateWindow(updateRect, immediate);
}

void Chrome::invalidateContentsAndWindow(const IntRect& updateRect, bool immediate)
{
    m_client->invalidateContentsAndWindow(updateRect, immediate);
}

void Chrome::invalidateContentsForSlowScroll(const IntRect& updateRect, bool immediate)
{
    m_client->invalidateContentsForSlowScroll(updateRect, immediate);
}

void Chrome::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    m_client->scroll(scrollDelta, rectToScroll, clipRect);
}

IntPoint Chrome::screenToWindow(const IntPoint& point) const
{
    return m_client->screenToWindow(point);
}

IntRect Chrome::windowToScreen(const IntRect& rect) const
{
    return m_client->windowToScreen(rect);
}

PlatformPageClient Chrome::platformPageClient() const
{
    return m_client->platformPageClient();
}

void Chrome::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    m_client->contentsSizeChanged(frame, size);
}

void Chrome::scrollRectIntoView(const IntRect& rect) const
{
    // FIXME: The unused ScrollView* argument can and should be removed from ChromeClient::scrollRectIntoView.
    m_client->scrollRectIntoView(rect, 0);
}

void Chrome::scrollbarsModeDidChange() const
{
    m_client->scrollbarsModeDidChange();
}

void Chrome::setWindowRect(const FloatRect& rect) const
{
    m_client->setWindowRect(rect);
}

FloatRect Chrome::windowRect() const
{
    return m_client->windowRect();
}

FloatRect Chrome::pageRect() const
{
    return m_client->pageRect();
}

float Chrome::scaleFactor()
{
    return m_client->scaleFactor();
}

void Chrome::focus() const
{
    m_client->focus();
}

void Chrome::unfocus() const
{
    m_client->unfocus();
}

bool Chrome::canTakeFocus(FocusDirection direction) const
{
    return m_client->canTakeFocus(direction);
}

void Chrome::takeFocus(FocusDirection direction) const
{
    m_client->takeFocus(direction);
}

void Chrome::focusedNodeChanged(Node* node) const
{
    m_client->focusedNodeChanged(node);
}

Page* Chrome::createWindow(Frame* frame, const FrameLoadRequest& request, const WindowFeatures& features) const
{
    Page* newPage = m_client->createWindow(frame, request, features);

#if ENABLE(DOM_STORAGE)
    if (newPage) {
        if (StorageNamespace* oldSessionStorage = m_page->sessionStorage(false))
            newPage->setSessionStorage(oldSessionStorage->copy());
    }
#endif

    return newPage;
}

void Chrome::show() const
{
    m_client->show();
}

bool Chrome::canRunModal() const
{
    return m_client->canRunModal();
}

bool Chrome::canRunModalNow() const
{
    // If loads are blocked, we can't run modal because the contents
    // of the modal dialog will never show up!
    return canRunModal() && !ResourceHandle::loadsBlocked();
}

void Chrome::runModal() const
{
    // Defer callbacks in all the other pages in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    PageGroupLoadDeferrer deferrer(m_page, false);

    TimerBase::fireTimersInNestedEventLoop();
    m_client->runModal();
}

void Chrome::setToolbarsVisible(bool b) const
{
    m_client->setToolbarsVisible(b);
}

bool Chrome::toolbarsVisible() const
{
    return m_client->toolbarsVisible();
}

void Chrome::setStatusbarVisible(bool b) const
{
    m_client->setStatusbarVisible(b);
}

bool Chrome::statusbarVisible() const
{
    return m_client->statusbarVisible();
}

void Chrome::setScrollbarsVisible(bool b) const
{
    m_client->setScrollbarsVisible(b);
}

bool Chrome::scrollbarsVisible() const
{
    return m_client->scrollbarsVisible();
}

void Chrome::setMenubarVisible(bool b) const
{
    m_client->setMenubarVisible(b);
}

bool Chrome::menubarVisible() const
{
    return m_client->menubarVisible();
}

void Chrome::setResizable(bool b) const
{
    m_client->setResizable(b);
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    return m_client->runBeforeUnloadConfirmPanel(message, frame);
}

void Chrome::closeWindowSoon()
{
    m_client->closeWindowSoon();
}

void Chrome::runJavaScriptAlert(Frame* frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    m_client->runJavaScriptAlert(frame, frame->displayStringModifiedByEncoding(message));
}

bool Chrome::runJavaScriptConfirm(Frame* frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    return m_client->runJavaScriptConfirm(frame, frame->displayStringModifiedByEncoding(message));
}

bool Chrome::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultValue, String& result)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    bool ok = m_client->runJavaScriptPrompt(frame, frame->displayStringModifiedByEncoding(prompt), frame->displayStringModifiedByEncoding(defaultValue), result);

    if (ok)
        result = frame->displayStringModifiedByEncoding(result);

    return ok;
}

void Chrome::setStatusbarText(Frame* frame, const String& status)
{
    ASSERT(frame);
    m_client->setStatusbarText(frame->displayStringModifiedByEncoding(status));
}

bool Chrome::shouldInterruptJavaScript()
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    return m_client->shouldInterruptJavaScript();
}

void Chrome::registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title)
{
    m_client->registerProtocolHandler(scheme, baseURL, url, title);
}

void Chrome::registerContentHandler(const String& mimeType, const String& baseURL, const String& url, const String& title)
{
    m_client->registerContentHandler(mimeType,  baseURL, url,  title);
}

IntRect Chrome::windowResizerRect() const
{
    return m_client->windowResizerRect();
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    if (result.innerNode()) {
        Document* document = result.innerNode()->document();
        if (document && document->isDNSPrefetchEnabled())
            ResourceHandle::prepareForURL(result.absoluteLinkURL());
    }
    m_client->mouseDidMoveOverElement(result, modifierFlags);

#if ENABLE(INSPECTOR)
    if (InspectorController* inspector = m_page->inspectorController())
        inspector->mouseDidMoveOverElement(result, modifierFlags);
#endif
}

void Chrome::setToolTip(const HitTestResult& result)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    TextDirection toolTipDirection;
    String toolTip = result.spellingToolTip(toolTipDirection);

    // Next priority is a toolTip from a URL beneath the mouse (if preference is set to show those).
    if (toolTip.isEmpty() && m_page->settings()->showsURLsInToolTips()) {
        if (Node* node = result.innerNonSharedNode()) {
            // Get tooltip representing form action, if relevant
            if (node->hasTagName(inputTag)) {
                HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
                if (input->inputType() == HTMLInputElement::SUBMIT)
                    if (HTMLFormElement* form = input->form()) {
                        toolTip = form->action();
                        if (form->renderer())
                            toolTipDirection = form->renderer()->style()->direction();
                        else
                            toolTipDirection = LTR;
                    }
            }
        }

        // Get tooltip representing link's URL
        if (toolTip.isEmpty()) {
            // FIXME: Need to pass this URL through userVisibleString once that's in WebCore
            toolTip = result.absoluteLinkURL().string();
            // URL always display as LTR.
            toolTipDirection = LTR;
        }
    }

    // Next we'll consider a tooltip for element with "title" attribute
    if (toolTip.isEmpty())
        toolTip = result.title(toolTipDirection);

    // Lastly, for <input type="file"> that allow multiple files, we'll consider a tooltip for the selected filenames
    if (toolTip.isEmpty()) {
        if (Node* node = result.innerNonSharedNode()) {
            if (node->hasTagName(inputTag)) {
                HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
                if (input->inputType() == HTMLInputElement::FILE) {
                    FileList* files = input->files();
                    unsigned listSize = files->length();
                    if (files && listSize > 1) {
                        Vector<UChar> names;
                        for (size_t i = 0; i < listSize; ++i) {
                            append(names, files->item(i)->fileName());
                            if (i != listSize - 1)
                                names.append('\n');
                        }
                        toolTip = String::adopt(names);
                        // filename always display as LTR.
                        toolTipDirection = LTR;
                    }
                }
            }
        }
    }

    m_client->setToolTip(toolTip, toolTipDirection);
}

void Chrome::print(Frame* frame)
{
    m_client->print(frame);
}

void Chrome::requestGeolocationPermissionForFrame(Frame* frame, Geolocation* geolocation)
{
    m_client->requestGeolocationPermissionForFrame(frame, geolocation);
}

void Chrome::cancelGeolocationPermissionRequestForFrame(Frame* frame, Geolocation* geolocation)
{
    m_client->cancelGeolocationPermissionRequestForFrame(frame, geolocation);
}

void Chrome::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> fileChooser)
{
    m_client->runOpenPanel(frame, fileChooser);
}

void Chrome::chooseIconForFiles(const Vector<String>& filenames, FileChooser* fileChooser)
{
    m_client->chooseIconForFiles(filenames, fileChooser);
}

bool Chrome::setCursor(PlatformCursorHandle cursor)
{
    return m_client->setCursor(cursor);
}

#if ENABLE(NOTIFICATIONS)
NotificationPresenter* Chrome::notificationPresenter() const
{
    return m_client->notificationPresenter();
}
#endif

// --------

#if ENABLE(DASHBOARD_SUPPORT)
void ChromeClient::dashboardRegionsChanged()
{
}
#endif

void ChromeClient::populateVisitedLinks()
{
}

FloatRect ChromeClient::customHighlightRect(Node*, const AtomicString&, const FloatRect&)
{
    return FloatRect();
}

void ChromeClient::paintCustomHighlight(Node*, const AtomicString&, const FloatRect&, const FloatRect&, bool, bool)
{
}

bool ChromeClient::shouldReplaceWithGeneratedFileForUpload(const String&, String&)
{
    return false;
}

String ChromeClient::generateReplacementFile(const String&)
{
    ASSERT_NOT_REACHED();
    return String();
}

bool ChromeClient::paintCustomScrollbar(GraphicsContext*, const FloatRect&, ScrollbarControlSize,
                                        ScrollbarControlState, ScrollbarPart, bool,
                                        float, float, ScrollbarControlPartMask)
{
    return false;
}

bool ChromeClient::paintCustomScrollCorner(GraphicsContext*, const FloatRect&)
{
    return false;
}


} // namespace WebCore
