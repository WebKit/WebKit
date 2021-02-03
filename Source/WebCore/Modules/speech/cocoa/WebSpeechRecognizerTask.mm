/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebSpeechRecognizerTask.h"

#if HAVE(SPEECHRECOGNIZER)

#import <pal/spi/cocoa/SpeechSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakObjCPtr.h>

#import <pal/cocoa/SpeechSoftLink.h>

// Set the maximum duration to be an hour; we can adjust this if needed.
static constexpr size_t maximumRecognitionDuration = 60 * 60;

using namespace WebCore;

NS_ASSUME_NONNULL_BEGIN

@interface WebSpeechRecognizerTaskImpl : NSObject<SFSpeechRecognitionTaskDelegate, SFSpeechRecognizerDelegate> {
@private
    SpeechRecognitionConnectionClientIdentifier _identifier;
    BlockPtr<void(const SpeechRecognitionUpdate&)> _delegateCallback;
    bool _doMultipleRecognitions;
    uint64_t _maxAlternatives;
    RetainPtr<SFSpeechRecognizer> _recognizer;
    RetainPtr<SFSpeechAudioBufferRecognitionRequest> _request;
    WeakObjCPtr<SFSpeechRecognitionTask> _task;
    bool _hasSentSpeechStart;
    bool _hasSentSpeechEnd;
    bool _hasSentEnd;
}

- (instancetype)initWithIdentifier:(SpeechRecognitionConnectionClientIdentifier)identifier locale:(NSString*)localeIdentifier doMultipleRecognitions:(BOOL)continuous reportInterimResults:(BOOL)interimResults maxAlternatives:(unsigned long)alternatives delegateCallback:(void(^)(const SpeechRecognitionUpdate&))callback;
- (void)callbackWithTranscriptions:(NSArray<SFTranscription *> *)transcriptions isFinal:(BOOL)isFinal;
- (void)audioSamplesAvailable:(CMSampleBufferRef)sampleBuffer;
- (void)abort;
- (void)stop;
- (void)sendSpeechStartIfNeeded;
- (void)sendSpeechEndIfNeeded;
- (void)sendEndIfNeeded;

@end

@implementation WebSpeechRecognizerTaskImpl

- (instancetype)initWithIdentifier:(SpeechRecognitionConnectionClientIdentifier)identifier locale:(NSString*)localeIdentifier doMultipleRecognitions:(BOOL)continuous reportInterimResults:(BOOL)interimResults maxAlternatives:(unsigned long)alternatives delegateCallback:(void(^)(const SpeechRecognitionUpdate&))callback
{
    if (!(self = [super init]))
        return nil;

    _identifier = identifier;
    _doMultipleRecognitions = continuous;
    _delegateCallback = callback;
    _hasSentSpeechStart = false;
    _hasSentSpeechEnd = false;
    _hasSentEnd = false;

    _maxAlternatives = alternatives ? alternatives : 1;

    if (![localeIdentifier length])
        _recognizer = adoptNS([PAL::allocSFSpeechRecognizerInstance() init]);
    else
        _recognizer = adoptNS([PAL::allocSFSpeechRecognizerInstance() initWithLocale:[NSLocale localeWithLocaleIdentifier:localeIdentifier]]);
    if (!_recognizer) {
        [self release];
        return nil;
    }

    if (![_recognizer isAvailable]) {
        [self release];
        return nil;
    }

    [_recognizer setDelegate:self];

    _request = adoptNS([PAL::allocSFSpeechAudioBufferRecognitionRequestInstance() init]);
    if ([_recognizer supportsOnDeviceRecognition])
        [_request setRequiresOnDeviceRecognition:YES];
    [_request setShouldReportPartialResults:interimResults];
    [_request setTaskHint:SFSpeechRecognitionTaskHintDictation];

#if USE(APPLE_INTERNAL_SDK)
    [_request setDetectMultipleUtterances:YES];
    [_request _setMaximumRecognitionDuration:maximumRecognitionDuration];
#endif

    _task = [_recognizer recognitionTaskWithRequest:_request.get() delegate:self];
    return self;
}

- (void)callbackWithTranscriptions:(NSArray<SFTranscription *> *)transcriptions isFinal:(BOOL)isFinal
{
    Vector<SpeechRecognitionAlternativeData> alternatives;
    alternatives.reserveInitialCapacity(_maxAlternatives);
    for (SFTranscription* transcription in transcriptions) {
        // FIXME: <rdar://73629573> get confidence of SFTranscription when possible.
        double maxConfidence = 0.0;
        for (SFTranscriptionSegment* segment in [transcription segments]) {
            double confidence = [segment confidence];
            maxConfidence = maxConfidence < confidence ? confidence : maxConfidence;
        }
        alternatives.uncheckedAppend(SpeechRecognitionAlternativeData { [transcription formattedString], maxConfidence });
        if (alternatives.size() == _maxAlternatives)
            break;
    }
    Vector<SpeechRecognitionResultData> datas;
    datas.append(SpeechRecognitionResultData { WTFMove(alternatives), bool(isFinal) });
    _delegateCallback(SpeechRecognitionUpdate::createResult(_identifier, WTFMove(datas)));
}

- (void)audioSamplesAvailable:(CMSampleBufferRef)sampleBuffer
{
    ASSERT(isMainThread());
    [_request appendAudioSampleBuffer:sampleBuffer];
}

- (void)abort
{
    if (!_task || [_task state] == SFSpeechRecognitionTaskStateCanceling)
        return;

    if ([_task state] == SFSpeechRecognitionTaskStateCompleted) {
        [self sendSpeechEndIfNeeded];
        [self sendEndIfNeeded];
        return;
    }

    [self sendSpeechEndIfNeeded];
    [_request endAudio];
    [_task cancel];
}

- (void)stop
{
    if (!_task || [_task state] == SFSpeechRecognitionTaskStateCanceling)
        return;

    if ([_task state] == SFSpeechRecognitionTaskStateCompleted) {
        [self sendSpeechEndIfNeeded];
        [self sendEndIfNeeded];
        return;
    }

    [self sendSpeechEndIfNeeded];
    [_request endAudio];
    [_task finish];
}

- (void)sendSpeechStartIfNeeded
{
    if (_hasSentSpeechStart)
        return;

    _hasSentSpeechStart = true;
    _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::SpeechStart));
}

- (void)sendSpeechEndIfNeeded
{
    if (!_hasSentSpeechStart || _hasSentSpeechEnd)
        return;

    _hasSentSpeechEnd = true;
    _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::SpeechEnd));
}

- (void)sendEndIfNeeded
{
    if (_hasSentEnd)
        return;

    _hasSentEnd = true;
    _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::End));
}

#pragma mark SFSpeechRecognizerDelegate

- (void)speechRecognizer:(SFSpeechRecognizer *)speechRecognizer availabilityDidChange:(BOOL)available
{
    ASSERT(isMainThread());

    if (available || !_task)
        return;

    auto error = SpeechRecognitionError { SpeechRecognitionErrorType::ServiceNotAllowed, "Speech recognition service becomes unavailable"_s };
    _delegateCallback(SpeechRecognitionUpdate::createError(_identifier, WTFMove(error)));
}

#pragma mark SFSpeechRecognitionTaskDelegate

- (void)speechRecognitionTask:(SFSpeechRecognitionTask *)task didHypothesizeTranscription:(SFTranscription *)transcription
{
    ASSERT(isMainThread());

    [self sendSpeechStartIfNeeded];
    [self callbackWithTranscriptions:[NSArray arrayWithObjects:transcription, nil] isFinal:NO];
}

- (void)speechRecognitionTask:(SFSpeechRecognitionTask *)task didFinishRecognition:(SFSpeechRecognitionResult *)recognitionResult
{
    ASSERT(isMainThread());
    [self callbackWithTranscriptions:recognitionResult.transcriptions isFinal:YES];

    if (!_doMultipleRecognitions) {
        [self sendSpeechEndIfNeeded];
        [self sendEndIfNeeded];
    }
}

- (void)speechRecognitionTaskWasCancelled:(SFSpeechRecognitionTask *)task
{
    ASSERT(isMainThread());

    [self sendSpeechEndIfNeeded];
    [self sendEndIfNeeded];
}

- (void)speechRecognitionTask:(SFSpeechRecognitionTask *)task didFinishSuccessfully:(BOOL)successfully
{
    ASSERT(isMainThread());

    if (!successfully) {
        auto error = SpeechRecognitionError { SpeechRecognitionErrorType::Aborted, task.error.localizedDescription };
        _delegateCallback(SpeechRecognitionUpdate::createError(_identifier, WTFMove(error)));
    }
    
    [self sendEndIfNeeded];
}

@end

@implementation WebSpeechRecognizerTask

- (instancetype)initWithIdentifier:(SpeechRecognitionConnectionClientIdentifier)identifier locale:(NSString*)localeIdentifier doMultipleRecognitions:(BOOL)continuous reportInterimResults:(BOOL)interimResults maxAlternatives:(unsigned long)alternatives delegateCallback:(void(^)(const SpeechRecognitionUpdate&))callback
{
    if (!(self = [super init]))
        return nil;

    _impl = adoptNS([[WebSpeechRecognizerTaskImpl alloc] initWithIdentifier:identifier locale:localeIdentifier doMultipleRecognitions:continuous reportInterimResults:interimResults maxAlternatives:alternatives delegateCallback:callback]);

    if (!_impl) {
        [self release];
        return nil;
    }

    return self;
}

- (void)audioSamplesAvailable:(CMSampleBufferRef)sampleBuffer
{
    [_impl audioSamplesAvailable:sampleBuffer];
}

- (void)abort
{
    [_impl abort];
}

- (void)stop
{
    [_impl stop];
}

@end

NS_ASSUME_NONNULL_END

#endif
