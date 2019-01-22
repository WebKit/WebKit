/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PreviewConverter.h"

#if USE(QUICK_LOOK)

#import "ResourceRequest.h"
#import "ResourceResponse.h"
#import <pal/ios/QuickLookSoftLink.h>
#import <pal/spi/ios/QuickLookSPI.h>

namespace WebCore {

static NSDictionary *optionsWithPassword(const String& password)
{
    if (password.isNull())
        return nil;

    return @{ (NSString *)PAL::get_QuickLook_kQLPreviewOptionPasswordKey() : password };
}

PreviewConverter::PreviewConverter(id delegate, const ResourceResponse& response, const String& password)
    : m_platformConverter { adoptNS([PAL::allocQLPreviewConverterInstance() initWithConnection:nil delegate:delegate response:response.nsURLResponse() options:optionsWithPassword(password)]) }
{
}

PreviewConverter::PreviewConverter(NSData *data, const String& uti, const String& password)
    : m_platformConverter { adoptNS([PAL::allocQLPreviewConverterInstance() initWithData:data name:nil uti:uti options:optionsWithPassword(password)]) }
{
}

ResourceRequest PreviewConverter::safeRequest(const ResourceRequest& request) const
{
    return [m_platformConverter safeRequestForRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)];
}

ResourceRequest PreviewConverter::previewRequest() const
{
    return [m_platformConverter previewRequest];
}

ResourceResponse PreviewConverter::previewResponse() const
{
    return [m_platformConverter previewResponse];
}

String PreviewConverter::previewFileName() const
{
    return [m_platformConverter previewFileName];
}

String PreviewConverter::previewUTI() const
{
    return [m_platformConverter previewUTI];
}

} // namespace WebCore

#endif // USE(QUICK_LOOK)
