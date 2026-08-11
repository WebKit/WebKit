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

#if USE(AVFOUNDATION) && HAVE(WK_SECURE_CODING_AVOUTPUTCONTEXT)

#import "config.h"
#import "CoreIPCAVOutputContext.h"

#import "Logging.h"
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebKit {

CoreIPCAVOutputContext::CoreIPCAVOutputContext(AVOutputContext *context)
{
    RetainPtr dict = [context _webKitPropertyListData];

    RetainPtr<NSString> contextID = [dict objectForKey:@"contextID"];
    if (![contextID isKindOfClass:NSString.class]) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCAVOutputContext 'contextID' value is nil or not an NSString");
        return;
    }

    RetainPtr<NSString> contextType = [dict objectForKey:@"contextType"];
    if (![contextType isKindOfClass:NSString.class]) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCAVOutputContext 'contextType' value is nil or not an NSString");
        return;
    }
    m_data = { (String)contextID.get(), (String)contextType.get() };
}

RetainPtr<id> CoreIPCAVOutputContext::toID() const
{
    RetainPtr dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:2]);
    [dict setObject:m_data.contextID.createNSString().get() forKey:@"contextID"];
    [dict setObject:m_data.contextType.createNSString().get() forKey:@"contextType"];
    return adoptNS([[PAL::getAVOutputContextClassSingleton() alloc] _initWithWebKitPropertyListData:dict.get()]);
}

} // namespace WebKit

#endif // USE(AVFOUNDATION) && HAVE(HAVE_WK_SECURE_CODING_AVOUTPUTCONTEXT)
