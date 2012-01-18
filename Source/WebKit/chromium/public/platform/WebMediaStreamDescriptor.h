/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebMediaStreamDescriptor_h
#define WebMediaStreamDescriptor_h

#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivatePtr.h"
#include "WebVector.h"

namespace WebCore {
class MediaStreamDescriptor;
}

namespace WebKit {

class WebMediaStreamSource;
class WebString;

class WebMediaStreamDescriptor {
public:
    WebMediaStreamDescriptor() { }
    WebMediaStreamDescriptor(const WebMediaStreamDescriptor& request) { assign(request); }
    ~WebMediaStreamDescriptor() { reset(); }

    WebMediaStreamDescriptor& operator=(const WebMediaStreamDescriptor& other)
    {
        assign(other);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebMediaStreamDescriptor&);
    WEBKIT_EXPORT void initialize(const WebString& label, const WebVector<WebMediaStreamSource>&);
    WEBKIT_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    WEBKIT_EXPORT WebString label() const;
    WEBKIT_EXPORT void sources(WebVector<WebMediaStreamSource>&) const;

#if WEBKIT_IMPLEMENTATION
    WebMediaStreamDescriptor(const WTF::PassRefPtr<WebCore::MediaStreamDescriptor>&);
    operator WTF::PassRefPtr<WebCore::MediaStreamDescriptor>() const;
    operator WebCore::MediaStreamDescriptor*() const;
    WebMediaStreamDescriptor& operator=(const WTF::PassRefPtr<WebCore::MediaStreamDescriptor>&);
#endif

private:
    WebPrivatePtr<WebCore::MediaStreamDescriptor> m_private;
};

} // namespace WebKit

#endif // WebMediaStreamDescriptor_h
