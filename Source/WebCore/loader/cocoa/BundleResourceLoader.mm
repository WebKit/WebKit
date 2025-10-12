/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "BundleResourceLoader.h"

#import "HTTPHeaderMap.h"
#import "MIMETypeRegistry.h"
#import "ResourceError.h"
#import "ResourceLoader.h"
#import "ResourceResponse.h"
#import "SharedBuffer.h"
#import <wtf/RunLoop.h>
#import <wtf/WorkQueue.h>

namespace WebCore {
namespace BundleResourceLoader {

static WorkQueue& loadQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("org.WebKit.BundleResourceLoader"_s, WorkQueue::QOS::Utility));
    return queue.get();
}

void loadResourceFromBundle(ResourceLoader& loader, const String& subdirectory)
{
    ASSERT(RunLoop::isMain());

    loadQueue().dispatch([protectedLoader = Ref { loader }, url = loader.request().url().isolatedCopy(), subdirectory = subdirectory.isolatedCopy()]() mutable {
        RetainPtr relativePath = [subdirectory.createNSString() stringByAppendingString: url.path().createNSString().get()];
        auto *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
        auto *path = [bundle pathForResource:relativePath.get() ofType:nil];
        auto *data = [NSData dataWithContentsOfFile:path];

        if (!data) {
            RunLoop::mainSingleton().dispatch([protectedLoader = WTFMove(protectedLoader), url = WTFMove(url).isolatedCopy()] {
                protectedLoader->didFail(ResourceError { errorDomainWebKitInternal, 0, url, "URL is invalid"_s });
            });
            return;
        }

        RunLoop::mainSingleton().dispatch([protectedLoader = WTFMove(protectedLoader), url = WTFMove(url).isolatedCopy(), buffer = SharedBuffer::create(data)]() mutable {
            auto mimeType = MIMETypeRegistry::mimeTypeForPath(url.path());
            ResourceResponse response { WTFMove(url), WTFMove(mimeType), static_cast<long long>(buffer->size()), MIMETypeRegistry::isTextMIMEType(mimeType) ? "UTF-8"_s : String() };
            response.setHTTPStatusCode(200);
            response.setHTTPStatusText("OK"_s);
            response.setSource(ResourceResponse::Source::Network);

            // Allow images to load.
            response.addHTTPHeaderField(HTTPHeaderName::AccessControlAllowOrigin, "*"_s);

            protectedLoader->deliverResponseAndData(WTFMove(response), WTFMove(buffer));
        });
    });
}
}
}
