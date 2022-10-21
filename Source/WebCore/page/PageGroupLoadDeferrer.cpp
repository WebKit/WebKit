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
#include "PageGroupLoadDeferrer.h"

#include "Document.h"
#include "DocumentParser.h"
#include "Frame.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptRunner.h"

namespace WebCore {

PageGroupLoadDeferrer::PageGroupLoadDeferrer(Page& page, bool deferSelf)
{
    for (auto& otherPage : page.group().pages()) {
        if (!deferSelf && &otherPage == &page)
            continue;
        if (otherPage.defersLoading())
            continue;
        m_deferredFrames.append(&otherPage.mainFrame());

        // This code is not logically part of load deferring, but we do not want JS code executed beneath modal
        // windows or sheets, which is exactly when PageGroupLoadDeferrer is used.
        for (AbstractFrame* frame = &otherPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            localFrame->document()->suspendScheduledTasks(ReasonForSuspension::WillDeferLoading);
        }
    }

    for (auto& deferredFrame : m_deferredFrames) {
        if (Page* page = deferredFrame->page())
            page->setDefersLoading(true);
    }
}

PageGroupLoadDeferrer::~PageGroupLoadDeferrer()
{
    for (auto& deferredFrame : m_deferredFrames) {
        auto* page = deferredFrame->page();
        if (!page)
            continue;
        page->setDefersLoading(false);

        for (AbstractFrame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            localFrame->document()->resumeScheduledTasks(ReasonForSuspension::WillDeferLoading);
        }
    }
}


} // namespace WebCore
