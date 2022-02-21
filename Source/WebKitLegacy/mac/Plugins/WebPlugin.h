/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <WebKitLegacy/WebKitAvailability.h>

#if !TARGET_OS_IPHONE
#import <AppKit/AppKit.h>
#endif

/*!
    WebPlugIn is an informal protocol that enables interaction between an application
    and web related plug-ins it may contain.
*/

@interface NSObject (WebPlugIn)

/*!
    @method webPlugInInitialize
    @abstract Tell the plug-in to perform one-time initialization.
    @discussion This method must be only called once per instance of the plug-in
    object and must be called before any other methods in this protocol.
*/
- (void)webPlugInInitialize;

/*!
    @method webPlugInStart
    @abstract Tell the plug-in to start normal operation.
    @discussion The plug-in usually begins drawing, playing sounds and/or
    animation in this method.  This method must be called before calling webPlugInStop.
    This method may called more than once, provided that the application has
    already called webPlugInInitialize and that each call to webPlugInStart is followed
    by a call to webPlugInStop.
*/
- (void)webPlugInStart;

/*!
    @method webPlugInStop
    @abstract Tell the plug-in to stop normal operation.
    @discussion webPlugInStop must be called before this method.  This method may be
    called more than once, provided that the application has already called
    webPlugInInitialize and that each call to webPlugInStop is preceded by a call to
    webPlugInStart.
*/
- (void)webPlugInStop;

/*!
    @method webPlugInDestroy
    @abstract Tell the plug-in perform cleanup and prepare to be deallocated.
    @discussion The plug-in typically releases memory and other resources in this
    method.  If the plug-in has retained the WebPlugInContainer, it must release
    it in this mehthod.  This method must be only called once per instance of the
    plug-in object.  No other methods in this interface may be called after the
    application has called webPlugInDestroy.
*/
- (void)webPlugInDestroy;

#if !TARGET_OS_IPHONE
/*!
    @method webPlugInSetIsSelected:
    @discusssion Informs the plug-in whether or not it is selected.  This is typically
    used to allow the plug-in to alter it's appearance when selected.
*/
- (void)webPlugInSetIsSelected:(BOOL)isSelected;
#endif

/*!
    @property objectForWebScript
    @discussion objectForWebScript is used to expose a plug-in's scripting interface.  The 
    methods of the object are exposed to the script environment.  See the WebScripting
    informal protocol for more details.
    @result Returns the object that exposes the plug-in's interface.  The class of this
    object can implement methods from the WebScripting informal protocol.
*/
@property (nonatomic, readonly, strong) id objectForWebScript;

/*!
    @method webPlugInMainResourceDidReceiveResponse:
    @abstract Called on the plug-in when WebKit receives -connection:didReceiveResponse:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidReceiveResponse:(NSURLResponse *)response WEBKIT_AVAILABLE_MAC(10_6);

/*!
    @method webPlugInMainResourceDidReceiveData:
    @abstract Called on the plug-in when WebKit recieves -connection:didReceiveData:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidReceiveData:(NSData *)data WEBKIT_AVAILABLE_MAC(10_6);

/*!
    @method webPlugInMainResourceDidFailWithError:
    @abstract Called on the plug-in when WebKit receives -connection:didFailWithError:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidFailWithError:(NSError *)error WEBKIT_AVAILABLE_MAC(10_6);

/*!
    @method webPlugInMainResourceDidFinishLoading
    @abstract Called on the plug-in when WebKit receives -connectionDidFinishLoading:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidFinishLoading WEBKIT_AVAILABLE_MAC(10_6);

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
