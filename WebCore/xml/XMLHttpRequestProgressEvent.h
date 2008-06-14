/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Julien Chaffraix <jchaffraix@webkit.org>. All rights reserved.
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

#ifndef XMLHttpRequestProgressEvent_h
#define XMLHttpRequestProgressEvent_h

#include "ProgressEvent.h"

namespace WebCore {

    class XMLHttpRequestProgressEvent : public ProgressEvent {
    public:
        static PassRefPtr<XMLHttpRequestProgressEvent> create()
        {
            return adoptRef(new XMLHttpRequestProgressEvent);
        }
        static PassRefPtr<XMLHttpRequestProgressEvent> create(const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total)
        {
            return adoptRef(new XMLHttpRequestProgressEvent(type, lengthComputable, loaded, total));
        }

        virtual bool isXMLHttpRequestProgressEvent() const { return true; }

        // Those 2 methods are to be compatible with Firefox and are only a wrapper on top of the real implementation.
        unsigned position() const { return loaded(); }
        unsigned totalSize() const { return total(); }

    private:
        XMLHttpRequestProgressEvent() { }
        XMLHttpRequestProgressEvent(const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total)
            : ProgressEvent(type, lengthComputable, loaded, total)
        {
        }
    };

} // namespace WebCore

#endif // XMLHttpRequestProgressEvent_h
