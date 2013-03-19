/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#if ENABLE(SPEECH_SYNTHESIS)

#include <public/WebSpeechSynthesisUtterance.h>

#include "PlatformSpeechSynthesisUtterance.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

namespace WebKit {

WebSpeechSynthesisUtterance::WebSpeechSynthesisUtterance(const PassRefPtr<PlatformSpeechSynthesisUtterance>& utterance)
    : m_private(utterance)
{
}

WebSpeechSynthesisUtterance& WebSpeechSynthesisUtterance::operator=(WebCore::PlatformSpeechSynthesisUtterance* utterance)
{
    m_private = utterance;
    return *this;
}

void WebSpeechSynthesisUtterance::assign(const WebSpeechSynthesisUtterance& other)
{
    m_private = other.m_private;
}

void WebSpeechSynthesisUtterance::reset()
{
    m_private.reset();
}

WebSpeechSynthesisUtterance::operator PassRefPtr<PlatformSpeechSynthesisUtterance>() const
{
    return m_private.get();
}

WebSpeechSynthesisUtterance::operator PlatformSpeechSynthesisUtterance*() const
{
    return m_private.get();
}

WebString WebSpeechSynthesisUtterance::text() const
{
    return m_private->text();
}

WebString WebSpeechSynthesisUtterance::lang() const
{
    return m_private->lang();
}

WebString WebSpeechSynthesisUtterance::voice() const
{
    return m_private->voice() ? WebString(m_private->voice()->name()) : WebString();
}

float WebSpeechSynthesisUtterance::volume() const
{
    return m_private->volume();
}

float WebSpeechSynthesisUtterance::rate() const
{
    return m_private->rate();
}

float WebSpeechSynthesisUtterance::pitch() const
{
    return m_private->pitch();
}

double WebSpeechSynthesisUtterance::startTime() const
{
    return m_private->startTime();
}

} // namespace WebKit

#endif // ENABLE(SPEECH_SYNTHESIS)
