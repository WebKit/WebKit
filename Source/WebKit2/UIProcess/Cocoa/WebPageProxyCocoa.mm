/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"

#import <wtf/cf/TypeCasts.h>

namespace WebKit {

static RetainPtr<CFStringRef> autosaveKey(const String& name)
{
    return String("com.apple.WebKit.searchField:" + name).createCFString();
}

void WebPageProxy::saveRecentSearches(const String& name, const Vector<String>& searchItems)
{
    if (!name) {
        // FIXME: This should be a message check.
        return;
    }

    RetainPtr<CFMutableArrayRef> items;

    if (!searchItems.isEmpty()) {
        items = adoptCF(CFArrayCreateMutable(0, searchItems.size(), &kCFTypeArrayCallBacks));

        for (const auto& searchItem : searchItems)
            CFArrayAppendValue(items.get(), searchItem.createCFString().get());
    }

    CFPreferencesSetAppValue(autosaveKey(name).get(), items.get(), kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

void WebPageProxy::loadRecentSearches(const String& name, Vector<String>& searchItems)
{
    if (!name) {
        // FIXME: This should be a message check.
        return;
    }

    auto items = adoptCF(dynamic_cf_cast<CFArrayRef>(CFPreferencesCopyAppValue(autosaveKey(name).get(), kCFPreferencesCurrentApplication)));
    if (!items)
        return;

    for (size_t i = 0, size = CFArrayGetCount(items.get()); i < size; ++i) {
        if (auto item = dynamic_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(items.get(), i)))
            searchItems.append(item);
    }
}

void WebPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilter& contentFilter, uint64_t frameID)
{
    if (WebFrameProxy* frame = m_process->webFrame(frameID))
        frame->setContentFilterForBlockedLoad(std::make_unique<WebCore::ContentFilter>(contentFilter));
}

}
