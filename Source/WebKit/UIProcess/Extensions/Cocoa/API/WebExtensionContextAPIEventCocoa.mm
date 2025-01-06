/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WKWebExtensionControllerInternal.h"
#import "WKWebViewInternal.h"
#import "WebExtensionController.h"
#import "WebProcessProxy.h"
#import <wtf/EnumTraits.h>

namespace WebKit {

void WebExtensionContext::addListener(WebCore::FrameIdentifier frameIdentifier, WebExtensionEventListenerType listenerType, WebExtensionContentWorldType contentWorldType)
{
    RefPtr frame = WebFrameProxy::webFrame(frameIdentifier);
    if (!frame)
        return;

    RELEASE_LOG_DEBUG(Extensions, "Registered event listener for type %{public}hhu in %{public}@ world", enumToUnderlyingType(listenerType), (NSString *)toDebugString(contentWorldType));

    if (!protectedExtension()->backgroundContentIsPersistent() && isBackgroundPage(frameIdentifier))
        m_backgroundContentEventListeners.add(listenerType);

    auto result = m_eventListenerFrames.add({ listenerType, contentWorldType }, WeakFrameCountedSet { });
    result.iterator->value.add(*frame);
}

void WebExtensionContext::removeListener(WebCore::FrameIdentifier frameIdentifier, WebExtensionEventListenerType listenerType, WebExtensionContentWorldType contentWorldType, size_t removedCount)
{
    ASSERT(removedCount);

    RefPtr frame = WebFrameProxy::webFrame(frameIdentifier);
    if (!frame)
        return;

    RELEASE_LOG_DEBUG(Extensions, "Unregistered %{public}zu event listener(s) for type %{public}hhu in %{public}@ world", removedCount, enumToUnderlyingType(listenerType), (NSString *)toDebugString(contentWorldType));

    if (!protectedExtension()->backgroundContentIsPersistent() && isBackgroundPage(frameIdentifier)) {
        for (size_t i = 0; i < removedCount; ++i)
            m_backgroundContentEventListeners.remove(listenerType);
    }

    auto iterator = m_eventListenerFrames.find({ listenerType, contentWorldType });
    if (iterator == m_eventListenerFrames.end())
        return;

    for (size_t i = 0; i < removedCount; ++i)
        iterator->value.remove(*frame);

    if (!iterator->value.isEmptyIgnoringNullReferences())
        return;

    m_eventListenerFrames.remove(iterator);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
