/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "CloseWatcherManager.h"

#include "Event.h"
#include "EventNames.h"
#include "KeyboardEvent.h"

namespace WebCore {

CloseWatcherManager::CloseWatcherManager() = default;

void CloseWatcherManager::add(Ref<CloseWatcher> watcher)
{
    if (m_groups.size() < m_allowedNumberOfGroups) {
        Vector<Ref<CloseWatcher>> newGroup;
        newGroup.append(watcher);
        m_groups.append(newGroup);
    } else {
        ASSERT(!m_groups.isEmpty());
        m_groups.last().append(watcher);
    }

    m_nextUserInteractionAllowsNewGroup = true;
}


void CloseWatcherManager::remove(CloseWatcher& watcher)
{
    for (auto& group : m_groups) {
        group.removeFirstMatching([&watcher] (const Ref<CloseWatcher>& current) {
            return current.ptr() == &watcher;
        });
        if (group.isEmpty())
            m_groups.removeFirst(group);
    }
}

void CloseWatcherManager::notifyAboutUserActivation()
{
    if (m_nextUserInteractionAllowsNewGroup)
        m_allowedNumberOfGroups++;

    m_nextUserInteractionAllowsNewGroup = false;
}

bool CloseWatcherManager::canPreventClose()
{
    return m_groups.size() < m_allowedNumberOfGroups;
}

void CloseWatcherManager::escapeKeyHandler(KeyboardEvent& event)
{
    if (!m_groups.isEmpty() && !event.defaultHandled() && event.isTrusted() && event.key() == "Escape"_s) {
        auto& group = m_groups.last();
        Vector<Ref<CloseWatcher>> groupCopy(group);
        for (Ref watcher : makeReversedRange(groupCopy)) {
            if (!watcher->requestToClose())
                break;
        }
    }

    if (m_allowedNumberOfGroups > 1)
        m_allowedNumberOfGroups--;
}

} // namespace WebCore
