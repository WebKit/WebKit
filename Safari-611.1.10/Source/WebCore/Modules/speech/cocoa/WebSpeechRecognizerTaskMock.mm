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
#import "WebSpeechRecognizerTaskMock.h"

#if HAVE(SPEECHRECOGNIZER)

using namespace WebCore;

NS_ASSUME_NONNULL_BEGIN

@implementation WebSpeechRecognizerTaskMock

- (instancetype)initWithIdentifier:(SpeechRecognitionConnectionClientIdentifier)identifier locale:(NSString*)localeIdentifier doMultipleRecognitions:(BOOL)continuous reportInterimResults:(BOOL)interimResults maxAlternatives:(unsigned long)alternatives delegateCallback:(void(^)(const SpeechRecognitionUpdate&))callback
{
    if (!(self = [super init]))
        return nil;

    _doMultipleRecognitions = continuous;
    _identifier = identifier;
    _delegateCallback = callback;
    _completed = false;
    _hasSentSpeechStart = false;
    _hasSentSpeechEnd = false;

    return self;
}

- (void)audioSamplesAvailable:(CMSampleBufferRef)sampleBuffer
{
    if (!_hasSentSpeechStart) {
        _hasSentSpeechStart = true;
        _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::SpeechStart));
    }

    // Fake some recognition results.
    Vector<SpeechRecognitionAlternativeData> alternatives;
    alternatives.append(SpeechRecognitionAlternativeData { "Test", 1.0 });
    Vector<SpeechRecognitionResultData> datas;
    datas.append(SpeechRecognitionResultData { WTFMove(alternatives), true  });
    _delegateCallback(SpeechRecognitionUpdate::createResult(_identifier, WTFMove(datas)));

    if (!_doMultipleRecognitions)
        [self abort];
}

- (void)abort
{
    if (_completed)
        return;
    _completed = true;

    if (!_hasSentSpeechEnd && _hasSentSpeechStart) {
        _hasSentSpeechEnd = true;
        _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::SpeechEnd));
    }

    _delegateCallback(SpeechRecognitionUpdate::create(_identifier, SpeechRecognitionUpdateType::End));
}

- (void)stop
{
    [self abort];
}

@end

NS_ASSUME_NONNULL_END

#endif
