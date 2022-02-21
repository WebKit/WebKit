/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "RevealItem.h"

#import "ArgumentCodersCocoa.h"
#import "WebCoreArgumentCoders.h"
#import <pal/cocoa/RevealSoftLink.h>

namespace WebKit {

#if ENABLE(REVEAL)

RevealItem::RevealItem(RetainPtr<RVItem>&& item)
    : m_item { WTFMove(item) }
{
}

void RevealItem::encode(IPC::Encoder& encoder) const
{
    encoder << m_item;
}

std::optional<RevealItem> RevealItem::decode(IPC::Decoder& decoder)
{
    static NeverDestroyed<RetainPtr<NSArray>> allowedClasses;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto allowed = adoptNS([[NSMutableArray alloc] initWithCapacity:1]);
        if (auto rvItemClass = PAL::getRVItemClass())
            [allowed addObject:rvItemClass];
        allowedClasses.get() = adoptNS([allowed copy]);
    });
    
    auto item = IPC::decode<RVItem>(decoder, allowedClasses.get().get());
    if (!item)
        return std::nullopt;

    return RevealItem { WTFMove(*item) };
}
#endif

}
