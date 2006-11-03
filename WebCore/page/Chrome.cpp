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

#include "Page.h"
#include "ResourceLoader.h"
#include "ChromeClient.h"
#include <wtf/Vector.h>

namespace WebCore {

bool Chrome::canRunModal()
{
    if (!m_client)
        return false;
    return m_client->canRunModal();
}

bool Chrome::canRunModalNow()
{
    // If loads are blocked, we can't run modal because the contents
    // of the modal dialog will never show up!
    return canRunModal() && !ResourceLoader::loadsBlocked();
}

void Chrome::runModal()
{
    if (!canRunModal())
        return;

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

} // namespace WebCore
