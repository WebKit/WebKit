/*
 * Copyright (C) 2014 University of Washington. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SharedEventSenders.h"

#include "HTMLLinkElement.h"
#include "HTMLStyleElement.h"
#include "ImageLoader.h"

namespace WebCore {

EventSender<HTMLLinkElement>& SharedEventSenders::linkLoadEventSender()
{
    if (!m_linkLoadEventSender)
        m_linkLoadEventSender = std::make_unique<EventSender<HTMLLinkElement>>(eventNames().loadEvent);

    return *m_linkLoadEventSender;
}

EventSender<HTMLStyleElement>& SharedEventSenders::styleLoadEventSender()
{
    if (!m_styleLoadEventSender)
        m_styleLoadEventSender = std::make_unique<EventSender<HTMLStyleElement>>(eventNames().loadEvent);

    return *m_styleLoadEventSender;
}

EventSender<ImageLoader>& SharedEventSenders::imageBeforeloadEventSender()
{
    if (!m_imageBeforeloadEventSender)
        m_imageBeforeloadEventSender = std::make_unique<EventSender<ImageLoader>>(eventNames().beforeloadEvent);

    return *m_imageBeforeloadEventSender;
}

EventSender<ImageLoader>& SharedEventSenders::imageLoadEventSender()
{
    if (!m_imageLoadEventSender)
        m_imageLoadEventSender = std::make_unique<EventSender<ImageLoader>>(eventNames().loadEvent);

    return *m_imageLoadEventSender;
}

EventSender<ImageLoader>& SharedEventSenders::imageErrorEventSender()
{
    if (!m_imageErrorEventSender)
        m_imageErrorEventSender = std::make_unique<EventSender<ImageLoader>>(eventNames().errorEvent);

    return *m_imageErrorEventSender;
}

} // namespace WebCore
