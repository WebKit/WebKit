/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 */

#import "config.h"

#if ENABLE(MODEL_PROCESS)

#import "WKModelProcessModelLayer.h"

#import "ModelProcessModelPlayerProxy.h"
#import <wtf/RefPtr.h>

@implementation WKModelProcessModelLayer {
    WeakPtr<WebKit::ModelProcessModelPlayerProxy> _player;
}

- (void)setPlayer:(WeakPtr<WebKit::ModelProcessModelPlayerProxy>)player
{
    _player = player;
}

- (WeakPtr<WebKit::ModelProcessModelPlayerProxy>)player
{
    return _player;
}

- (void)setOpacity:(float)opacity
{
    [super setOpacity:opacity];

    if (RefPtr strongPlayer = _player.get())
        strongPlayer->updateOpacity();
}

- (void)layoutSublayers
{
    [super layoutSublayers];

    if (RefPtr strongPlayer = _player.get())
        strongPlayer->updateTransformAfterLayout();
}

@end

#endif // MODEL_PROCESS
