/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "NetworkResourceLoader.h"

#include "ShareableResource.h"
#include "SharedMemory.h"
#include "WebResourceLoaderMessages.h"
#include <wtf/CurrentTime.h>

using namespace WebCore;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
typedef struct _CFURLCache* CFURLCacheRef;
typedef struct _CFCachedURLResponse* CFCachedURLResponseRef;
extern "C" CFURLCacheRef CFURLCacheCopySharedURLCache();
extern "C" CFCachedURLResponseRef CFURLCacheCopyResponseForRequest(CFURLCacheRef, CFURLRequestRef);
extern "C" CFArrayRef CFCachedURLResponseCopyReceiverDataArray(CFCachedURLResponseRef);
#endif

namespace WebKit {

void NetworkResourceLoader::platformDidReceiveResponse(const WebCore::ResourceResponse&)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 1090
    return;
#else

    // Check the cache to see if we can vm_copy a filesystem-backed resource buffer.   
    RetainPtr<CFURLCacheRef> cache = adoptCF(CFURLCacheCopySharedURLCache());
    if (!cache)
        return;

    CFURLRequestRef cfRequest = request().cfURLRequest(DoNotUpdateHTTPBody);
    RetainPtr<CFCachedURLResponseRef> cachedResponse = adoptCF(CFURLCacheCopyResponseForRequest(cache.get(), cfRequest));
    if (!cachedResponse)
        return;

    RetainPtr<CFArrayRef> array = adoptCF(CFCachedURLResponseCopyReceiverDataArray(cachedResponse.get()));
    if (!array || CFArrayGetCount(array.get()) != 1)
        return;
    
    CFTypeRef value = CFArrayGetValueAtIndex(array.get(), 0);
    if (CFGetTypeID(value) != CFDataGetTypeID())
        return;
    CFDataRef data = static_cast<CFDataRef>(value);

    // We only care about the vm_copy optimization for resources that should be file backed.
    // FIXME: If there were an API available to guarantee that a buffer was file backed, we should use it here.
    if (CFDataGetLength(data) < (CFIndex)fileBackedResourceMinimumSize())
        return;
    
    RefPtr<SharedMemory> sharedMemory = SharedMemory::createWithVMCopy((void*)CFDataGetBytePtr(data), CFDataGetLength(data));
    if (!sharedMemory) {
        LOG_ERROR("Failed to create VMCopied shared memory for cached resource.  We'll let it load normally.");
        return;
    }
 
    size_t size = sharedMemory->size();
    RefPtr<ShareableResource> resource = ShareableResource::create(sharedMemory.release(), 0, size);
    ShareableResource::Handle handle;
    if (!resource->createHandle(handle)) {
        LOG_ERROR("Failed to create ShareableResource handle to send to the WebProcess for resource.  We'll let it load normally.");
        return;
    }

    // Since we're delivering this resource by ourselves all at once, we'll abort the resource handle since we don't need anymore callbacks from CFNetwork.
    abortInProgressLoad();
    
    send(Messages::WebResourceLoader::DidReceiveResource(handle, currentTime()));
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED < 1090
}

size_t NetworkResourceLoader::fileBackedResourceMinimumSize()
{
    return SharedMemory::systemPageSize();
}

} // namespace WebKit
