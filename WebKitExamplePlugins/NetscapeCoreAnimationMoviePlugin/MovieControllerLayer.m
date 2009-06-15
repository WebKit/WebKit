/*
     File: MovieControllerLayer.m
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
 Inc. ("Apple") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Apple software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Inc. may
 be used to endorse or promote products derived from the Apple Software
 without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2009 Apple Inc. All Rights Reserved.
 
 */

#import "MovieControllerLayer.h"

#import <QTKit/QTKit.h>

@interface MovieControllerLayer ()
- (BOOL)_isPlaying;
- (NSTimeInterval)_currentTime;
- (NSTimeInterval)_duration;
@end

@implementation MovieControllerLayer

static CGImageRef createImageNamed(NSString *name)
{
    NSURL *url = [[NSBundle bundleForClass:[MovieControllerLayer class]] URLForResource:name withExtension:@"tiff"];
    
    if (!url)
        return NULL;
    
    CGImageSourceRef imageSource = CGImageSourceCreateWithURL((CFURLRef)url, NULL);
    if (!imageSource)
        return NULL;
    
    CGImageRef image = CGImageSourceCreateImageAtIndex(imageSource, 0, NULL);
    CFRelease(imageSource);
    
    return image;
}

- (id)init
{
    if (self = [super init]) {
        self.needsDisplayOnBoundsChange = YES;
        self.frame = CGRectMake(0, 0, 0, 25);
        self.autoresizingMask = kCALayerWidthSizable;
        
        _playImage = createImageNamed(@"Play");
        _pauseImage = createImageNamed(@"Pause");
        _sliderTrackLeft = createImageNamed(@"SliderTrackLeft");
        _sliderTrackRight = createImageNamed(@"SliderTrackRight");
        _sliderTrackCenter = createImageNamed(@"SliderTrackCenter");
        
        _thumb = createImageNamed(@"Thumb");
    }
    
    return self;
}

- (void)dealloc
{
    CGImageRelease(_playImage);
    CGImageRelease(_pauseImage);
    
    CGImageRelease(_sliderTrackLeft);
    CGImageRelease(_sliderTrackRight);
    CGImageRelease(_sliderTrackCenter);
    
    CGImageRelease(_thumb);
    
    [self setMovie:nil];
    [_updateTimeTimer invalidate];

    [super dealloc];
}

#pragma mark Drawing

- (CGRect)_playPauseButtonRect
{
    return CGRectMake(0, 0, 25, 25);
}

- (CGRect)_sliderRect
{
    CGFloat sliderYPosition = (self.bounds.size.height - CGImageGetHeight(_sliderTrackLeft)) / 2.0;
    CGFloat playPauseButtonWidth = [self _playPauseButtonRect].size.width;
    
    return CGRectMake(playPauseButtonWidth, sliderYPosition, 
                      self.bounds.size.width - playPauseButtonWidth - 7, CGImageGetHeight(_sliderTrackLeft));
}

- (CGRect)_sliderThumbRect
{
    CGRect sliderRect = [self _sliderRect];

    CGFloat fraction = 0.0;
    if (_movie)
        fraction = [self _currentTime] / [self _duration];
  
    CGFloat x = fraction * (CGRectGetWidth(sliderRect) - CGImageGetWidth(_thumb));
    
    return CGRectMake(CGRectGetMinX(sliderRect) + x, CGRectGetMinY(sliderRect) - 1, 
                      CGImageGetWidth(_thumb), CGImageGetHeight(_thumb));
}

- (CGRect)_innerSliderRect
{
    return CGRectInset([self _sliderRect], CGRectGetWidth([self _sliderThumbRect]) / 2, 0);
}

- (void)_drawPlayPauseButtonInContext:(CGContextRef)context
{
    CGContextDrawImage(context, [self _playPauseButtonRect], [self _isPlaying] ? _pauseImage : _playImage);
}

- (void)_drawSliderInContext:(CGContextRef)context
{
    // Draw the thumb
    CGRect sliderThumbRect = [self _sliderThumbRect];
    CGContextDrawImage(context, sliderThumbRect, _thumb);
    
    CGRect sliderRect = [self _sliderRect];
    
    // Draw left part
    CGRect sliderLeftTrackRect = CGRectMake(CGRectGetMinX(sliderRect), CGRectGetMinY(sliderRect), 
                                            CGImageGetWidth(_sliderTrackLeft), CGImageGetHeight(_sliderTrackLeft));
    CGContextDrawImage(context, sliderLeftTrackRect, _sliderTrackLeft);
    
    // Draw center part
    CGRect sliderCenterTrackRect = CGRectInset(sliderRect, CGImageGetWidth(_sliderTrackLeft), 0);
    CGContextDrawImage(context, sliderCenterTrackRect, _sliderTrackCenter);
    
    // Draw right part
    CGRect sliderRightTrackRect = CGRectMake(CGRectGetMaxX(sliderCenterTrackRect), CGRectGetMinY(sliderRect), 
                                             CGImageGetWidth(_sliderTrackRight), CGImageGetHeight(_sliderTrackRight));
    CGContextDrawImage(context, sliderRightTrackRect, _sliderTrackRight);
    
}

- (void)drawInContext:(CGContextRef)context
{
    CGContextSaveGState(context);
    CGContextSetFillColorWithColor(context, CGColorGetConstantColor(kCGColorBlack));
    CGContextFillRect(context, self.bounds);
    CGContextRestoreGState(context);
    
    [self _drawPlayPauseButtonInContext:context];
    [self _drawSliderInContext:context];
}

#pragma mark Movie handling

- (NSTimeInterval)_currentTime
{
    if (!_movie)
        return 0;
    
    QTTime time = [_movie currentTime];
    NSTimeInterval timeInterval;
    if (!QTGetTimeInterval(time, &timeInterval))
        return 0;
    
    return timeInterval;
}

- (NSTimeInterval)_duration
{
    if (!_movie)
        return 0;
    
    QTTime time = [_movie duration];
    NSTimeInterval timeInterval;
    if (!QTGetTimeInterval(time, &timeInterval))
        return 0;
    
    return timeInterval;
}

- (BOOL)_isPlaying
{
    return [_movie rate] != 0.0;
}

- (void)_updateTime:(NSTimer *)timer
{
    [self setNeedsDisplay];
}

- (void)_rateDidChange:(NSNotification *)notification
{
    float rate = [[[notification userInfo] objectForKey:QTMovieRateDidChangeNotificationParameter] floatValue];
    
    if (rate == 0.0) {
        [_updateTimeTimer invalidate];
        _updateTimeTimer = nil;
    } else
        _updateTimeTimer = [NSTimer scheduledTimerWithTimeInterval:0.035 target:self selector:@selector(_updateTime:) userInfo:nil repeats:YES];
    
    [self setNeedsDisplay];
}

- (void)_timeDidChange:(NSNotification *)notification
{
    [self setNeedsDisplay];
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    // We don't want to animate the contents of the layer.
    if ([key isEqualToString:@"contents"])
        return nil;
    
    return [super actionForKey:key];
}

- (void)setMovie:(QTMovie *)movie
{
    if (_movie == movie)
        return;
    
    if (_movie) {
        [[NSNotificationCenter defaultCenter] removeObserver:self 
                                                        name:QTMovieRateDidChangeNotification 
                                                      object:_movie];
        [[NSNotificationCenter defaultCenter] removeObserver:self 
                                                        name:QTMovieTimeDidChangeNotification 
                                                      object:_movie];
        [_movie release];
    }
    
    _movie = [movie retain];
    
    if (_movie) {
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_rateDidChange:) 
                                                     name:QTMovieRateDidChangeNotification 
                                                   object:_movie];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_timeDidChange:) 
                                                     name:QTMovieTimeDidChangeNotification 
                                                   object:_movie];
        [self setNeedsDisplay];
    }
    
}

# pragma mark Event handling

- (void)_setNewTimeForThumbCenterX:(CGFloat)centerX
{
    CGRect innerRect = [self _innerSliderRect];
    
    CGFloat fraction = (centerX - CGRectGetMinX(innerRect)) / CGRectGetWidth(innerRect);
    if (fraction > 1.0)
        fraction = 1.0;
    else if (fraction < 0.0)
        fraction = 0.0;
    
    NSTimeInterval newTime = fraction * [self _duration];
    
    [_movie setCurrentTime:QTMakeTimeWithTimeInterval(newTime)];
    [self setNeedsDisplay];    
}

- (void)handleMouseDown:(CGPoint)point
{
    if (!_movie)
        return;

    if (CGRectContainsPoint([self _sliderRect], point)) {
        _wasPlayingBeforeMouseDown = [self _isPlaying];
        _isScrubbing = YES;

        [_movie stop];
        if (CGRectContainsPoint([self _sliderThumbRect], point))
            _mouseDownXDelta = point.x - CGRectGetMidX([self _sliderThumbRect]);
        else {
            [self _setNewTimeForThumbCenterX:point.x];
            _mouseDownXDelta = 0;
        }
    }
}

- (void)handleMouseUp:(CGPoint)point
{
    if (!_movie)
        return;
    
    if (_isScrubbing) {
        _isScrubbing = NO;
        _mouseDownXDelta = 0;
        
        if (_wasPlayingBeforeMouseDown)
            [_movie play];
        return;
    }
    
    if (CGRectContainsPoint([self _playPauseButtonRect], point)) {
        if ([self _isPlaying])
            [_movie stop];
        else
            [_movie play];
        return;
    }
}

- (void)handleMouseDragged:(CGPoint)point
{
    if (!_movie)
        return;

    if (!_isScrubbing)
        return;

    point.x -= _mouseDownXDelta;

    [self _setNewTimeForThumbCenterX:point.x];
}

@end
