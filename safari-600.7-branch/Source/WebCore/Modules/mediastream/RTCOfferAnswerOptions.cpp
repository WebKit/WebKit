/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "RTCOfferAnswerOptions.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

PassRefPtr<RTCOfferAnswerOptions> RTCOfferAnswerOptions::create(const Dictionary& options, ExceptionCode& ec)
{
    RefPtr<RTCOfferAnswerOptions> offerAnswerOptions = adoptRef(new RTCOfferAnswerOptions());
    String requestIdentity;
    if (!offerAnswerOptions->initialize(options)) {
        // FIXME: https://webkit.org/b/129800
        // According to the spec, the error is going to be defined yet, so let's use TYPE_MISMATCH_ERR for now.
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }

    return offerAnswerOptions.release();
}

bool RTCOfferAnswerOptions::initialize(const Dictionary& options)
{
    if (!m_private)
        m_private = RTCOfferAnswerOptionsPrivate::create();

    String requestIdentity;
    if (!options.isUndefinedOrNull() && (!options.get("requestIdentity", requestIdentity) || requestIdentity.isEmpty()))
        return false;

    m_private->setRequestIdentity(requestIdentity);
    return true;
}

PassRefPtr<RTCOfferOptions> RTCOfferOptions::create(const Dictionary& options, ExceptionCode& ec)
{
    RefPtr<RTCOfferOptions> offerOptions = adoptRef(new RTCOfferOptions());
    if (!offerOptions->initialize(options)) {
        // FIXME: https://webkit.org/b/129800
        // According to the spec, the error is going to be defined yet, so let's use TYPE_MISMATCH_ERR for now.
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }

    return offerOptions.release();
}

bool RTCOfferOptions::initialize(const Dictionary& options)
{
    RefPtr<RTCOfferOptionsPrivate> optionsPrivate = RTCOfferOptionsPrivate::create();
    m_private = optionsPrivate;

    if (options.isUndefinedOrNull())
        return true;

    if (!RTCOfferAnswerOptions::initialize(options))
        return false;

    String offerToReceiveVideoStr;
    bool numberConversionSuccess;
    if (!options.get("offerToReceiveVideo", offerToReceiveVideoStr))
        return false;

    int64_t intConversionResult = offerToReceiveVideoStr.toInt64Strict(&numberConversionSuccess);
    if (!numberConversionSuccess)
        return false;

    optionsPrivate->setOfferToReceiveVideo(intConversionResult);

    String offerToReceiveAudioStr;
    if (!options.get("offerToReceiveAudio", offerToReceiveAudioStr))
        return false;

    intConversionResult = offerToReceiveAudioStr.toInt64Strict(&numberConversionSuccess);
    if (!numberConversionSuccess)
        return false;

    optionsPrivate->setOfferToReceiveAudio(intConversionResult);

    bool voiceActivityDetection;
    if (options.get("voiceActivityDetection", voiceActivityDetection))
        optionsPrivate->setVoiceActivityDetection(voiceActivityDetection);

    bool iceRestart;
    if (options.get("iceRestart", iceRestart))
        optionsPrivate->setIceRestart(iceRestart);

    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
