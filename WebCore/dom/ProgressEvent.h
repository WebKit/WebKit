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

#ifndef ProgressEvent_h
#define ProgressEvent_h

#include "Event.h"

namespace WebCore {
    
    class ProgressEvent : public Event {
    public:
        ProgressEvent();
        ProgressEvent(const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total);

        void initProgressEvent(const AtomicString& typeArg, 
                               bool canBubbleArg,
                               bool cancelableArg,
                               bool lengthComputableArg,
                               unsigned loadedArg,
                               unsigned totalArg);
        
        bool lengthComputable() const { return m_lengthComputable; }
        unsigned loaded() const { return m_loaded; }
        unsigned total() const { return m_total; }
        
        virtual bool isProgressEvent() const { return true; }
        
    private:
        bool m_lengthComputable;
        unsigned m_loaded;
        unsigned m_total;
    };
}

#endif // ProgressEvent_h

