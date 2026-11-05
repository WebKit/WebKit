/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(AVKIT)

#include "Helpers/Utilities.h"
#include <QuartzCore/QuartzCore.h>
#include <WebCore/FloatRect.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/VideoPresentationModel.h>
#include <WebCore/WebAVPlayerLayer.h>
#include <wtf/RetainPtr.h>
#include <wtf/Seconds.h>

@interface TestNullActionLayerDelegate : NSObject <CALayerDelegate>
@end

@implementation TestNullActionLayerDelegate
- (id<CAAction>)actionForLayer:(CALayer *)layer forKey:(NSString *)key
{
    UNUSED_PARAM(layer);
    UNUSED_PARAM(key);
    return (id<CAAction>)[NSNull null];
}
@end

namespace TestWebKitAPI {

class TestVideoPresentationModel final : public WebCore::VideoPresentationModel {
public:
    static Ref<TestVideoPresentationModel> create(WebCore::FloatSize videoDimensions)
    {
        return adoptRef(*new TestVideoPresentationModel(videoDimensions));
    }

    void setResolvedFlag(bool* flag) { m_resolvedFlag = flag; }
    WebCore::FloatRect lastVideoLayerFrame() const { return m_lastVideoLayerFrame; }

    void addClient(WebCore::VideoPresentationModelClient&) final { }
    void removeClient(WebCore::VideoPresentationModelClient&) final { }
    void requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool) final { }
    void setVideoLayerGravity(WebCore::MediaPlayerEnums::VideoGravity) final { }
    void setVideoFullscreenFrame(WebCore::FloatRect) final { }
    void fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode, ShouldNotifyMediaElement) final { }
    bool hasVideo() const final { return true; }
    bool isChildOfElementFullscreen() const final { return false; }
    void willEnterPictureInPicture() final { }
    void didEnterPictureInPicture() final { }
    void failedToEnterPictureInPicture() final { }
    void willExitPictureInPicture() final { }
    void didExitPictureInPicture() final { }

    WebCore::FloatSize videoDimensions() const final { return m_videoDimensions; }

    void setVideoLayerFrame(WebCore::FloatRect frame) final
    {
        m_lastVideoLayerFrame = frame;
        if (m_resolvedFlag)
            *m_resolvedFlag = true;
    }

private:
    explicit TestVideoPresentationModel(WebCore::FloatSize videoDimensions)
        : m_videoDimensions(videoDimensions)
    {
    }

    WebCore::FloatSize m_videoDimensions;
    WebCore::FloatRect m_lastVideoLayerFrame;
    bool* m_resolvedFlag { nullptr };
};

static constexpr CGRect fullscreenVideoFrame = { { 0, 0 }, { 2560, 1440 } };
static constexpr CGRect inlineVideoFrame = { { 0, 0 }, { 1280, 720 } };

static RetainPtr<WebAVPlayerLayer> createPlayerLayerShrinkingToInline(Ref<TestVideoPresentationModel> model, CALayer *videoSublayer)
{
    RetainPtr playerLayer = adoptNS([[WebAVPlayerLayer alloc] init]);
    [videoSublayer setBounds:fullscreenVideoFrame];
    [playerLayer addSublayer:videoSublayer];
    [playerLayer setVideoSublayer:videoSublayer];
    [playerLayer setPresentationModel:model.ptr()];
    [playerLayer setBounds:fullscreenVideoFrame];
    return playerLayer;
}

TEST(WebAVPlayerLayer, ResolvesBoundsPromptlyWhenSublayerDoesNotAnimate)
{
    auto model = TestVideoPresentationModel::create({ 2560, 1440 });

    RetainPtr delegate = adoptNS([[TestNullActionLayerDelegate alloc] init]);
    RetainPtr videoSublayer = adoptNS([[CALayer alloc] init]);
    [videoSublayer setDelegate:delegate.get()];

    RetainPtr playerLayer = createPlayerLayerShrinkingToInline(model.copyRef(), videoSublayer.get());

    bool resolved = false;
    model->setResolvedFlag(&resolved);

    [videoSublayer setBounds:fullscreenVideoFrame];
    [videoSublayer setAffineTransform:CGAffineTransformIdentity];
    [playerLayer setBounds:inlineVideoFrame];
    [playerLayer layoutSublayers];

    EXPECT_TRUE(Util::runFor(&resolved, 0.1_s));
    EXPECT_EQ(model->lastVideoLayerFrame().width(), inlineVideoFrame.size.width);
    EXPECT_EQ(model->lastVideoLayerFrame().height(), inlineVideoFrame.size.height);
}

TEST(WebAVPlayerLayer, DefersResolvingBoundsWhileSublayerAnimates)
{
    auto model = TestVideoPresentationModel::create({ 2560, 1440 });

    RetainPtr videoSublayer = adoptNS([[CALayer alloc] init]);
    RetainPtr playerLayer = createPlayerLayerShrinkingToInline(model.copyRef(), videoSublayer.get());

    bool resolved = false;
    model->setResolvedFlag(&resolved);

    [videoSublayer setBounds:fullscreenVideoFrame];
    [videoSublayer setAffineTransform:CGAffineTransformIdentity];

    RetainPtr animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:10];
    [videoSublayer addAnimation:animation.get() forKey:@"testAnimation"];

    [playerLayer setBounds:inlineVideoFrame];
    [playerLayer layoutSublayers];

    EXPECT_FALSE(Util::runFor(&resolved, 0.1_s));
    EXPECT_TRUE(Util::runFor(&resolved, 2_s));
}

} // namespace TestWebKitAPI

#endif // HAVE(AVKIT)
