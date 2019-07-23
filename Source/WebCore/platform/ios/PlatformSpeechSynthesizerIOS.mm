/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
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

#import "config.h"
#import "PlatformSpeechSynthesizer.h"

#if ENABLE(SPEECH_SYNTHESIS) && PLATFORM(IOS_FAMILY)

#import "PlatformSpeechSynthesisUtterance.h"
#import "PlatformSpeechSynthesisVoice.h"
#import <AVFoundation/AVSpeechSynthesis.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RetainPtr.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

static float getAVSpeechUtteranceDefaultSpeechRate()
{
    static float value;
    static void* symbol;
    if (!symbol) {
        void* symbol = dlsym(PAL::AVFoundationLibrary(), "AVSpeechUtteranceDefaultSpeechRate");
        RELEASE_ASSERT_WITH_MESSAGE(symbol, "%s", dlerror());
        value = *static_cast<float const *>(symbol);
    }
    return value;
}

static float getAVSpeechUtteranceMaximumSpeechRate()
{
    static float value;
    static void* symbol;
    if (!symbol) {
        void* symbol = dlsym(PAL::AVFoundationLibrary(), "AVSpeechUtteranceMaximumSpeechRate");
        RELEASE_ASSERT_WITH_MESSAGE(symbol, "%s", dlerror());
        value = *static_cast<float const *>(symbol);
    }
    return value;
}

#define AVSpeechUtteranceDefaultSpeechRate getAVSpeechUtteranceDefaultSpeechRate()
#define AVSpeechUtteranceMaximumSpeechRate getAVSpeechUtteranceMaximumSpeechRate()

@interface WebSpeechSynthesisWrapper : NSObject<AVSpeechSynthesizerDelegate>
{
    WebCore::PlatformSpeechSynthesizer* m_synthesizerObject;
    // Hold a Ref to the utterance so that it won't disappear until the synth is done with it.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> m_utterance;

    RetainPtr<AVSpeechSynthesizer> m_synthesizer;
}

- (WebSpeechSynthesisWrapper *)initWithSpeechSynthesizer:(WebCore::PlatformSpeechSynthesizer*)synthesizer;
- (void)speakUtterance:(RefPtr<WebCore::PlatformSpeechSynthesisUtterance>&&)utterance;

@end

@implementation WebSpeechSynthesisWrapper

- (WebSpeechSynthesisWrapper *)initWithSpeechSynthesizer:(WebCore::PlatformSpeechSynthesizer*)synthesizer
{
    if (!(self = [super init]))
        return nil;

    m_synthesizerObject = synthesizer;
    return self;
}

- (float)mapSpeechRateToPlatformRate:(float)rate
{
    // WebSpeech says to go from .1 -> 10 (default 1)
    // AVSpeechSynthesizer asks for 0 -> 1 (default. 5)
    if (rate < 1)
        rate *= AVSpeechUtteranceDefaultSpeechRate;
    else
        rate = AVSpeechUtteranceDefaultSpeechRate + ((rate - 1) * (AVSpeechUtteranceMaximumSpeechRate - AVSpeechUtteranceDefaultSpeechRate));

    return rate;
}

- (void)speakUtterance:(RefPtr<WebCore::PlatformSpeechSynthesisUtterance>&&)utterance
{
    // When speak is called we should not have an existing speech utterance outstanding.
    ASSERT(!m_utterance);
    ASSERT(utterance);

    if (!utterance)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (!m_synthesizer) {
        m_synthesizer = adoptNS([PAL::allocAVSpeechSynthesizerInstance() init]);
        [m_synthesizer setDelegate:self];
    }

    // Choose the best voice, by first looking at the utterance voice, then the utterance language,
    // then choose the default language.
    WebCore::PlatformSpeechSynthesisVoice* utteranceVoice = utterance->voice();
    NSString *voiceLanguage = nil;
    if (!utteranceVoice) {
        if (utterance->lang().isEmpty())
            voiceLanguage = [PAL::getAVSpeechSynthesisVoiceClass() currentLanguageCode];
        else
            voiceLanguage = utterance->lang();
    } else
        voiceLanguage = utterance->voice()->lang();

    AVSpeechSynthesisVoice *avVoice = nil;
    if (voiceLanguage)
        avVoice = [PAL::getAVSpeechSynthesisVoiceClass() voiceWithLanguage:voiceLanguage];

    AVSpeechUtterance *avUtterance = [PAL::getAVSpeechUtteranceClass() speechUtteranceWithString:utterance->text()];

    [avUtterance setRate:[self mapSpeechRateToPlatformRate:utterance->rate()]];
    [avUtterance setVolume:utterance->volume()];
    [avUtterance setPitchMultiplier:utterance->pitch()];
    [avUtterance setVoice:avVoice];
    m_utterance = WTFMove(utterance);

    [m_synthesizer speakUtterance:avUtterance];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (void)pause
{
    if (!m_utterance)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_synthesizer pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (void)resume
{
    if (!m_utterance)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_synthesizer continueSpeaking];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (void)cancel
{
    if (!m_utterance)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [m_synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    END_BLOCK_OBJC_EXCEPTIONS
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didStartSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);
    if (!m_utterance)
        return;

    m_synthesizerObject->client()->didStartSpeaking(*m_utterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);
    if (!m_utterance)
        return;

    // Clear the m_utterance variable in case finish speaking kicks off a new speaking job immediately.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> platformUtterance = m_utterance;
    m_utterance = nullptr;

    m_synthesizerObject->client()->didFinishSpeaking(*platformUtterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didPauseSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);
    if (!m_utterance)
        return;

    m_synthesizerObject->client()->didPauseSpeaking(*m_utterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didContinueSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);
    if (!m_utterance)
        return;

    m_synthesizerObject->client()->didResumeSpeaking(*m_utterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);
    if (!m_utterance)
        return;

    // Clear the m_utterance variable in case finish speaking kicks off a new speaking job immediately.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> platformUtterance = m_utterance;
    m_utterance = nullptr;

    m_synthesizerObject->client()->didFinishSpeaking(*platformUtterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer willSpeakRangeOfSpeechString:(NSRange)characterRange utterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    UNUSED_PARAM(utterance);

    if (!m_utterance)
        return;

    // iOS only supports word boundaries.
    m_synthesizerObject->client()->boundaryEventOccurred(*m_utterance, WebCore::SpeechBoundary::SpeechWordBoundary, characterRange.location);
}

@end

namespace WebCore {

PlatformSpeechSynthesizer::PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient* client)
    : m_speechSynthesizerClient(client)
{
}

PlatformSpeechSynthesizer::~PlatformSpeechSynthesizer()
{
}

void PlatformSpeechSynthesizer::initializeVoiceList()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    for (AVSpeechSynthesisVoice *voice in [PAL::getAVSpeechSynthesisVoiceClass() speechVoices]) {
        NSString *language = [voice language];
        bool isDefault = true;
        NSString *voiceURI = [voice identifier];
        NSString *name = [voice name];
        m_voiceList.append(PlatformSpeechSynthesisVoice::create(voiceURI, name, language, true, isDefault));
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformSpeechSynthesizer::pause()
{
    [m_platformSpeechWrapper.get() pause];
}

void PlatformSpeechSynthesizer::resume()
{
    [m_platformSpeechWrapper.get() resume];
}

void PlatformSpeechSynthesizer::speak(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    if (!m_platformSpeechWrapper)
        m_platformSpeechWrapper = adoptNS([[WebSpeechSynthesisWrapper alloc] initWithSpeechSynthesizer:this]);

    [m_platformSpeechWrapper.get() speakUtterance:utterance.get()];
}

void PlatformSpeechSynthesizer::cancel()
{
    [m_platformSpeechWrapper.get() cancel];
}

void PlatformSpeechSynthesizer::resetState()
{
    [m_platformSpeechWrapper.get() cancel];
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS) && PLATFORM(IOS_FAMILY)
