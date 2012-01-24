/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebIntentServiceInfo.h"

namespace WebKit {

WebURL WebIntentServiceInfo::url() const
{
    return m_href;
}

void WebIntentServiceInfo::setURL(const WebURL& url)
{
    m_href = url;
}

WebString WebIntentServiceInfo::title() const
{
    return m_title;
}

void WebIntentServiceInfo::setTitle(const WebString& title)
{
    m_title = title;
}

WebString WebIntentServiceInfo::action() const
{
    return m_action;
}

void WebIntentServiceInfo::setAction(const WebString& action)
{
    m_action = action;
}

WebString WebIntentServiceInfo::type() const
{
    return m_type;
}

void WebIntentServiceInfo::setType(const WebString& type)
{
    m_type = type;
}

WebString WebIntentServiceInfo::disposition() const
{
    return m_disposition;
}

void WebIntentServiceInfo::setDisposition(const WebString& disposition)
{
    m_disposition = disposition;
}

WebIntentServiceInfo::WebIntentServiceInfo(const WebString& action,
                                           const WebString& type,
                                           const WebURL& href,
                                           const WebString& title,
                                           const WebString& disposition)
    : m_action(action)
    , m_type(type)
    , m_href(href)
    , m_title(title)
    , m_disposition(disposition)
{
}

} // namespace WebKit
