/*
 * Copyright (C) 2005, 2007, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCoreNSURLExtras.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/Function.h>
#import <wtf/HexNumber.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <unicode/uchar.h>
#import <unicode/uidna.h>
#import <unicode/uscript.h>

namespace WebCore {

NSURL *URLByCanonicalizingURL(NSURL *URL)
{
    RetainPtr<NSURLRequest> request = adoptNS([[NSURLRequest alloc] initWithURL:URL]);
    Class concreteClass = [NSURLProtocol _protocolClassForRequest:request.get()];
    if (!concreteClass) {
        return URL;
    }
    
    // This applies NSURL's concept of canonicalization, but not URL's concept. It would
    // make sense to apply both, but when we tried that it caused a performance degradation
    // (see 5315926). It might make sense to apply only the URL concept and not the NSURL
    // concept, but it's too risky to make that change for WebKit 3.0.
    NSURLRequest *newRequest = [concreteClass canonicalRequestForRequest:request.get()];
    NSURL *newURL = [newRequest URL]; 
    return [[newURL retain] autorelease];
}

} // namespace WebCore
