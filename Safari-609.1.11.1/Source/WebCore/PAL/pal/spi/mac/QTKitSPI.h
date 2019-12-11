/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if USE(APPLE_INTERNAL_SDK)

#import <QTKit/QTHUDBackgroundView_Private.h>
#import <QTKit/QTKit_Private.h>
#import <QTKit/QTUtilities_Private.h>

#else

enum {
    kQTTimeIsIndefinite = 1 << 0
};

typedef struct {
    long long timeValue;
    long timeScale;
    long flags;
} QTTime;

typedef struct {
    QTTime time;
    QTTime duration;
} QTTimeRange;

typedef enum {
    QTIncludeCommonTypes = 0,
    QTIncludeOnlyFigMediaFileTypes = 1 << 8,
} QTMovieFileTypeOptions;

enum {
    QTMovieTypeUnknown = 0,
    QTMovieTypeLocal,
    QTMovieTypeFastStart,
    QTMovieTypeLiveStream,
    QTMovieTypeStoredStream
};

@class QTTrack;

@interface QTMedia : NSObject

- (id)attributeForKey:(NSString *)attributeKey;

@end

@interface QTMovie : NSObject <NSCoding, NSCopying>

+ (NSArray *)movieFileTypes:(QTMovieFileTypeOptions)types;

+ (void)disableComponent:(ComponentDescription)component;

- (id)initWithAttributes:(NSDictionary *)attributes error:(NSError **)errorPtr;

- (void)play;
- (void)stop;

- (QTTime)duration;

- (QTTime)currentTime;
- (void)setCurrentTime:(QTTime)time;

- (float)rate;
- (void)setRate:(float)rate;

- (float)volume;
- (void)setVolume:(float)volume;

- (NSDictionary *)movieAttributes;

- (id)attributeForKey:(NSString *)attributeKey;
- (void)setAttribute:(id)value forKey:(NSString *)attributeKey;

- (NSArray *)tracks;

- (NSArray *)alternateGroupTypes;
- (NSArray *)alternatesForMediaType:(NSString *)type;
- (void)deselectAlternateGroupTrack:(QTTrack *)alternateTrack;
- (void)selectAlternateGroupTrack:(QTTrack *)alternateTrack;

- (NSURL *)URL;
- (UInt32)movieType;

@end

@interface QTMovieLayer : CALayer

- (void)setMovie:(QTMovie *)movie;
- (QTMovie *)movie;

@end

@interface NSValue (NSValueQTTimeRangeExtensions)
+ (NSValue *)valueWithQTTimeRange:(QTTimeRange)range;
- (QTTimeRange)QTTimeRangeValue;
@end

@interface QTTrack : NSObject

- (QTMedia *)media;

- (BOOL)isEnabled;
- (void)setEnabled:(BOOL)enabled;

- (id)attributeForKey:(NSString *)attributeKey;

@end

@interface QTHUDBackgroundView : NSView

- (void)setContentBorderPosition:(CGFloat)contentBorderPosition;

@end

@interface QTUtilities : NSObject

+ (id)qtUtilities;

- (NSArray *)sitesInDownloadCache;
- (void)clearDownloadCache;
- (void)clearDownloadCacheForSite:(NSString *)site;

@end

#endif
