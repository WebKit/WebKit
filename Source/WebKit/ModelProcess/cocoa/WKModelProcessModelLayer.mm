/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 */

#import "config.h"

#if ENABLE(MODEL_PROCESS)
#import "WKModelProcessModelLayer.h"

#import "ModelProcessModelPlayerProxy.h"
#import <wtf/RefPtr.h>

@implementation WKModelProcessModelLayer {
    RefPtr<WebKit::ModelProcessModelPlayerProxy> _player;
}

- (void)setPlayer:(RefPtr<WebKit::ModelProcessModelPlayerProxy>)player
{
    _player = WTFMove(player);
}

- (RefPtr<WebKit::ModelProcessModelPlayerProxy>)player
{
    return _player;
}

- (void)setOpacity:(float)opacity
{
    [super setOpacity:opacity];

    _player->updateOpacity();
}

- (void)layoutSublayers
{
    [super layoutSublayers];

    _player->updateTransform();
}

@end


#endif // MODEL_PROCESS
