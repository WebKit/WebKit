/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebICEOptions_h
#define WebICEOptions_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"

namespace WebCore {
class IceOptions;
}

namespace WebKit {

class WebString;

class WebICEOptions {
public:
    enum CandidateType {
        CandidateTypeAll,
        CandidateTypeNoRelay,
        CandidateTypeOnlyRelay,
    };

    WebICEOptions() { }
    WebICEOptions(const WebICEOptions& other) { assign(other); }
    ~WebICEOptions() { reset(); }

    WebICEOptions& operator=(const WebICEOptions& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebICEOptions&);

    WEBKIT_EXPORT void initialize(CandidateType);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT CandidateType candidateTypeToUse() const;

#if WEBKIT_IMPLEMENTATION
    WebICEOptions(const WTF::PassRefPtr<WebCore::IceOptions>&);
#endif

private:
    WebPrivatePtr<WebCore::IceOptions> m_private;
};

} // namespace WebKit

#endif // WebICEOptions_h
