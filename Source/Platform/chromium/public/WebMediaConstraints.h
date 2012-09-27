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

#ifndef WebMediaConstraints_h
#define WebMediaConstraints_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebString.h"
#include "WebVector.h"

namespace WebCore {
struct MediaConstraint;
class MediaConstraints;
}

namespace WebKit {

struct WebMediaConstraint {
    WebMediaConstraint()
    {
    }

    WebMediaConstraint(WebString name, WebString value)
        : m_name(name)
        , m_value(value)
    {
    }

#if WEBKIT_IMPLEMENTATION
    WebMediaConstraint(const WebCore::MediaConstraint&);
#endif

    WebString m_name;
    WebString m_value;
};

class WebMediaConstraints {
public:
    WebMediaConstraints() { }
    WebMediaConstraints(const WebMediaConstraints& other) { assign(other); }
    ~WebMediaConstraints() { reset(); }

    WebMediaConstraints& operator=(const WebMediaConstraints& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebMediaConstraints&);

    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT void getMandatoryConstraints(WebVector<WebMediaConstraint>&) const;
    WEBKIT_EXPORT void getOptionalConstraints(WebVector<WebMediaConstraint>&) const;

    WEBKIT_EXPORT bool getMandatoryConstraintValue(const WebString& name, WebString& value) const;
    WEBKIT_EXPORT bool getOptionalConstraintValue(const WebString& name, WebString& value) const;

#if WEBKIT_IMPLEMENTATION
    WebMediaConstraints(const WTF::PassRefPtr<WebCore::MediaConstraints>&);
    WebMediaConstraints(WebCore::MediaConstraints*);
#endif

private:
    WebPrivatePtr<WebCore::MediaConstraints> m_private;
};

} // namespace WebKit

#endif // WebMediaConstraints_h
