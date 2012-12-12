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

#include "config.h"
#include <public/WebPrerender.h>
#include <wtf/PassRefPtr.h>

#if ENABLE(LINK_PRERENDER)

#include "Prerender.h"

namespace WebKit {

namespace {

class ExtraDataContainer : public WebCore::Prerender::ExtraData {
public:
    static PassRefPtr<ExtraDataContainer> create(WebPrerender::ExtraData* extraData) { return adoptRef(new ExtraDataContainer(extraData)); }

    virtual ~ExtraDataContainer() { }

    WebPrerender::ExtraData* extraData() const { return m_extraData.get(); }

private:
    explicit ExtraDataContainer(WebPrerender::ExtraData* extraData)
        : m_extraData(adoptPtr(extraData))
    {
    }

    OwnPtr<WebPrerender::ExtraData> m_extraData;
};

} // anon namespace

WebPrerender::WebPrerender(PassRefPtr<WebCore::Prerender> prerender)
    : m_private(prerender)
{
}

const WebCore::Prerender* WebPrerender::toPrerender() const
{
    return m_private.get();
}

void WebPrerender::reset()
{
    m_private.reset();
}

void WebPrerender::assign(const WebPrerender& other)
{
    m_private = other.m_private;
}

bool WebPrerender::isNull() const
{
    return m_private.isNull();
}

WebURL WebPrerender::url() const
{
    return WebURL(m_private->url());
}

WebString WebPrerender::referrer() const
{
    return m_private->referrer();
}

WebReferrerPolicy WebPrerender::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(m_private->referrerPolicy());
}

void WebPrerender::setExtraData(WebPrerender::ExtraData* extraData)
{
    m_private->setExtraData(ExtraDataContainer::create(extraData));
}

const WebPrerender::ExtraData* WebPrerender::extraData() const
{
    RefPtr<WebCore::Prerender::ExtraData> webcoreExtraData = m_private->extraData();
    if (!webcoreExtraData)
        return 0;
    return static_cast<ExtraDataContainer*>(webcoreExtraData.get())->extraData();
}

void WebPrerender::didStartPrerender()
{
    m_private->didStartPrerender();
}

void WebPrerender::didStopPrerender()
{
    m_private->didStopPrerender();
}

void WebPrerender::didSendLoadForPrerender()
{
    m_private->didSendLoadForPrerender();
}

void WebPrerender::didSendDOMContentLoadedForPrerender()
{
    m_private->didSendDOMContentLoadedForPrerender();
}

} // namespace WebKit

#endif // ENABLED(LINK_PRERENDER)
