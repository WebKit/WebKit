// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

Page* Chrome::createWindow(const FrameLoadRequest& request) const
{
    return m_client->createWindow(request);
}

Page* Chrome::createModalDialog(const FrameLoadRequest& request) const
{
    return m_client->createModalDialog(request);
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

void Chrome::addMessageToConsole(const String &message, unsigned lineNumber, const String &sourceURL)
{
    m_client->addMessageToConsole(message, lineNumber, sourceURL);
}

} // namespace WebCore

