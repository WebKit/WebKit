// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Trolltech ASA
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "Chrome.h"

#include "ChromeClient.h"
#include "FloatRect.h"
#include "Frame.h"
#include "InspectorController.h"
#include "Page.h"
#include "ResourceHandle.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

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

Page* Chrome::createWindow(Frame* frame, const FrameLoadRequest& request) const
{
    return m_client->createWindow(frame, request);
}

Page* Chrome::createModalDialog(Frame* frame, const FrameLoadRequest& request) const
{
    return m_client->createModalDialog(frame, request);
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
    if (m_page->defersLoading()) {
        LOG_ERROR("Tried to run modal in a page when it was deferring loading -- should never happen.");
        return;
    }

    // Defer callbacks in all the other pages in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    Vector<Page*> pagesToDefer;
    if (const HashSet<Page*>* group = m_page->frameNamespace()) {
        HashSet<Page*>::const_iterator end = group->end();
        for (HashSet<Page*>::const_iterator it = group->begin(); it != end; ++it) {
            Page* otherPage = *it;
            if (otherPage != m_page && !otherPage->defersLoading())
                pagesToDefer.append(otherPage);
        }
    }
    size_t count = pagesToDefer.size();
    for (size_t i = 0; i < count; ++i)
        pagesToDefer[i]->setDefersLoading(true);

    // Go run the modal event loop.
    m_client->runModal();
    
    // Restore loading for any views that we shut down.
    for (size_t i = 0; i < count; ++i)
        pagesToDefer[i]->setDefersLoading(false);
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

void Chrome::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (source == JSMessageSource && level == ErrorMessageLevel)
        m_client->addMessageToConsole(message, lineNumber, sourceID);
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    return m_client->runBeforeUnloadConfirmPanel(message, frame);
}

void Chrome::closeWindowSoon()
{
    m_client->closeWindowSoon();
}

void Chrome::runJavaScriptAlert(Frame* frame, const String& message)
{
    ASSERT(frame);
    String text = message;
    text.replace('\\', frame->backslashAsCurrencySymbol());
    m_client->runJavaScriptAlert(frame, text);
}

bool Chrome::runJavaScriptConfirm(Frame* frame, const String& message)
{
    ASSERT(frame);
    String text = message;
    text.replace('\\', frame->backslashAsCurrencySymbol());
    
    return m_client->runJavaScriptConfirm(frame, text);
}

bool Chrome::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultValue, String& result)
{
    ASSERT(frame);
    String promptText = prompt;
    promptText.replace('\\', frame->backslashAsCurrencySymbol());
    String defaultValueText = defaultValue;
    defaultValueText.replace('\\', frame->backslashAsCurrencySymbol());
    
    bool ok = m_client->runJavaScriptPrompt(frame, promptText, defaultValueText, result);
    
    if (ok)
        result.replace(frame->backslashAsCurrencySymbol(), '\\');
    
    return ok;
}

void Chrome::setStatusbarText(Frame* frame, const String& status)
{
    ASSERT(frame);
    String text = status;
    text.replace('\\', frame->backslashAsCurrencySymbol());
    
    m_client->setStatusbarText(text);
}

bool Chrome::shouldInterruptJavaScript()
{
    return m_client->shouldInterruptJavaScript();
}

IntRect Chrome::windowResizerRect() const
{
    return m_client->windowResizerRect();
}

void Chrome::addToDirtyRegion(const IntRect& rect)
{
    m_client->addToDirtyRegion(rect);
}

void Chrome::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    m_client->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void Chrome::updateBackingStore()
{
    m_client->updateBackingStore();
}

} // namespace WebCore

