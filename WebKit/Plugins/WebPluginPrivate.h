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

/*
    Private extensions to the WebPlugin interface.  A plugin may implement these methods 
    to receive loading callbacks for its main resource.  Plug-ins that implement this SPI
    show better loading progress in the browser, can be saved to disk, and are
    more efficient by avoiding making duplicate GET or POST requests for the plug-in's main
    resource.
*/

@interface NSObject (WebPlugInPrivate)

/*!
    @method webPlugInMainResourceDidReceiveResponse:
    @abstract Called on the plug-in when WebKit receives -connection:didReceiveResponse:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidReceiveResponse:(NSURLResponse *)response;

/*!
    @method webPlugInMainResourceDidReceiveData:
    @abstract Called on the plug-in when WebKit recieves -connection:didReceiveData:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidReceiveData:(NSData *)data;

/*!
    @method webPlugInMainResourceDidFailWithError:
    @abstract Called on the plug-in when WebKit receives -connection:didFailWithError:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidFailWithError:(NSError *)error;

/*!
    @method webPlugInMainResourceDidFinishLoading
    @abstract Called on the plug-in when WebKit receives -connectionDidFinishLoading:
    for the plug-in's main resource.
    @discussion This method is only sent to the plug-in if the
    WebPlugInShouldLoadMainResourceKey argument passed to the plug-in was NO.
*/
- (void)webPlugInMainResourceDidFinishLoading;

@end
