/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKUIScrollEdgeEffect.h"

#if PLATFORM(IOS_FAMILY) && HAVE(LIQUID_GLASS)

#import <UIKit/UIKit.h>
#import <wtf/OptionSet.h>

namespace WebKit {

enum class HiddenScrollEdgeEffectSource : uint8_t {
    Internal    = 1 << 0,
    Client      = 1 << 1
};

} // namespace WebKit

@implementation WKUIScrollEdgeEffect {
    __weak UIScrollEdgeEffect *_effect;
    OptionSet<WebKit::HiddenScrollEdgeEffectSource> _hiddenSources;
    WebCore::BoxSide _boxSide;
}

- (instancetype)initWithScrollEdgeEffect:(UIScrollEdgeEffect *)effect boxSide:(WebCore::BoxSide)boxSide
{
    if (!(self = [super init]))
        return nil;

    _boxSide = boxSide;
    _effect = effect;
    _hiddenSources = { };
    return self;
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    return _effect;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [super respondsToSelector:selector] || [_effect respondsToSelector:selector];
}

- (BOOL)isKindOfClass:(Class)classToCheck
{
    return [super isKindOfClass:classToCheck] || [_effect isKindOfClass:classToCheck];
}

- (void)setInternallyHidden:(BOOL)hidden
{
    [self _setHidden:hidden fromSource:WebKit::HiddenScrollEdgeEffectSource::Internal];
}

- (BOOL)isInternallyHidden
{
    return _hiddenSources.contains(WebKit::HiddenScrollEdgeEffectSource::Internal);
}

- (void)setHidden:(BOOL)hidden
{
    [self _setHidden:hidden fromSource:WebKit::HiddenScrollEdgeEffectSource::Client];
}

- (void)_setHidden:(BOOL)hidden fromSource:(WebKit::HiddenScrollEdgeEffectSource)source
{
    bool oldVisible = !_hiddenSources;
    if (hidden)
        _hiddenSources.add(source);
    else
        _hiddenSources.remove(source);

    bool newVisible = !_hiddenSources;
    if (oldVisible == newVisible)
        return;

    _effect.hidden = !newVisible;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p wrapping %@ (%s)>", self.class, self, _effect.description, [&] {
        switch (_boxSide) {
        case WebCore::BoxSide::Top:
            return "Top";
        case WebCore::BoxSide::Right:
            return "Right";
        case WebCore::BoxSide::Bottom:
            return "Bottom";
        case WebCore::BoxSide::Left:
            return "Left";
        }
        ASSERT_NOT_REACHED();
        return "?";
    }()];
}

@end

#endif // PLATFORM(IOS_FAMILY) && HAVE(LIQUID_GLASS)
