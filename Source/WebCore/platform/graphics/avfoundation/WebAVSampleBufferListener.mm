/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WebAVSampleBufferListener.h"

#import <Foundation/Foundation.h>
#import <Foundation/NSObject.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadSafetyAnalysis.h>
#import <wtf/Vector.h>

#pragma mark - Soft Linking

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
// FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
@interface AVSampleBufferDisplayLayer (Staging_113656776)
@property (nonatomic, readonly, getter=isReadyForDisplay) BOOL readyForDisplay;
@end
#endif

static NSString * const errorKeyPath = @"error";
static NSString * const outputObscuredDueToInsufficientExternalProtectionKeyPath = @"outputObscuredDueToInsufficientExternalProtection";

static void* errorContext = &errorContext;
static void* outputObscuredDueToInsufficientExternalProtectionContext = &outputObscuredDueToInsufficientExternalProtectionContext;

@interface WebAVSampleBufferListenerPrivate : NSObject {
    WeakPtr<WebCore::WebAVSampleBufferListenerClient> _client WTF_GUARDED_BY_CAPABILITY(mainThread);
    Vector<RetainPtr<AVSampleBufferDisplayLayer>> _layers;
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> _renderers;
}

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithClient:(WebCore::WebAVSampleBufferListenerClient &)client NS_DESIGNATED_INITIALIZER;
- (void)invalidate;
- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer;
- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;
- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer;

@end

@implementation WebAVSampleBufferListenerPrivate

- (instancetype)initWithClient:(WebCore::WebAVSampleBufferListenerClient &)client
{
    assertIsMainThread();
    if (!(self = [super init]))
        return nil;

    _client = client;
    return self;
}

- (void)invalidate
{
    assertIsMainThread();

    _client = nullptr;

    for (auto& layer : _layers) {
        [layer removeObserver:self forKeyPath:errorKeyPath];
        [layer removeObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath];
    }
    _layers.clear();

    for (auto& renderer : _renderers)
        [renderer removeObserver:self forKeyPath:errorKeyPath];
    _renderers.clear();

    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)beginObservingLayer:(AVSampleBufferDisplayLayer *)layer
{
    ASSERT(isMainThread());
    ASSERT(!_layers.contains(layer));

    _layers.append(layer);
    [layer addObserver:self forKeyPath:errorKeyPath options:NSKeyValueObservingOptionNew context:errorContext];
    [layer addObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath options:NSKeyValueObservingOptionNew context:outputObscuredDueToInsufficientExternalProtectionContext];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerFailedToDecode:) name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerRequiresFlushToResumeDecodingChanged:) name:AVSampleBufferDisplayLayerRequiresFlushToResumeDecodingDidChangeNotification object:layer];
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
    // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
    if (PAL::canLoad_AVFoundation_AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification())
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(layerReadyForDisplayChanged:) name:AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification object:layer];
#endif
}

- (void)stopObservingLayer:(AVSampleBufferDisplayLayer *)layer
{
    ASSERT(isMainThread());
    ASSERT(_layers.contains(layer));

    [layer removeObserver:self forKeyPath:errorKeyPath];
    [layer removeObserver:self forKeyPath:outputObscuredDueToInsufficientExternalProtectionKeyPath];
    _layers.remove(_layers.find(layer));

    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerFailedToDecodeNotification object:layer];
    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerRequiresFlushToResumeDecodingDidChangeNotification object:layer];
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
    // FIXME (117934497): Remove staging code once -[AVSampleBufferDisplayLayer isReadyForDisplay] is available in SDKs used by WebKit builders
    if (PAL::canLoad_AVFoundation_AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification())
        [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferDisplayLayerReadyForDisplayDidChangeNotification object:layer];
#endif
}

- (void)beginObservingRenderer:(AVSampleBufferAudioRenderer *)renderer
{
    ASSERT(isMainThread());
    ASSERT(!_renderers.contains(renderer));

    _renderers.append(renderer);
    [renderer addObserver:self forKeyPath:errorKeyPath options:NSKeyValueObservingOptionNew context:errorContext];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(audioRendererWasAutomaticallyFlushed:) name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];
}

- (void)stopObservingRenderer:(AVSampleBufferAudioRenderer *)renderer
{
    ASSERT(isMainThread());
    ASSERT(_renderers.contains(renderer));

    [renderer removeObserver:self forKeyPath:errorKeyPath];
    [NSNotificationCenter.defaultCenter removeObserver:self name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:renderer];

    _renderers.remove(_renderers.find(renderer));
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
{
    if (context == errorContext) {
        RetainPtr error = dynamic_objc_cast<NSError>([change valueForKey:NSKeyValueChangeNewKey]);
        if (!error)
            return;

        if ([object isKindOfClass:PAL::getAVSampleBufferDisplayLayerClass()]) {
            RetainPtr layer = (AVSampleBufferDisplayLayer *)object;

            ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), error = WTFMove(error)] {
                ASSERT(_layers.contains(layer.get()));
                if (auto client = _client.get())
                    client->layerDidReceiveError(layer.get(), error.get());
            });
            return;
        }

        if ([object isKindOfClass:PAL::getAVSampleBufferAudioRendererClass()]) {
            RetainPtr renderer = (AVSampleBufferAudioRenderer *)object;

            ensureOnMainThread([self, protectedSelf = RetainPtr { self }, renderer = WTFMove(renderer), error = WTFMove(error)] {
                ASSERT(_renderers.contains(renderer.get()));
                if (auto client = _client.get())
                    client->rendererDidReceiveError(renderer.get(), error.get());
            });
            return;
        }

        ASSERT_NOT_REACHED_WITH_MESSAGE("Unexpected kind of object while observing errorContext");
        return;
    }

    if (context == outputObscuredDueToInsufficientExternalProtectionContext) {
        RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(object, PAL::getAVSampleBufferDisplayLayerClass());
        BOOL isObscured = [[change valueForKey:NSKeyValueChangeNewKey] boolValue];

        ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), isObscured] {
            ASSERT(_layers.contains(layer.get()));
            if (auto client = _client.get())
                client->outputObscuredDueToInsufficientExternalProtectionChanged(isObscured);
        });
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (void)layerFailedToDecode:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    RetainPtr error = dynamic_objc_cast<NSError>([notification.userInfo valueForKey:AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey]);

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), error = WTFMove(error)] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto client = _client.get())
            client->layerDidReceiveError(layer.get(), error.get());
    });
}

- (void)layerRequiresFlushToResumeDecodingChanged:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    BOOL requiresFlush = [layer requiresFlushToResumeDecoding];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), requiresFlush] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto client = _client.get())
            client->layerRequiresFlushToResumeDecodingChanged(layer.get(), requiresFlush);
    });
}

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_READYFORDISPLAY)
- (void)layerReadyForDisplayChanged:(NSNotification *)notification
{
    RetainPtr layer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(notification.object, PAL::getAVSampleBufferDisplayLayerClass());
    BOOL isReadyForDisplay = [layer isReadyForDisplay];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, layer = WTFMove(layer), isReadyForDisplay] {
        if (!_layers.contains(layer.get()))
            return;
        if (auto client = _client.get())
            client->layerReadyForDisplayChanged(layer.get(), isReadyForDisplay);
    });
}
#endif

- (void)audioRendererWasAutomaticallyFlushed:(NSNotification *)notification
{
    RetainPtr renderer = dynamic_objc_cast<AVSampleBufferAudioRenderer>(notification.object, PAL::getAVSampleBufferAudioRendererClass());
    CMTime flushTime = [[notification.userInfo valueForKey:AVSampleBufferAudioRendererFlushTimeKey] CMTimeValue];

    ensureOnMainThread([self, protectedSelf = RetainPtr { self }, renderer = WTFMove(renderer), flushTime] {
        if (!_renderers.contains(renderer.get()))
            return;
        if (auto client = _client.get())
            client->rendererWasAutomaticallyFlushed(renderer.get(), flushTime);
    });
}

@end

namespace WebCore {

WebAVSampleBufferListener::WebAVSampleBufferListener(WebAVSampleBufferListenerClient& client)
    : m_private(adoptNS([[WebAVSampleBufferListenerPrivate alloc] initWithClient:client]))
{
}

void WebAVSampleBufferListener::invalidate()
{
    [m_private invalidate];
}

void WebAVSampleBufferListener::beginObservingLayer(AVSampleBufferDisplayLayer* layer)
{
    [m_private beginObservingLayer:layer];
}

void WebAVSampleBufferListener::stopObservingLayer(AVSampleBufferDisplayLayer* layer)
{
    [m_private stopObservingLayer:layer];
}

void WebAVSampleBufferListener::beginObservingRenderer(AVSampleBufferAudioRenderer* renderer)
{
    [m_private beginObservingRenderer:renderer];
}

void WebAVSampleBufferListener::stopObservingRenderer(AVSampleBufferAudioRenderer* renderer)
{
    [m_private stopObservingRenderer:renderer];
}

} // namespace WebCore
