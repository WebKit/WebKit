/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "ContextMenuController.h"

#include "ContextMenu.h"
#include "ContextMenuClient.h"
#include "Event.h"
#include "EventNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "MouseEvent.h"
#include "Node.h"
#include "RenderLayer.h"
#include "RenderObject.h"

namespace WebCore {

using namespace EventNames;

ContextMenuController::ContextMenuController(Page* page, PassRefPtr<ContextMenuClient> client)
    : m_page(page)
    , m_client(client)
    , m_contextMenu(0)
{
}

ContextMenuController::~ContextMenuController()
{
}

void ContextMenuController::handleContextMenuEvent(Event* event)
{
    ASSERT(event->type() == contextmenuEvent);
    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    HitTestResult result(IntPoint(mouseEvent->x(), mouseEvent->y()));

    if (RenderObject* renderer = event->target()->renderer())
        if (RenderLayer* layer = renderer->layer())
            layer->hitTest(HitTestRequest(false, true), result);

    if (!result.innerNonSharedNode())
        return;

    m_contextMenu.set(new ContextMenu(result));
    m_contextMenu->populate();
    m_client->addCustomContextMenuItems(m_contextMenu.get());
    m_contextMenu->show();
}

void ContextMenuController::contextMenuActionSelected(ContextMenuAction action)
{
    // FIXME: Implement this
}

} // namespace WebCore

