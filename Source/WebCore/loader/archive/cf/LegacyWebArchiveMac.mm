/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#import "LegacyWebArchive.h"

namespace WebCore {

static NSString * const LegacyWebArchiveResourceResponseKey = @"WebResourceResponse";

// FIXME: If is is possible to parse in a serialized NSURLResponse manually, without using
// NSKeyedUnarchiver, manipulating plists directly, we would prefer to do that instead.
ResourceResponse LegacyWebArchive::createResourceResponseFromMacArchivedData(CFDataRef responseData)
{    
    ASSERT(responseData);
    if (!responseData)
        return ResourceResponse();
    
    NSURLResponse *response = nil;
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:(__bridge NSData *)responseData error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    @try {
        response = [unarchiver decodeObjectOfClass:NSURLResponse.class forKey:LegacyWebArchiveResourceResponseKey];
        [unarchiver finishDecoding];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode NS(HTTP)URLResponse: %@", exception);
        response = nil;
    }

    return ResourceResponse(response);
}

RetainPtr<CFDataRef> LegacyWebArchive::createPropertyListRepresentation(const ResourceResponse& response)
{    
    NSURLResponse *nsResponse = response.nsURLResponse();
    ASSERT(nsResponse);
    if (!nsResponse)
        return nullptr;

    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    [archiver encodeObject:nsResponse forKey:LegacyWebArchiveResourceResponseKey];
    return (__bridge CFDataRef)archiver.get().encodedData;
}

}
