/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

@interface NSObject (WebPlugInPrivate)

#if TARGET_OS_IPHONE

// FIXME: Comment me
- (Class)webPlugInFullScreenWindowClass;

// FIXME: Comment me
- (void)webPlugInWillEnterFullScreenWithFrame:(CGRect)newFrame;

// FIXME: Comment me
- (void)webPlugInWillLeaveFullScreenWithFrame:(CGRect)newFrame;

// FIXME: Comment me
- (BOOL)webPlugInReceivesEventsDirectly;

// FIXME: Comment me
- (void)webPlugInLayout;

// FIXME: Comment me
- (void)webPlugInDidDraw;

/*!
 @method webPlugInStopForPageCache
 @abstract Tell the plug-in to stop normal operation because the page the plug-in
 belongs to is entering a cache.
 @discussion A page in the PageCache can be quickly resumed. This is much like
 pausing and resuming a plug-in except the frame containing the plug-in will
 not be visible, is not active, and may even have been torn down. The API contract
 for messages before and after this message are the same as -webPlugInStop.
 */
- (void)webPlugInStopForPageCache;

#endif

@end
