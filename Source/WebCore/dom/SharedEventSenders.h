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

#ifndef SharedEventSenders_h
#define SharedEventSenders_h

#include "EventNames.h"
#include "EventSender.h"

namespace WebCore {

class HTMLLinkElement;
class HTMLStyleElement;
class ImageLoader;

class SharedEventSenders {
    WTF_MAKE_NONCOPYABLE(SharedEventSenders); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SharedEventSenders() { }

    EventSender<HTMLLinkElement>& linkLoadEventSender();
    EventSender<HTMLStyleElement>& styleLoadEventSender();
    EventSender<ImageLoader>& imageBeforeloadEventSender();
    EventSender<ImageLoader>& imageLoadEventSender();
    EventSender<ImageLoader>& imageErrorEventSender();
private:
    std::unique_ptr<EventSender<HTMLLinkElement>> m_linkLoadEventSender;
    std::unique_ptr<EventSender<HTMLStyleElement>> m_styleLoadEventSender;
    std::unique_ptr<EventSender<ImageLoader>> m_imageBeforeloadEventSender;
    std::unique_ptr<EventSender<ImageLoader>> m_imageLoadEventSender;
    std::unique_ptr<EventSender<ImageLoader>> m_imageErrorEventSender;
};

} // namespace WebCore

#endif // SharedEventSenders_h
