/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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

#pragma once

#include "Event.h"
#include "EventInit.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"

namespace WebCore {

class NavigationCurrentEntryChangeEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(NavigationCurrentEntryChangeEvent);
public:
    struct Init : EventInit {
        std::optional<NavigationNavigationType> navigationType;
        RefPtr<NavigationHistoryEntry> from;
    };

    static Ref<NavigationCurrentEntryChangeEvent> create(const AtomString& type, const Init&);

    std::optional<NavigationNavigationType> navigationType() const { return m_navigationType; };
    RefPtr<NavigationHistoryEntry> from() const { return m_from; };

private:
    NavigationCurrentEntryChangeEvent(const AtomString& type, const Init&);

    EventInterface eventInterface() const override;

    std::optional<NavigationNavigationType> m_navigationType;
    RefPtr<NavigationHistoryEntry> m_from;
};

} // namespace WebCore
