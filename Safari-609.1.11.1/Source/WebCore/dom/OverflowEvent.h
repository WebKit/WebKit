/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "Event.h"

namespace WebCore {

class OverflowEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(OverflowEvent);
public:
    enum orientType {
        HORIZONTAL = 0,
        VERTICAL   = 1,
        BOTH       = 2
    };

    static Ref<OverflowEvent> create(bool horizontalOverflowChanged, bool horizontalOverflow, bool verticalOverflowChanged, bool verticalOverflow)
    {
        return adoptRef(*new OverflowEvent(horizontalOverflowChanged, horizontalOverflow, verticalOverflowChanged, verticalOverflow));
    }

    static Ref<OverflowEvent> createForBindings()
    {
        return adoptRef(*new OverflowEvent);
    }

    struct Init : EventInit {
        unsigned short orient { 0 };
        bool horizontalOverflow { false };
        bool verticalOverflow { false };
    };

    static Ref<OverflowEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new OverflowEvent(type, initializer, isTrusted));
    }

    WEBCORE_EXPORT void initOverflowEvent(unsigned short orient, bool horizontalOverflow, bool verticalOverflow);

    unsigned short orient() const { return m_orient; }
    bool horizontalOverflow() const { return m_horizontalOverflow; }
    bool verticalOverflow() const { return m_verticalOverflow; }

    EventInterface eventInterface() const override;

private:
    OverflowEvent();
    OverflowEvent(bool horizontalOverflowChanged, bool horizontalOverflow, bool verticalOverflowChanged, bool verticalOverflow);
    OverflowEvent(const AtomString&, const Init&, IsTrusted);

    unsigned short m_orient;
    bool m_horizontalOverflow;
    bool m_verticalOverflow;
};

} // namespace WebCore
