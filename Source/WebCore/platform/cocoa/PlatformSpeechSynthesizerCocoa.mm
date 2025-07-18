/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(SPEECH_SYNTHESIS) && PLATFORM(COCOA)

#import "PlatformSpeechSynthesisUtterance.h"
#import "PlatformSpeechSynthesisVoice.h"

#if __has_include(<AVFAudio/AVSpeechSynthesis.h>)
#import <AVFAudio/AVSpeechSynthesis.h>
#else
#import <AVFoundation/AVFoundation.h>
#endif

#import <pal/spi/cocoa/AXSpeechManagerSPI.h>
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

@interface WebSpeechSynthesisWrapper : NSObject<AVSpeechSynthesizerDelegate> {
    WeakPtr<WebCore::PlatformSpeechSynthesizer> m_synthesizerObject;
    // Hold a Ref to the utterance so that it won't disappear until the synth is done with it.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> m_utterance;

    const RetainPtr<AVSpeechSynthesizer> m_synthesizer;
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

#if HAVE(AVSPEECHSYNTHESIS_VOICES_CHANGE_NOTIFICATION)
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(availableVoicesDidChange) name:AVSpeechSynthesisAvailableVoicesDidChangeNotification object:nil];
#endif

    return self;
}

#if HAVE(AVSPEECHSYNTHESIS_VOICES_CHANGE_NOTIFICATION)

- (void)availableVoicesDidChange
{
    Ref { *m_synthesizerObject }->voicesDidChange();
}

#endif

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
    ASSERT(utterance);
    if (!utterance || !PAL::isAVFoundationFrameworkAvailable())
        return;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (!m_synthesizer) {
        lazyInitialize(m_synthesizer, adoptNS([PAL::allocAVSpeechSynthesizerInstance() init]));
        [m_synthesizer setDelegate:self];
    }
    
    // Choose the best voice, by first looking at the utterance voice, then the utterance language,
    // then choose the default language.
    RefPtr utteranceVoice = utterance->voice();
    RetainPtr<NSString> voiceLanguage;
    if (!utteranceVoice || utteranceVoice->voiceURI().isEmpty()) {
        if (utterance->lang().isEmpty())
            voiceLanguage = [PAL::getAVSpeechSynthesisVoiceClass() currentLanguageCode];
        else
            voiceLanguage = utterance->lang().createNSString();
    } else
        voiceLanguage = utterance->voice()->lang().createNSString();

    AVSpeechSynthesisVoice *avVoice = nil;
    if (utteranceVoice)
        avVoice = [PAL::getAVSpeechSynthesisVoiceClass() voiceWithIdentifier:utteranceVoice->voiceURI().createNSString().get()];

    if (!avVoice)
        avVoice = [PAL::getAVSpeechSynthesisVoiceClass() voiceWithLanguage:voiceLanguage.get()];

    AVSpeechUtterance *avUtterance = [PAL::getAVSpeechUtteranceClass() speechUtteranceWithString:utterance->text().createNSString().get()];

    [avUtterance setRate:[self mapSpeechRateToPlatformRate:utterance->rate()]];
    [avUtterance setVolume:utterance->volume()];
    [avUtterance setPitchMultiplier:utterance->pitch()];
    [avUtterance setVoice:avVoice];
    utterance->setWrapper(avUtterance);
    m_utterance = WTFMove(utterance);

    // macOS won't send a did start speaking callback for empty strings.
#if !HAVE(UNIFIED_SPEECHSYNTHESIS_FIX_FOR_81465164)
    if (!m_utterance->text().length())
        m_synthesizerObject->client().didStartSpeaking(Ref { *m_utterance });
#endif

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

- (void)resetState
{
    // On a reset, cancel utterance and set to nil immediately so the next speech job continues without waiting for a callback
    [self cancel];
    m_utterance = nil;
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
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    m_synthesizerObject->client().didStartSpeaking(Ref { *m_utterance });
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didFinishSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    // Clear the m_utterance variable in case finish speaking kicks off a new speaking job immediately.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> protectedUtterance = m_utterance;
    m_utterance = nullptr;

    m_synthesizerObject->client().didFinishSpeaking(*protectedUtterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didPauseSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    m_synthesizerObject->client().didPauseSpeaking(Ref { *m_utterance });
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didContinueSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    m_synthesizerObject->client().didResumeSpeaking(Ref { *m_utterance });
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer didCancelSpeechUtterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    // Clear the m_utterance variable in case finish speaking kicks off a new speaking job immediately.
    RefPtr<WebCore::PlatformSpeechSynthesisUtterance> protectedUtterance = m_utterance;
    m_utterance = nullptr;

    m_synthesizerObject->client().didFinishSpeaking(*protectedUtterance);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer willSpeakRangeOfSpeechString:(NSRange)characterRange utterance:(AVSpeechUtterance *)utterance
{
    UNUSED_PARAM(synthesizer);
    if (!m_utterance || m_utterance->wrapper() != utterance)
        return;

    // AVSpeechSynthesizer only supports word boundaries.
    m_synthesizerObject->client().boundaryEventOccurred(Ref { *m_utterance }, WebCore::SpeechBoundary::SpeechWordBoundary, characterRange.location, characterRange.length);
}

@end

namespace WebCore {

Ref<PlatformSpeechSynthesizer> PlatformSpeechSynthesizer::create(PlatformSpeechSynthesizerClient& client)
{
    return adoptRef(*new PlatformSpeechSynthesizer(client));
}

PlatformSpeechSynthesizer::PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient& client)
    : m_speechSynthesizerClient(client)
{
}

PlatformSpeechSynthesizer::~PlatformSpeechSynthesizer()
{
}

void PlatformSpeechSynthesizer::initializeVoiceList()
{
    if (!PAL::isAVFoundationFrameworkAvailable())
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    // SpeechSynthesis replaces on-device compact with higher quality compact voices. These
    // are not available to WebKit so we're losing these default voices for WebSpeech.
    // Only show built-in voices when requesting through WebKit to reduce fingerprinting surface area.
    for (AVSpeechSynthesisVoice *voice in [PAL::getAVSpeechSynthesisVoiceClass() speechVoicesIncludingSuperCompact]) {
        if (voice.isSystemVoice)
            m_voiceList.append(PlatformSpeechSynthesisVoice::create(voice.identifier, voice.name, voice.language, true, true));
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

void PlatformSpeechSynthesizer::pause()
{
    [m_platformSpeechWrapper pause];
}

void PlatformSpeechSynthesizer::resume()
{
    [m_platformSpeechWrapper resume];
}

void PlatformSpeechSynthesizer::speak(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    if (!m_platformSpeechWrapper)
        m_platformSpeechWrapper = adoptNS([[WebSpeechSynthesisWrapper alloc] initWithSpeechSynthesizer:this]);

    [m_platformSpeechWrapper speakUtterance:utterance.get()];
}

void PlatformSpeechSynthesizer::cancel()
{
    [m_platformSpeechWrapper cancel];
}

void PlatformSpeechSynthesizer::resetState()
{
    [m_platformSpeechWrapper resetState];
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS) && PLATFORM(COCOA)
