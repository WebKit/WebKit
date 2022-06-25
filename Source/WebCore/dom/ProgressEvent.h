/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

class ProgressEvent : public Event {
    WTF_MAKE_ISO_ALLOCATED(ProgressEvent);
public:
    static Ref<ProgressEvent> create(const AtomString& type, bool lengthComputable, unsigned long long loaded, unsigned long long total)
    {
        return adoptRef(*new ProgressEvent(type, lengthComputable, loaded, total));
    }

    struct Init : EventInit {
        bool lengthComputable { false };
        unsigned long long loaded { 0 };
        unsigned long long total { 0 };
    };

    static Ref<ProgressEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new ProgressEvent(type, initializer, isTrusted));
    }

    bool lengthComputable() const { return m_lengthComputable; }
    unsigned long long loaded() const { return m_loaded; }
    unsigned long long total() const { return m_total; }

    EventInterface eventInterface() const override;

protected:
    ProgressEvent(const AtomString& type, bool lengthComputable, unsigned long long loaded, unsigned long long total);
    ProgressEvent(const AtomString&, const Init&, IsTrusted);

private:
    bool m_lengthComputable;
    unsigned long long m_loaded;
    unsigned long long m_total;
};

} // namespace WebCore
