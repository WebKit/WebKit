/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
WebCore::ResourceError blockedByContentBlockerError(const WebCore::ResourceRequest&);
WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
WebCore::ResourceError wasBlockedByRestrictionsError(const WebCore::ResourceRequest&);
WebCore::ResourceError interruptedForPolicyChangeError(const WebCore::ResourceRequest&);
WebCore::ResourceError ftpDisabledError(const WebCore::ResourceRequest&);
WebCore::ResourceError failedCustomProtocolSyncLoad(const WebCore::ResourceRequest&);
#if ENABLE(CONTENT_FILTERING)
WebCore::ResourceError blockedByContentFilterError(const WebCore::ResourceRequest&);
#endif
WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
WebCore::ResourceError httpsUpgradeRedirectLoopError(const WebCore::ResourceRequest&);
WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);

#if USE(SOUP)
WebCore::ResourceError downloadNetworkError(const URL&, const WTF::String&);
WebCore::ResourceError downloadCancelledByUserError(const WebCore::ResourceResponse&);
WebCore::ResourceError downloadDestinationError(const WebCore::ResourceResponse&, const WTF::String&);
#endif

#if PLATFORM(GTK)
WebCore::ResourceError invalidPageRangeToPrint(const URL&);
#endif

} // namespace WebKit
