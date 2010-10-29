/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebPage.h"

#include "WebCoreArgumentCoders.h"
#include "WebEvent.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/WindowsKeyboardCodes.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
    m_page->addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));
}

void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
}

// FIXME: need to add support for input methods
    
bool WebPage::interceptEditingKeyboardEvent(KeyboardEvent* evt, bool shouldSaveCommand)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);
    
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;
    const Vector<KeypressCommand>& commands = evt->keypressCommands();
    bool hasKeypressCommand = !commands.isEmpty();
    
    bool eventWasHandled = false;
    
    if (shouldSaveCommand && !hasKeypressCommand) {
        Vector<KeypressCommand> commandsList;        
        if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(keyEvent->type()), 
                                                         Messages::WebPageProxy::InterpretKeyEvent::Reply(commandsList),
                                                         m_pageID, CoreIPC::Connection::NoTimeout))
            return false;
        for (size_t i = 0; i < commandsList.size(); i++)
            evt->keypressCommands().append(commandsList[i]);
    } else {
        size_t size = commands.size();
        // Are there commands that would just cause text insertion if executed via Editor?
        // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
        // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        bool haveTextInsertionCommands = false;
        for (size_t i = 0; i < size; ++i) {
            if (frame->editor()->command(commands[i].commandName).isTextInsertion())
                haveTextInsertionCommands = true;
        }
        if (!haveTextInsertionCommands || keyEvent->type() == PlatformKeyboardEvent::Char) {
            for (size_t i = 0; i < size; ++i) {
                if (commands[i].commandName == "insertText") {
                    // Don't insert null or control characters as they can result in unexpected behaviour
                    if (evt->charCode() < ' ')
                        return false;
                    eventWasHandled = frame->editor()->insertText(commands[i].text, evt);
                } else
                    if (frame->editor()->command(commands[i].commandName).isSupported())
                        eventWasHandled = frame->editor()->command(commands[i].commandName).execute(evt);
            }
        }
    }
    return eventWasHandled;
}

static inline void scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown)
        return false;

    // FIXME: This should be in WebCore.

    switch (keyboardEvent.windowsVirtualKeyCode()) {
    case VK_BACK:
        if (keyboardEvent.shiftKey())
            m_page->goForward();
        else
            m_page->goBack();
        break;
    case VK_SPACE:
        if (keyboardEvent.shiftKey())
            scroll(m_page.get(), ScrollUp, ScrollByPage);
        else
            scroll(m_page.get(), ScrollDown, ScrollByPage);
        break;
    case VK_PRIOR:
        scroll(m_page.get(), ScrollUp, ScrollByPage);
        break;
    case VK_NEXT:
        scroll(m_page.get(), ScrollDown, ScrollByPage);
        break;
    case VK_HOME:
        scroll(m_page.get(), ScrollUp, ScrollByDocument);
        scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        break;
    case VK_END:
        scroll(m_page.get(), ScrollDown, ScrollByDocument);
        scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        break;
    case VK_UP:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollUp, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollUp, ScrollByPage);
        else
            scroll(m_page.get(), ScrollUp, ScrollByLine);
        break;
    case VK_DOWN:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollDown, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollDown, ScrollByPage);
        else
            scroll(m_page.get(), ScrollDown, ScrollByLine);
        break;
    case VK_LEFT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goBack();
        else {
            if (keyboardEvent.altKey()  | keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollLeft, ScrollByPage);
            else
                scroll(m_page.get(), ScrollLeft, ScrollByLine);
        }
        break;
    case VK_RIGHT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goForward();
        else {
            if (keyboardEvent.altKey() || keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollRight, ScrollByPage);
            else
                scroll(m_page.get(), ScrollRight, ScrollByLine);
        }
        break;
    default:
        return false;
    }

    return true;
}

} // namespace WebKit
