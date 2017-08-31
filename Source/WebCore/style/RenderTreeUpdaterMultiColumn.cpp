/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2015, 2017 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "RenderTreeUpdaterMultiColumn.h"

#include "RenderBlockFlow.h"
#include "RenderMultiColumnFlowThread.h"

namespace WebCore {

void RenderTreeUpdater::MultiColumn::update(RenderBlockFlow& flow)
{
    bool needsFlowThread = flow.requiresColumns(flow.style().columnCount());
    auto* multiColumnFlowThread = flow.multiColumnFlowThread();
    if (!needsFlowThread) {
        if (multiColumnFlowThread) {
            multiColumnFlowThread->evacuateAndDestroy();
            ASSERT(!flow.multiColumnFlowThread());
        }
        return;
    }
    if (!multiColumnFlowThread)
        createFlowThread(flow);
}

void RenderTreeUpdater::MultiColumn::createFlowThread(RenderBlockFlow& flow)
{
    RenderMultiColumnFlowThread* flowThread = new RenderMultiColumnFlowThread(flow.document(), RenderStyle::createAnonymousStyleWithDisplay(flow.style(), BLOCK));
    flowThread->initializeStyle();
    flow.setChildrenInline(false); // Do this to avoid wrapping inline children that are just going to move into the flow thread.
    flow.deleteLines();
    flow.RenderBlock::addChild(flowThread);
    flowThread->populate(); // Called after the flow thread is inserted so that we are reachable by the flow thread.
    flow.setMultiColumnFlowThread(flowThread);
}


}
