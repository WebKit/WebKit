// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Chrome.h"

#include "ChromeClient.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InspectorController.h"
#include "Page.h"
#include "ResourceHandle.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include "kjs_window.h"
#include "PausedTimeouts.h"
#include "SecurityOrigin.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

class PageGroupLoadDeferrer : Noncopyable {
public:
    PageGroupLoadDeferrer(Page*, bool deferSelf);
    ~PageGroupLoadDeferrer();
private:
    Vector<RefPtr<Frame>, 16> m_deferredFrames;
#if !PLATFORM(MAC)
    Vector<pair<RefPtr<Frame>, PausedTimeouts*>, 16> m_pausedTimeouts;
#endif
};

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
    
Page* Chrome::createWindow(Frame* frame, const FrameLoadRequest& request, const WindowFeatures& features) const
{
    return m_client->createWindow(frame, request, features);
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

void Chrome::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (source == JSMessageSource)
        m_client->addMessageToConsole(message, lineNumber, sourceID);

    m_page->inspectorController()->addMessageToConsole(source, level, message, lineNumber, sourceID);
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
    String text = message;
    text.replace('\\', frame->backslashAsCurrencySymbol());

    m_client->runJavaScriptAlert(frame, text);
}

bool Chrome::runJavaScriptConfirm(Frame* frame, const String& message)
{
    // Defer loads in case the client method runs a new event loop that would 
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

    ASSERT(frame);
    String text = message;
    text.replace('\\', frame->backslashAsCurrencySymbol());
    
    return m_client->runJavaScriptConfirm(frame, text);
}

bool Chrome::runJavaScriptPrompt(Frame* frame, const String& prompt, const String& defaultValue, String& result)
{
    // Defer loads in case the client method runs a new event loop that would 
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

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
    // Defer loads in case the client method runs a new event loop that would 
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    PageGroupLoadDeferrer deferrer(m_page, true);

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

void Chrome::mouseDidMoveOverElement(const HitTestResult& result, unsigned modifierFlags)
{
    m_client->mouseDidMoveOverElement(result, modifierFlags);
}

void Chrome::setToolTip(const HitTestResult& result)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    String toolTip = result.spellingToolTip();

    // Next priority is a toolTip from a URL beneath the mouse (if preference is set to show those).
    if (toolTip.isEmpty() && m_page->settings()->showsURLsInToolTips()) {
        if (Node* node = result.innerNonSharedNode()) {
            // Get tooltip representing form action, if relevant
            if (node->hasTagName(inputTag)) {
                HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
                if (input->inputType() == HTMLInputElement::SUBMIT)
                    if (HTMLFormElement* form = input->form())
                        toolTip = form->action();
            }
        }

        // Get tooltip representing link's URL
        if (toolTip.isEmpty())
            // FIXME: Need to pass this URL through userVisibleString once that's in WebCore
            toolTip = result.absoluteLinkURL().string();
    }

    // Lastly we'll consider a tooltip for element with "title" attribute
    if (toolTip.isEmpty())
        toolTip = result.title();

    m_client->setToolTip(toolTip);
}

void Chrome::print(Frame* frame)
{
    m_client->print(frame);
}

PageGroupLoadDeferrer::PageGroupLoadDeferrer(Page* page, bool deferSelf)
{
    const HashSet<Page*>* group = page->frameNamespace();

    if (!group)
        return;

    HashSet<Page*>::const_iterator end = group->end();
    for (HashSet<Page*>::const_iterator it = group->begin(); it != end; ++it) {
        Page* otherPage = *it;
        if ((deferSelf || otherPage != page)) {
            if (!otherPage->defersLoading())
                m_deferredFrames.append(otherPage->mainFrame());

#if !PLATFORM(MAC)
            for (Frame* frame = otherPage->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
                if (KJS::Window* window = KJS::Window::retrieveWindow(frame)) {
                    PausedTimeouts* timeouts = window->pauseTimeouts();

                    m_pausedTimeouts.append(make_pair(frame, timeouts));
                }
            }
#endif
        }
    }

    size_t count = m_deferredFrames.size();
    for (size_t i = 0; i < count; ++i)
        if (Page* page = m_deferredFrames[i]->page())
            page->setDefersLoading(true);
}

PageGroupLoadDeferrer::~PageGroupLoadDeferrer()
{
    size_t count = m_deferredFrames.size();
    for (size_t i = 0; i < count; ++i)
        if (Page* page = m_deferredFrames[i]->page())
            page->setDefersLoading(false);

#if !PLATFORM(MAC)
    count = m_pausedTimeouts.size();

    for (size_t i = 0; i < count; i++) {
        KJS::Window* window = KJS::Window::retrieveWindow(m_pausedTimeouts[i].first.get());
        if (window)
            window->resumeTimeouts(m_pausedTimeouts[i].second);
        delete m_pausedTimeouts[i].second;
    }
#endif
}


} // namespace WebCore
