/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved. 
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * All rights reserved.
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

#include "config.h"
#include "ImageSource.h"
#include "ImageDecoderQt.h"

namespace WebCore {

NativeImagePtr ImageSource::createFrameAtIndex(size_t index)
{
    return m_decoder ? m_decoder->imageAtIndex(index) : 0;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    // Many annoying ads specify a 0 duration to make an image flash as quickly
    // as possible.  We follow WinIE's behavior and use a duration of 100 ms
    // for any frames that specify a duration of <= 50 ms.  See
    // <http://bugs.webkit.org/show_bug.cgi?id=14413> or Radar 4051389 for
    // more.
    const float duration = m_decoder->duration(index) / 1000.0f;
    return (duration < 0.051f) ? 0.100f : duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    return frameIsCompleteAtIndex(index) && m_decoder->supportsAlpha() &&
        m_decoder->imageAtIndex(index)->hasAlphaChannel();
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    return m_decoder && m_decoder->imageAtIndex(index);
}

}

// vim: ts=4 sw=4 et
