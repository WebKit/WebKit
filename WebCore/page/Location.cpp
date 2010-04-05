/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Location.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "PlatformString.h"

namespace WebCore {

Location::Location(Frame* frame)
    : m_frame(frame)
{
}

void Location::disconnectFrame()
{
    m_frame = 0;
}

inline const KURL& Location::url() const
{
    ASSERT(m_frame);

    const KURL& url = m_frame->loader()->url();
    if (!url.isValid())
        return blankURL();  // Use "about:blank" while the page is still loading (before we have a frame).

    return url;
}

String Location::href() const
{
    if (!m_frame)
        return String();

    const KURL& url = this->url();
    return url.hasPath() ? url.prettyURL() : url.prettyURL() + "/";
}

String Location::protocol() const
{
    if (!m_frame)
        return String();

    return url().protocol() + ":";
}

String Location::host() const
{
    if (!m_frame)
        return String();

    // Note: this is the IE spec. The NS spec swaps the two, it says
    // "The hostname property is the concatenation of the host and port properties, separated by a colon."
    const KURL& url = this->url();
    return url.port() ? url.host() + ":" + String::number(url.port()) : url.host();
}

String Location::hostname() const
{
    if (!m_frame)
        return String();

    return url().host();
}

String Location::port() const
{
    if (!m_frame)
        return String();

    const KURL& url = this->url();
    return url.port() ? String::number(url.port()) : "";
}

String Location::pathname() const
{
    if (!m_frame)
        return String();

    const KURL& url = this->url();
    return url.path().isEmpty() ? "/" : url.path();
}

String Location::search() const
{
    if (!m_frame)
        return String();

    const KURL& url = this->url();
    return url.query().isEmpty() ? "" : "?" + url.query();
}

String Location::hash() const
{
    if (!m_frame)
        return String();

    const String& fragmentIdentifier = this->url().fragmentIdentifier();
    return fragmentIdentifier.isEmpty() ? "" : "#" + fragmentIdentifier;
}

String Location::toString() const
{
    if (!m_frame)
        return String();

    const KURL& url = this->url();
    return url.hasPath() ? url.prettyURL() : url.prettyURL() + "/";
}

} // namespace WebCore
